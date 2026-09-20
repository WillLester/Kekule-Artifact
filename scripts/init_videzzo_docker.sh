#!/usr/bin/env bash

set -Eeuo pipefail

usage() {
    cat <<'EOF'
Usage: init_videzzo_docker.sh <-v|-n> <bug-tag> <workspace> <llvm-source> [artifact-root]

  -v             Build the vanilla configuration.
  -n             Build the instrumented configuration.
  bug-tag        Bug configuration to prepare, for example esp1 or ati2.
  workspace      New directory in which ViDeZZo will be cloned and prepared.
  llvm-source    Path to LLVM 15.0.0 source code. The code should match -v|-n.
  artifact-root  Root of this artifact (default: parent of this scripts directory).

Environment:
  VIDEZZO_DOCKER_IMAGE  Docker image to run (default: videzzo:latest).
EOF
}

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

require_file() {
    [[ -f "$1" ]] || die "required file not found: $1"
}

require_dir() {
    [[ -d "$1" ]] || die "required directory not found: $1"
}

copy_file() {
    local source=$1
    local destination=$2
    require_file "$source"
    cp -- "$source" "$destination"
}

copy_tree() {
    local source=$1
    local destination=$2
    require_dir "$source"
    cp -R -- "$source" "$destination"
}

apply_patch_file() {
    local target=$1
    local patch_file=$2
    require_file "$target"
    require_file "$patch_file"
    patch "$target" < "$patch_file"
}

[[ $# -ge 4 && $# -le 5 ]] || {
    usage >&2
    exit 2
}

mode=$1
bug_tag=$2
workspace=$3
llvm_source=$4

case "$mode" in
    -v|-n) ;;
    *) die "mode must be -v (vanilla) or -n (instrumented)" ;;
esac

for command in git patch cp mkdir realpath docker; do
    command -v "$command" >/dev/null 2>&1 || die "required command not found: $command"
done

[[ "$bug_tag" =~ ^[A-Za-z0-9._-]+$ ]] || die "invalid bug tag: $bug_tag"
require_dir "$llvm_source"
llvm_source=$(realpath -- "$llvm_source")

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
if [[ -d "$script_dir/videzzo" && -d "$script_dir/qemu" ]]; then
    default_artifact_root=$script_dir
else
    default_artifact_root=$(cd -- "$script_dir/.." && pwd)
fi
artifact_root=${5:-$default_artifact_root}
artifact_root=$(realpath -- "$artifact_root")

# Repository-owned inputs. Keeping these paths relative to artifact_root removes
# all dependencies on a particular username, home directory, or checkout path.
assets_dir="$artifact_root/videzzo"
qemu_dir="$artifact_root/qemu"
llvm_patch_dir="$artifact_root/llvm-project/kekule-v"
instrumented_dir="$artifact_root/instrumented"
artifact_scripts_dir="$artifact_root/scripts"
dependencies_dir="$artifact_root/dependencies"

require_dir "$assets_dir"
require_dir "$qemu_dir"
require_dir "$instrumented_dir"

[[ ! -e "$workspace" ]] || die "workspace already exists: $workspace"
mkdir -p -- "$(dirname -- "$workspace")"

git clone https://github.com/HexHive/ViDeZZo.git "$workspace"
workspace=$(realpath -- "$workspace")
git -C "$workspace" checkout --detach d698dde482a124863

if [[ "$mode" == -v ]]; then
    copy_tree "$llvm_source" "$workspace/llvm-project-15.0.0.src"
else
    copy_tree "$llvm_source" "$workspace/llvm-project-15.0.0-mod.src"
fi

apply_patch_file "$workspace/Dockerfile" "$assets_dir/Dockerfile.patch"
copy_file "$assets_dir/Init.sh" "$workspace/"
copy_file "$assets_dir/Run.sh" "$workspace/"
copy_file "$artifact_scripts_dir/gen_cov_data.py" "$workspace/"

mkdir -p -- \
    "$workspace/patches/include/qemu" \
    "$workspace/patches/util" \
    "$workspace/patches/tests/qtest"

copy_file "$qemu_dir/include/qemu/cutils.h.patch" "$workspace/patches/include/qemu/"
copy_file "$qemu_dir/util/cutils.c.patch" "$workspace/patches/util/"
copy_file "$assets_dir/videzzo_qemu/libqtest-ok.patch" "$workspace/patches/tests/qtest/"

apply_patch_file "$workspace/videzzo_fork.c" "$assets_dir/videzzo_fork.c.patch"
apply_patch_file "$workspace/videzzo_qemu/Makefile" "$assets_dir/videzzo_qemu/Makefile.patch"
apply_patch_file "$workspace/videzzo_qemu/0001-Update-QEMU-to-support-ViDeZZo-as-a-library.patch" \
    "$assets_dir/videzzo_qemu/0001-Update-QEMU-to-support-ViDeZZo-as-a-library.patch.patch"
apply_patch_file "$workspace/videzzo_qemu/0002-copy-to-qemu.sh" \
    "$assets_dir/videzzo_qemu/0002-copy-to-qemu.sh.patch"
apply_patch_file "$workspace/videzzo_qemu/0003-compile-qemu-san.sh" \
    "$assets_dir/videzzo_qemu/0003-compile-qemu-san.sh.patch"
apply_patch_file "$workspace/videzzo_qemu/0004-zip-qemu-targets.sh" \
    "$assets_dir/videzzo_qemu/0004-zip-qemu-targets.sh.patch"
apply_patch_file "$workspace/videzzo_qemu/0005-compile-qemu-cov.sh" \
    "$assets_dir/videzzo_qemu/0005-compile-qemu-cov.sh.patch"
