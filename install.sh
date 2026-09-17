#!/usr/bin/env bash
set -Eeuo pipefail

usage() {
    cat <<'HELP'
Usage: ./install.sh [all|kekule-v|videzzo|kekule-m|kekule-m-a|kekule-m-p|morphuzz ...]

Download LLVM 15.0.0 once and prepare selected variants (default: all).
Kekule-V and ViDeZZo stop after source extraction and repository overwrites.
Kekule-M and Morphuzz are built and installed in separate prefixes.
kekule-m-a builds the ablation variant; kekule-m-p builds path-level dependencies.
The default and all include both variants, providing clang-a and clang-p aliases.

Environment:
  LLVM_WORK_DIR      Downloads, sources, builds, links (default: <repo>/.llvm)
  LLVM_INSTALL_ROOT  Prefix parent (default: $LLVM_WORK_DIR/install)
  JOBS               Parallel build jobs (default: 2; LLVM needs much RAM)

Requires wget, tar, xz, and GNU utilities. Host builds also require CMake,
make, GCC/G++ 12, Python 3, and LLVM's build dependencies. The -a/-p variants
also require patch.
After installation, source the printed env.sh path in Bash.
HELP
}

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

if [[ ${1:-} == -h || ${1:-} == --help ]]; then
    usage
    exit 0
fi

artifact_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
work_dir=${LLVM_WORK_DIR:-$artifact_root/.llvm}
install_root=${LLVM_INSTALL_ROOT:-$work_dir/install}
jobs=${JOBS:-2}
[[ $jobs =~ ^[1-9][0-9]*$ ]] || die 'JOBS must be a positive integer'

targets=("$@")
if [[ ${#targets[@]} == 0 || (${#targets[@]} == 1 && ${targets[0]} == all) ]]; then
    targets=(kekule-v videzzo kekule-m kekule-m-a kekule-m-p morphuzz)
fi
host_build=false
needs_patch=false
for target in "${targets[@]}"; do
    case $target in
        kekule-v|videzzo) ;;
        kekule-m|morphuzz) host_build=true ;;
        kekule-m-a|kekule-m-p) host_build=true; needs_patch=true ;;
        *) die "unknown target: $target (see --help)" ;;
    esac
done

commands=(wget tar xz cp mkdir mv ln realpath touch)
if $host_build; then commands+=(cmake make gcc-12 g++-12 python3); fi
if $needs_patch; then commands+=(patch); fi
for command in "${commands[@]}"; do
    command -v "$command" >/dev/null 2>&1 || die "required command not found: $command"
done

mkdir -p -- "$work_dir"
work_dir=$(realpath -- "$work_dir")
install_root=$(realpath -m -- "$install_root")
mkdir -p -- "$work_dir/downloads" "$work_dir/src" "$work_dir/bin"
archive="$work_dir/downloads/llvm-project-15.0.0.src.tar.xz"
url=https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.0/llvm-project-15.0.0.src.tar.xz
if [[ ! -f $archive ]]; then
    wget -c -O "$archive.part" "$url"
    # Only promote complete downloads into the reusable cache.
    xz -t -- "$archive.part"
    mv -- "$archive.part" "$archive"
fi

prepare_source() {
    local variant=$1
    local overlay=$variant
    case $variant in kekule-m-a|kekule-m-p) overlay=kekule-m ;; esac
    local source="$work_dir/src/$variant"
    if [[ ! -f $source/.artifact-extracted ]]; then
        # Keep partial extraction separate so an interrupted run can be retried.
        [[ ! -e $source ]] || die "unmanaged source directory exists: $source"
        mkdir -p -- "$source.extracting"
        tar -xJf "$archive" --strip-components=1 -C "$source.extracting"
        [[ -f $source.extracting/llvm/CMakeLists.txt ]] || die 'invalid LLVM source archive'
        touch "$source.extracting/.artifact-extracted"
        mv -- "$source.extracting" "$source"
    fi
    if [[ $variant != morphuzz ]]; then
        # Refresh the overlay before patching so repeated runs start cleanly.
        cp -R -- "$artifact_root/llvm-project/$overlay/." "$source/"
    fi
    case $variant in
        kekule-m-a)
            patch --batch --forward --no-backup-if-mismatch \
                "$source/compiler-rt/lib/fuzzer/FuzzerLoop.cpp" \
                "$artifact_root/llvm-project/kekule-m/compiler-rt/lib/fuzzer/no-priority.patch"
            ;;
        kekule-m-p)
            local file
            for file in FuzzerLoop.cpp FuzzerTracePC.h; do
                patch --batch --forward --no-backup-if-mismatch \
                    "$source/compiler-rt/lib/fuzzer/$file" \
                    "$artifact_root/llvm-project/kekule-m/compiler-rt/lib/fuzzer/$file.patch"
            done
            ;;
    esac
    printf 'Prepared %s sources: %s\n' "$variant" "$source"
}

link_binary() {
    local binary=$1 name=$2
    [[ -x $binary ]] || die "installed binary missing: $binary"
    local link="$work_dir/bin/$name"
    [[ ! -e $link || -L $link ]] || die "refusing to replace a non-symlink: $link"
    ln -sfnT -- "$binary" "$link"
}

for target in "${targets[@]}"; do
    prepare_source "$target"
    case $target in
        kekule-v|videzzo) continue ;;
    esac

    prefix="$install_root/$target"
    build="$work_dir/build/$target"
    cmake -S "$work_dir/src/$target/llvm" -B "$build" -G 'Unix Makefiles' \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=gcc-12 \
        -DCMAKE_CXX_COMPILER=g++-12 \
        -DCMAKE_CXX_FLAGS=-fconcepts \
        '-DLLVM_ENABLE_PROJECTS=clang;compiler-rt' \
        "-DCMAKE_INSTALL_PREFIX=$prefix"
    cmake --build "$build" --parallel "$jobs"
    cmake --install "$build"

    suffix=''
    case $target in
        kekule-m) suffix=-n ;;
        kekule-m-a) suffix=-a ;;
        kekule-m-p) suffix=-p ;;
    esac
    link_binary "$prefix/bin/clang" "clang$suffix"
    link_binary "$prefix/bin/clang++" "clang++$suffix"
done

# Keep aliases first and make the remaining LLVM tools available afterwards.
# %q allows spaces in paths when sourcing this file in Bash.
printf 'export PATH=%q:%q:%q:%q:%q:"$PATH"\n' \
    "$work_dir/bin" "$install_root/morphuzz/bin" "$install_root/kekule-m/bin" \
    "$install_root/kekule-m-a/bin" "$install_root/kekule-m-p/bin" \
    > "$work_dir/env.sh"
printf '\nTo use the installed compilers and LLVM tools, run:\n  source %q\n' "$work_dir/env.sh"
printf '\nDocker LLVM source paths (for the variants you prepared):\n'
printf '  Kekule-V (-n): %s/src/kekule-v\n  ViDeZZo (-v):  %s/src/videzzo\n' "$work_dir" "$work_dir"