apply_patch_file "$workspace/videzzo_qemu/0006-compile-qemu-deb.sh" \
    "$assets_dir/videzzo_qemu/0006-compile-qemu-deb.sh.patch"
apply_patch_file "$workspace/videzzo_qemu/videzzo_qemu.c" \
    "$assets_dir/videzzo_qemu/videzzo_qemu.patch"
apply_patch_file "$workspace/videzzo_tool/poc-gen.c" \
    "$assets_dir/videzzo_tool/poc-gen.c.patch"

for file in \
    videzzo_qemu/videzzo_qemu-sysemu.patch \
    videzzo_qemu/videzzo_qemu-mainstone.patch \
    videzzo_qemu/videzzo_qemu-exec.patch \
    dep.patch \
    dep-base.patch \
    no-priority.patch; do
    copy_file "$assets_dir/$file" "$workspace/"
done

copy_file "$llvm_patch_dir/compiler-rt/lib/fuzzer/FuzzerLoop.cpp.patch" "$workspace/"
copy_file "$llvm_patch_dir/compiler-rt/lib/fuzzer/FuzzerTracePC.h.patch" "$workspace/"

case "$bug_tag" in
    ati2)
        copy_file "$instrumented_dir/hw/display/ati_2d.c.patch" "$workspace/videzzo_qemu/"
        ;;
    cirrus-vga-cov|cirrus-vga-dep)
        copy_file "$instrumented_dir/hw/display/cirrus-vga-cov.patch" "$workspace/videzzo_qemu/"
        ;;
    cirrus-vga-upstream)
        copy_file "$instrumented_dir/hw/display/cirrus-vga-cov.patch" "$workspace/videzzo_qemu/"
        copy_file "$instrumented_dir/hw/display/cirrus_vga_rop2.h.patch" "$workspace/videzzo_qemu/"
        ;;
    esp1|esp-cov)
        copy_file "$instrumented_dir/hw/scsi/esp-cov.patch" "$workspace/videzzo_qemu/"
        ;;
    esp1-fpe)
        copy_file "$instrumented_dir/hw/scsi/scsi-disk.c.patch" "$workspace/videzzo_qemu/"
        copy_file "$instrumented_dir/hw/scsi/esp.c.patch" "$workspace/videzzo_qemu/"
        ;;
    ohci-upstream)
        copy_file "$instrumented_dir/hw/usb/core.c.patch" "$workspace/videzzo_qemu/"
        ;;
    sdhci-upstream)
        copy_file "$instrumented_dir/hw/sd/sdhci.c.patch" "$workspace/videzzo_qemu/"
        ;;
    smc91c111-2)
        copy_file "$instrumented_dir/hw/net/smc91c111.c.patch" "$workspace/videzzo_qemu/"
        ;;
    smc91c111-3)
        copy_file "$instrumented_dir/hw/net/smc91c111-2.c.patch" "$workspace/videzzo_qemu/"
        ;;
    smc91c111-4)
        copy_file "$instrumented_dir/hw/net/smc91c111-3.c.patch" "$workspace/videzzo_qemu/"
        ;;
    smc91c111-upstream)
        copy_file "$instrumented_dir/hw/net/smc91c111-4.c.patch" "$workspace/videzzo_qemu/"
        ;;
esac

if [[ "$mode" == -n ]]; then
    require_dir "$dependencies_dir"
    copy_tree "$dependencies_dir/meson-1.2.3" "$workspace/"
    copy_tree "$dependencies_dir/meson-1.5.0" "$workspace/"

    mkdir -p -- "$workspace/pass"
    copy_file "$artifact_root/CMakeLists.txt" "$workspace/pass/"
    copy_tree "$artifact_root/pass" "$workspace/pass/"
    copy_file "$artifact_scripts_dir/get_device_module.sh" "$workspace/pass/"
    copy_file "$artifact_scripts_dir/instrument.sh" "$workspace/pass/"
    copy_file "$artifact_scripts_dir/generate_rsp.py" "$workspace/"
    copy_file "$artifact_scripts_dir/generate_link_sh.py" "$workspace/"
    copy_file "$qemu_dir/configure-us.patch" "$workspace/"
    copy_file "$qemu_dir/meson_options-vd.patch" "$workspace/"

    case "$bug_tag" in
        ati2|upstream) ;;
        esp1)
            copy_file "$instrumented_dir/rsp/qemu-videzzo-x86_64-am53c974.sh" "$workspace/"
            copy_file "$qemu_dir/python/scripts/vendor.py.patch" "$workspace/videzzo_qemu/"
            ;;
        esp3)
            copy_file "$instrumented_dir/hw/scsi/esp3.patch" "$workspace/videzzo_qemu/"
            ;;
        *) printf 'Using default upstream configuration for %s\n' "$bug_tag" ;;
    esac
else
    case "$bug_tag" in
        upstream|esp1|nvme) ;;
        esp2)
            copy_file "$instrumented_dir/hw/scsi/esp-vanilla-2.patch" "$workspace/videzzo_qemu/"
            ;;
        esp3)
            copy_file "$instrumented_dir/hw/scsi/esp3.patch" "$workspace/videzzo_qemu/"
            ;;
        *) die "unsupported vanilla bug tag: $bug_tag" ;;
    esac
fi

docker_image=${VIDEZZO_DOCKER_IMAGE:-videzzo:latest}
docker_command=(docker)
if ! docker info >/dev/null 2>&1; then
    command -v sudo >/dev/null 2>&1 || die "cannot access Docker and sudo is unavailable"
    docker_command=(sudo docker)
fi

"${docker_command[@]}" run -d -it \
    --mount "type=bind,src=$workspace,dst=/root/videzzo" \
    "$docker_image" /bin/bash

