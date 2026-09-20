#!/bin/bash
set -x
vanilla="$1"
arch="$2"
version="$3"
mode="$4"
workdir="$5"
extra_option="$6"


if [ "$#" -lt 5 ]; then
	echo "Usage: vanilla (-v or -n), arch, version (which bug or device), mode (san, cov or dep), workdir, optional: extra option (-p path feedback, -a no priority)."
	exit 1
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd) || exit 1
artifact_root=$(cd -- "$script_dir/.." && pwd) || exit 1

mkdir -p -- "$workdir" || exit 1
workdir=$(cd -- "$workdir" && pwd) || exit 1
cd -- "$workdir" || exit 1

cp "$artifact_root"/scripts/prepare.patch "$workdir"
cp "$artifact_root"/scripts/less_crashes.patch "$workdir"
cp "$artifact_root"/scripts/clangcovdump.h "$workdir"
cp "$artifact_root"/scripts/qfuzz_run.sh "$workdir"
cp "$artifact_root"/scripts/gen_cov_data.py "$workdir"

case "$version" in
	"ati2")
		cp "$artifact_root"/instrumented/hw/display/ati_2d.c.patch "$workdir"
		;;
	"cirrus-vga-upstream")
		cp "$artifact_root"/instrumented/hw/display/cirrus-vga-cov.patch "$workdir"
		cp "$artifact_root"/instrumented/hw/display/cirrus_vga_rop2.h.patch "$workdir"
		;;
	"ehci-upstream"|"ohci")
		cp "$artifact_root"/instrumented/hw/usb/core-assertion.c.patch "$workdir"
		;;
	"esp1")
		cp "$artifact_root"/instrumented/hw/scsi/scsi-disk.c.patch "$workdir"
		;;
	"esp1-fpe")
		cp "$artifact_root"/instrumented/hw/scsi/scsi-disk.c.patch "$workdir"
		cp "$artifact_root"/instrumented/hw/scsi/esp.c.patch "$workdir"
		;;
	"esp-upstream")
		cp "$artifact_root"/instrumented/hw/scsi/esp-cov.patch "$workdir"
		;;
	"lan9118-upstream")
		cp "$artifact_root"/instrumented/hw/net/lan9118.c.patch "$workdir"
		;;
	"nvme")
		cp "$artifact_root"/instrumented/hw/scsi/scsi-disk.c.patch "$workdir"
		;;
	"pcnet-upstream")
		cp "$artifact_root"/instrumented/hw/net/pcnet.c.patch "$workdir"
		;;
	"sdhci")
		cp "$artifact_root"/instrumented/hw/scsi/scsi-disk.c.patch "$workdir"
		;;
	"smc91c111-2")
		cp "$artifact_root"/instrumented/hw/net/smc91c111.c.patch "$workdir"
		;;
	"smc91c111-3")
		cp "$artifact_root"/instrumented/hw/net/smc91c111-2.c.patch "$workdir"
		;;
	"smc91c111-4")
		cp "$artifact_root"/instrumented/hw/net/smc91c111-3.c.patch "$workdir"
		;;
	"smc91c111-upstream")
		cp "$artifact_root"/instrumented/hw/net/smc91c111-4.c.patch "$workdir"
		;;
	"uhci-upstream")
		cp "$artifact_root"/instrumented/hw/usb/core-assertion.c.patch "$workdir"
		;;
	*)
		;;
esac

if [ ! -d 'qemu' ]; then
    git clone https://gitlab.com/qemu-project/qemu
fi

cd qemu

mkdir build
cp ../clangcovdump.h tests/qtest/fuzz/

less_crashes=""
fuzzer="fuzz"
platform="QEMU"

case "$version" in
	"upstream")
		;;
	"ati1")
		git checkout c9ba79ba
		;;
	"ati2")
		git checkout 8442e162
		patch hw/display/ati_2d.c < ../ati_2d.c.patch
		;;
	"cirrus-vga")
		git checkout 248f6f62
		;;
	"esp1")
		git checkout c167c80b
		patch hw/scsi/scsi-disk.c < ../scsi-disk.c.patch
		less_crashes="--extra-cflags=-DLESS_CRASHES"
		;;
	"esp1-fpe")
		git checkout c167c80b
		patch hw/scsi/scsi-disk.c < ../scsi-disk.c.patch
		patch hw/scsi/esp.c < ../esp.c.patch
		less_crashes="--extra-cflags=-DLESS_CRASHES"
		;;
	"esp3")
		git checkout da96ad4a
		;;
	"esp3-buf")
		git checkout da96ad4a
		less_crashes="--extra-cflags=-DLESS_CRASHES"
		;;
	"esp5")
		git checkout c80a3395
		;;
	"esp5-buf")
		git checkout c80a3395
		less_crashes="--extra-cflags=-DLESS_CRASHES"
		;;
	"ohci"|"lan9118-2")
		git checkout 5134cf9b
		;;
	"lsi53c895a"|"smc91c111"|"lan9118")
		git checkout fea445e8
		;;
	"nvme")
		git checkout c167c80b
		patch hw/scsi/scsi-disk.c < ../scsi-disk.c.patch
		less_crashes="--extra-cflags=-DLESS_CRASHES"
		;;
	"nvme-2")
		git checkout 5134cf9b
		;;
	"sdhci")
		git checkout c167c80b
		patch hw/scsi/scsi-disk.c < ../scsi-disk.c.patch
		;;
	"smc91c111-2")
		git checkout 7c89e226
		patch hw/net/smc91c111.c < ../smc91c111.c.patch
		;;
	"smc91c111-3")
		git checkout 7c89e226
		patch hw/net/smc91c111.c < ../smc91c111-2.c.patch
		;;
	"smc91c111-4")
		git checkout 7c89e226
		patch hw/net/smc91c111.c < ../smc91c111-3.c.patch
		;;
	"std-vga")
		git checkout ae35f033
		;;
	*)
		echo "Go to upstream"
		;;
esac

if [[ "$mode" == "cov" ]] || [[ "$mode" == "dep" ]]
then
	git checkout c80a3395
	case "$version" in
		"esp-upstream")
			patch hw/scsi/scsi-disk.c < ../esp-cov.patch
			;;
		"cirrus-vga-upstream")
			patch system/physmem.c < ../cirrus-vga-cov.patch
			patch hw/display/cirrus_vga_rop2.h < ../cirrus_vga_rop2.h.patch
			;;
		"lan9118-upstream")
			patch hw/net/lan9118.c < ../lan9118.c.patch
			;;
		"pcnet-upstream")
			patch hw/net/pcnet.c < ../pcnet.c.patch
			;;
		"smc91c111-upstream")
			patch hw/net/smc91c111.c < ../smc91c111-4.c.patch
			;;
		*)
			;;
	esac
else
	case "$version" in
		"cirrus-vga-upstream")
			patch system/physmem.c < ../cirrus-vga-cov.patch
			patch hw/display/cirrus_vga_rop2.h < ../cirrus_vga_rop2.h.patch
			;;
		"ehci-upstream"|"ohci")
			patch hw/usb/core.c < ../core-assertion.c.patch
			;;
		"pcnet-upstream")
			patch hw/net/pcnet.c < ../pcnet.c.patch
			;;
		"smc91c111-upstream")
			git checkout 7c89e226
			patch hw/net/smc91c111.c < ../smc91c111-4.c.patch
			;;
		"uhci-upstream")
			patch hw/usb/core.c < ../core-assertion.c.patch
			;;
		*)
			;;
	esac
fi

TARGET_COMMIT="5915139a"
SANITIZER_COMMIT="cb771ac1f"
MESON_COMMIT="07f0d3264"

CURRENT_COMMIT=$(git rev-parse HEAD)

if git merge-base --is-ancestor "$TARGET_COMMIT" "$CURRENT_COMMIT"
then
	echo "The current commit is at or after the target commit [$TARGET_COMMIT]"
	libcommon="libcommon.a.p"
else
	echo "The current commit is before the target commit [$TARGET_COMMIT]"
	libcommon="libcommon.fa.p"
fi

if git merge-base --is-ancestor "$SANITIZER_COMMIT" "$CURRENT_COMMIT"
then
	echo "The current commit is at or after the target commit [$SANITIZER_COMMIT]"
	sanitizer_option="--enable-ubsan --enable-asan"
	dis_sanitizer_option="--disable-ubsan --disable-asan"
else
	echo "The current commit is before the target commit [$SANITIZER_COMMIT]"
	sanitizer_option="--enable-sanitizers"
	dis_sanitizer_option="--disable-sanitizers"
fi

if git merge-base --is-ancestor "$MESON_COMMIT" "$CURRENT_COMMIT"
then
	echo "The current commit is at or after the target commit [$MESON_COMMIT]"
	meson_version="1.5.0"
else
	echo "The current commit is before the target commit [$MESON_COMMIT]"
	meson_version="1.2.3"
fi

git apply ../prepare.patch
patch include/qemu/osdep.h < ../less_crashes.patch

cd build


link_to_binary() {
	local arch="$1"
	local core_file="$2"
	local fuzzer="$3"
	if [[ "$meson_version" == "1.2.3" ]] && [[ "$version" != "esp1" && "$version" != "esp1-fpe" && "$version" != "sdhci" && "$version" != "nvme" && "$version" != "e1000" ]] 
	then
		ninja -d keeprsp -n qemu-fuzz-"$arch"
		python3 generate_rsp.py "$arch" "$fuzzer" "$core_file"
		$compiler -m64 -mcx16 @qemu-fuzz-new-"$arch".rsp
	else
		ninja -v -n qemu-fuzz-"$arch" &> link.tmp
		python3 generate_link_sh.py link.tmp link.sh "$core_file"
		rm link.tmp
		chmod +x link.sh
		./link.sh
	fi
}

compiler="clang"
compilerx="clang++"

if [[ "$vanilla" == "-v" ]]
then
	if [[ "$mode" == "san" ]]
	then
		CC=$compiler CXX=$compilerx ../configure --enable-fuzzing $sanitizer_option $less_crashes --disable-werror --enable-spice --enable-slirp --target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
	else
		CC=$compiler CXX=$compilerx ../configure --enable-fuzzing $dis_sanitizer_option --disable-werror --enable-spice --enable-slirp --extra-cflags="-DCLANG_COV_DUMP -DLESS_CRASHES -fprofile-instr-generate -fcoverage-mapping" --target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
	fi
	make -j16 qemu-fuzz-"$arch"
else
	compiler="clang-n"
	compilerx="clang++-n"
	if [[ "$extra_option" == "-p" ]]
	then
		compiler="clang-p"
		compilerx="clang++-p"
	elif [[ "$extra_option" == "-a" ]]
	then
		compiler="clang-a"
		compilerx="clang++-a"
	fi
	cp "$artifact_root"/dependencies/meson-"$meson_version"/meson-"$meson_version"-py3-none-any.whl ../python/wheels/
	cd ..
	cp "$artifact_root"/build/pass/Kekule.so ./
	cp "$artifact_root"/scripts/get_device_module.sh ./
	cp "$artifact_root"/scripts/instrument.sh ./
	patch configure < "$artifact_root"/qemu/configure-us.patch
	patch meson_options.txt < "$artifact_root"/qemu/meson_options-us.patch
	cd build
	CC=$compiler CXX=$compilerx ../configure --enable-fuzzing $sanitizer_option --disable-werror --enable-spice --enable-slirp --target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
	make update-buildoptions
	cp "$artifact_root"/scripts/generate_rsp.py ./
	cp "$artifact_root"/scripts/generate_link_sh.py ./
	if [[ "$mode" == "san" ]]
	then
		if [[ "$version" == "esp1" || "$version" == "esp1-fpe" || "$version" == "sdhci" || "$version" == "nvme" || "$version" == "e1000" ]]
		then
			CC=$compiler CXX=$compilerx ../configure \
				--enable-fuzzing \
				--disable-werror $sanitizer_option --enable-spice \
				--enable-slirp --enable-llvm --disable-download $less_crashes \
				--target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		else
			CC=$compiler CXX=$compilerx ../configure \
				--enable-fuzzing \
				--disable-werror $sanitizer_option --enable-spice \
				--enable-slirp --enable-llvm $less_crashes \
				--target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		fi
	elif [[ "$mode" == "dep" ]]
	then
		if [[ "$version" == "esp1" || "$version" == "esp1-fpe" || "$version" == "sdhci" || "$version" == "nvme" || "$version" == "e1000" ]]
		then
			CC=$compiler CXX=$compilerx ../configure \
				--enable-fuzzing \
				--disable-werror $sanitizer_option --enable-spice \
				--enable-slirp --enable-llvm --disable-download \
				--extra-cflags="-DLESS_CRASHES" \
				--target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		else
			CC=$compiler CXX=$compilerx ../configure \
				--enable-fuzzing \
				--disable-werror $sanitizer_option --enable-spice \
				--enable-slirp --enable-llvm \
				--extra-cflags="-DLESS_CRASHES" \
				--target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		fi
	elif [[ "$mode" == "cov" ]]
	then
		if [[ "$version" == "esp1" || "$version" == "esp1-fpe" || "$version" == "sdhci" || "$version" == "nvme" || "$version" == "e1000" ]]
		then
			CLANG_COV_DUMP=1 \
			CC=$compiler CXX=$compilerx ../configure \
			    --enable-fuzzing \
			    --disable-werror $dis_sanitizer_option --enable-spice \
			    --enable-slirp --enable-llvm --disable-download \
			    --extra-cflags="-DCLANG_COV_DUMP -DLESS_CRASHES -fprofile-instr-generate -fcoverage-mapping" \
			    --target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		else
			CLANG_COV_DUMP=1 \
			CC=$compiler CXX=$compilerx ../configure \
			    --enable-fuzzing \
			    --disable-werror $dis_sanitizer_option --enable-spice \
			    --enable-slirp --enable-llvm \
			    --extra-cflags="-DCLANG_COV_DUMP -DLESS_CRASHES -fprofile-instr-generate -fcoverage-mapping" \
			    --target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		fi
	fi
	case "$version" in
		"upstream")
			;;
		"ac97-upstream")
			make "$libcommon"/hw_audio_ac97.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build \
				./"$libcommon"/hw_audio_ac97.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_audio_ac97.c.ll \
				"$workdir"/ac97-depend.txt "$libcommon"/instrumented_hw_audio_ac97.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_audio_ac97.c.o -c "$libcommon"/instrumented_hw_audio_ac97.c.ll
			cp "$libcommon"/hw_audio_ac97.c.o ../
			;;
		"acpi-upstream")
			make "$libcommon"/hw_acpi_erst.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build \
				./"$libcommon"/hw_acpi_erst.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_acpi_erst.c.ll \
				"$workdir"/acpi-erst-depend.txt "$libcommon"/instrumented_hw_acpi_erst.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_acpi_erst.c.o -c "$libcommon"/instrumented_hw_acpi_erst.c.ll
			cp "$libcommon"/hw_acpi_erst.c.o ../
			;;
		"ahci-hd-upstream")
			make "$libcommon"/hw_ide_ahci.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build \
				./"$libcommon"/hw_ide_ahci.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_ide_ahci.c.ll \
				"$workdir"/ahci-hd-depend.txt "$libcommon"/instrumented_hw_ide_ahci.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_ide_ahci.c.o -c "$libcommon"/instrumented_hw_ide_ahci.c.ll
			cp "$libcommon"/hw_ide_ahci.c.o ../
			;;
		"ati1"|"ati2"|"ati-upstream")
			make "$libcommon"/hw_display_ati.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build \
				./"$libcommon"/hw_display_ati.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_display_ati.c.ll \
				"$workdir"/ati-depend.txt "$libcommon"/instrumented_hw_display_ati.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_display_ati.c.o -c "$libcommon"/instrumented_hw_display_ati.c.ll
			cp "$libcommon"/hw_display_ati.c.o ../
			;;
		"cirrus-vga"|"cirrus-vga-upstream")
			make "$libcommon"/hw_display_cirrus_vga.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build \
				./"$libcommon"/hw_display_cirrus_vga.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_display_cirrus_vga.c.ll \
				"$workdir"/cirrus-vga-depend.txt "$libcommon"/instrumented_hw_display_cirrus_vga.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_display_cirrus_vga.c.o -c "$libcommon"/instrumented_hw_display_cirrus_vga.c.ll
			cp "$libcommon"/hw_display_cirrus_vga.c.o ../
			;;
		"cs4231-upstream")
			make "$libcommon"/hw_audio_cs4231.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build \
				./"$libcommon"/hw_audio_cs4231.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_audio_cs4231.c.ll \
				"$workdir"/cs4231-depend.txt "$libcommon"/instrumented_hw_audio_cs4231.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_audio_cs4231.c.o -c "$libcommon"/instrumented_hw_audio_cs4231.c.ll
			cp "$libcommon"/hw_audio_cs4231.c.o ../
			;;
		"cs4231a-upstream")
			make "$libcommon"/hw_audio_cs4231a.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build \
				./"$libcommon"/hw_audio_cs4231a.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_audio_cs4231a.c.ll \
				"$workdir"/cs4231a-depend.txt "$libcommon"/instrumented_hw_audio_cs4231a.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_audio_cs4231a.c.o -c "$libcommon"/instrumented_hw_audio_cs4231a.c.ll
			cp "$libcommon"/hw_audio_cs4231a.c.o ../
			;;
		"ctu-can-upstream")
			make "$libcommon"/hw_net_can_ctucan_pci.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build \
				./"$libcommon"/hw_net_can_ctucan_pci.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_net_can_ctucan_pci.c.ll \
				"$workdir"/ctu-can-depend.txt "$libcommon"/instrumented_hw_net_can_ctucan_pci.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_net_can_ctucan_pci.c.o -c "$libcommon"/instrumented_hw_net_can_ctucan_pci.c.ll
			cp "$libcommon"/hw_net_can_ctucan_pci.c.o ../
			;;
		"ehci-upstream")
			make "$libcommon"/hw_usb_hcd-ehci-pci.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_usb_hcd-ehci-pci.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_usb_hcd-ehci-pci.c.ll \
				"$workdir"/ehci-depend.txt "$libcommon"/instrumented_hw_usb_hcd-ehci-pci.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_usb_hcd-ehci-pci.c.o -c "$libcommon"/instrumented_hw_usb_hcd-ehci-pci.c.ll
			cp "$libcommon"/hw_usb_hcd-ehci-pci.c.o ../
			;;
		"es1370-upstream")
			make "$libcommon"/hw_audio_es1370.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_audio_es1370.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_audio_es1370.c.ll \
				"$workdir"/es1370-depend.txt "$libcommon"/instrumented_hw_audio_es1370.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_audio_es1370.c.o -c "$libcommon"/instrumented_hw_audio_es1370.c.ll
			cp "$libcommon"/hw_audio_es1370.c.o ../
			;;
		"esp1"|"esp1-fpe"|"esp3"|"esp3-buf"|"esp4"|"esp5"|"esp5-buf"|"esp-upstream")
			make "$libcommon"/hw_scsi_esp-pci.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_scsi_esp-pci.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_scsi_esp-pci.c.ll \
				"$workdir"/am53c974-depend.txt "$libcommon"/instrumented_hw_scsi_esp-pci.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_scsi_esp-pci.c.o -c "$libcommon"/instrumented_hw_scsi_esp-pci.c.ll
			cp "$libcommon"/hw_scsi_esp-pci.c.o ../
			;;
		"fdc-upstream")
			make "$libcommon"/hw_block_fdc-sysbus.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_block_fdc-sysbus.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_block_fdc-sysbus.c.ll \
				"$workdir"/fdc-depend.txt "$libcommon"/instrumented_hw_block_fdc-sysbus.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_block_fdc-sysbus.c.o -c "$libcommon"/instrumented_hw_block_fdc-sysbus.c.ll
			cp "$libcommon"/hw_block_fdc-sysbus.c.o ../
			;;
		"imx-usb-phy-upstream")
			make "$libcommon"/hw_usb_imx-usb-phy.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_usb_imx-usb-phy.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_usb_imx-usb-phy.c.ll \
				"$workdir"/imx-usb-phy-depend.txt "$libcommon"/instrumented_hw_usb_imx-usb-phy.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_usb_imx-usb-phy.c.o -c "$libcommon"/instrumented_hw_usb_imx-usb-phy.c.ll
			cp "$libcommon"/hw_usb_imx-usb-phy.c.o ../
			;;
		"kvaser-can-upstream")
			make "$libcommon"/hw_net_can_can_kvaser_pci.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_net_can_can_kvaser_pci.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_net_can_can_kvaser_pci.c.ll \
				"$workdir"/kvaser-can-depend.txt "$libcommon"/instrumented_hw_net_can_can_kvaser_pci.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_net_can_can_kvaser_pci.c.o -c "$libcommon"/instrumented_hw_net_can_can_kvaser_pci.c.ll
			cp "$libcommon"/hw_net_can_can_kvaser_pci.c.o ../
			;;
		"lan9118"|"lan9118-2"|"lan9118-upstream")
			make "$libcommon"/hw_net_lan9118.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_net_lan9118.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_net_lan9118.c.ll \
				"$workdir"/lan9118-depend.txt "$libcommon"/instrumented_hw_net_lan9118.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_net_lan9118.c.o -c "$libcommon"/instrumented_hw_net_lan9118.c.ll
			cp "$libcommon"/hw_net_lan9118.c.o ../
			;;
		"lsi53c895a"|"lsi53c895a-upstream")
			make "$libcommon"/hw_scsi_lsi53c895a.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_scsi_lsi53c895a.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_scsi_lsi53c895a.c.ll \
				"$workdir"/lsi53c895a-depend.txt "$libcommon"/instrumented_hw_scsi_lsi53c895a.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_scsi_lsi53c895a.c.o -c "$libcommon"/instrumented_hw_scsi_lsi53c895a.c.ll
			cp "$libcommon"/hw_scsi_lsi53c895a.c.o ../
			;;
		"megasas-upstream")
			make "$libcommon"/hw_scsi_megasas.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_scsi_megasas.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_scsi_megasas.c.ll \
				"$workdir"/megaraid-depend.txt "$libcommon"/instrumented_hw_scsi_megasas.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_scsi_megasas.c.o -c "$libcommon"/instrumented_hw_scsi_megasas.c.ll
			cp "$libcommon"/hw_scsi_megasas.c.o ../
			;;
		"ne2000-upstream")
			make "$libcommon"/hw_net_ne2000-pci.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_net_ne2000-pci.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_net_ne2000-pci.c.ll \
				"$workdir"/ne2k_pci-depend.txt "$libcommon"/instrumented_hw_net_ne2000-pci.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_net_ne2000-pci.c.o -c "$libcommon"/instrumented_hw_net_ne2000-pci.c.ll
			cp "$libcommon"/hw_net_ne2000-pci.c.o ../
			;;
		"nvme"|"nvme-2"|"nvme-upstream")
			make "$libcommon"/hw_nvme_ctrl.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_nvme_ctrl.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_nvme_ctrl.c.ll \
				"$workdir"/nvme-depend.txt "$libcommon"/instrumented_hw_nvme_ctrl.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_nvme_ctrl.c.o -c "$libcommon"/instrumented_hw_nvme_ctrl.c.ll
			cp "$libcommon"/hw_nvme_ctrl.c.o ../
			;;
		"ohci"|"ohci-upstream")
			make "$libcommon"/hw_usb_hcd-ohci-pci.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build \
				./"$libcommon"/hw_usb_hcd-ohci-pci.c.ll
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_usb_hcd-ohci-pci.c.ll \
				"$workdir"/ohci-depend.txt "$libcommon"/instrumented_hw_usb_hcd-ohci-pci.c.ll
			$compiler -m64 -mcx16 -o "$libcommon"/hw_usb_hcd-ohci-pci.c.o -c "$libcommon"/instrumented_hw_usb_hcd-ohci-pci.c.ll
			cp "$libcommon"/hw_usb_hcd-ohci-pci.c.o ../
			;;
		"parallel-upstream")
			make "$libcommon"/hw_char_parallel.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build \
				./"$libcommon"/hw_char_parallel.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_char_parallel.c.ll \
				"$workdir"/parallel-depend.txt "$libcommon"/instrumented_hw_char_parallel.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_char_parallel.c.o -c "$libcommon"/instrumented_hw_char_parallel.c.ll
			cp "$libcommon"/hw_char_parallel.c.o ../
			;;
		"pcm3680-can-upstream")
			make "$libcommon"/hw_net_can_can_pcm3680_pci.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_net_can_can_pcm3680_pci.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_net_can_can_pcm3680_pci.c.ll \
				"$workdir"/pcm3680-can-depend.txt "$libcommon"/instrumented_hw_net_can_can_pcm3680_pci.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_net_can_can_pcm3680_pci.c.o -c "$libcommon"/instrumented_hw_net_can_can_pcm3680_pci.c.ll
			cp "$libcommon"/hw_net_can_can_pcm3680_pci.c.o ../
			;;
		"pcnet-upstream")
			make "$libcommon"/hw_net_pcnet-pci.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_net_pcnet-pci.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_net_pcnet-pci.c.ll \
				"$workdir"/pcnet-depend.txt "$libcommon"/instrumented_hw_net_pcnet-pci.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_net_pcnet-pci.c.o -c "$libcommon"/instrumented_hw_net_pcnet-pci.c.ll
			cp "$libcommon"/hw_net_pcnet-pci.c.o ../
			;;
		"qxl-upstream")
			make "$libcommon"/hw_display_qxl.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_display_qxl.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_display_qxl.c.ll \
				"$workdir"/qxl-depend.txt "$libcommon"/instrumented_hw_display_qxl.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_display_qxl.c.o -c "$libcommon"/instrumented_hw_display_qxl.c.ll
			cp "$libcommon"/hw_display_qxl.c.o ../
			;;
		"rocker-upstream")
			make "$libcommon"/hw_net_rocker_rocker.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_net_rocker_rocker.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_net_rocker_rocker.c.ll \
				"$workdir"/rocker-depend.txt "$libcommon"/instrumented_hw_net_rocker_rocker.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_net_rocker_rocker.c.o -c "$libcommon"/instrumented_hw_net_rocker_rocker.c.ll
			cp "$libcommon"/hw_net_rocker_rocker.c.o ../
			;;
		"rtl8139-upstream")
			make "$libcommon"/hw_net_rtl8139.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_net_rtl8139.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_net_rtl8139.c.ll \
				"$workdir"/rtl8139-depend.txt "$libcommon"/instrumented_hw_net_rtl8139.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_net_rtl8139.c.o -c "$libcommon"/instrumented_hw_net_rtl8139.c.ll
			cp "$libcommon"/hw_net_rtl8139.c.o ../
			;;
		"sdhci"|"sdhci-upstream")
			make "$libcommon"/hw_sd_sdhci-pci.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_sd_sdhci-pci.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_sd_sdhci-pci.c.ll \
				"$workdir"/sdhci-v3-depend.txt "$libcommon"/instrumented_hw_sd_sdhci-pci.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_sd_sdhci-pci.c.o -c "$libcommon"/instrumented_hw_sd_sdhci-pci.c.ll
			cp "$libcommon"/hw_sd_sdhci-pci.c.o ../
			;;
		"smc91c111"|"smc91c111-2"|"smc91c111-3"|"smc91c111-4"|"smc91c111-upstream")
			make "$libcommon"/hw_net_smc91c111.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_net_smc91c111.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_net_smc91c111.c.ll \
				"$workdir"/smc91c111-depend.txt "$libcommon"/instrumented_hw_net_smc91c111.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_net_smc91c111.c.o -c "$libcommon"/instrumented_hw_net_smc91c111.c.ll
			cp "$libcommon"/hw_net_smc91c111.c.o ../
			;;
		"std-vga"|"std-vga-upstream")
			make "$libcommon"/hw_display_vga-mmio.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_display_vga-mmio.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_display_vga-mmio.c.ll \
				"$workdir"/std-vga-depend.txt "$libcommon"/instrumented_hw_display_vga-mmio.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_display_vga-mmio.c.o -c "$libcommon"/instrumented_hw_display_vga-mmio.c.ll
			cp "$libcommon"/hw_display_vga-mmio.c.o ../
			;;
		"stellaris-enet-upstream")
			make "$libcommon"/hw_net_stellaris_enet.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_net_stellaris_enet.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_net_stellaris_enet.c.ll \
				"$workdir"/stellaris-enet-depend.txt "$libcommon"/instrumented_hw_net_stellaris_enet.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_net_stellaris_enet.c.o -c "$libcommon"/instrumented_hw_net_stellaris_enet.c.ll
			cp "$libcommon"/hw_net_stellaris_enet.c.o ../
			;;
		"tulip-upstream")
			make "$libcommon"/hw_net_tulip.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_net_tulip.c.ll
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_net_tulip.c.ll \
				"$workdir"/tulip-depend.txt "$libcommon"/instrumented_hw_net_tulip.c.ll
			$compiler -m64 -mcx16 -o "$libcommon"/hw_net_tulip.c.o -c "$libcommon"/instrumented_hw_net_tulip.c.ll
			cp "$libcommon"/hw_net_tulip.c.o ../
			;;
		"uhci-upstream")
			make "$libcommon"/hw_usb_hcd-uhci.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_usb_hcd-uhci.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_usb_hcd-uhci.c.ll \
				"$workdir"/uhci-depend.txt "$libcommon"/instrumented_hw_usb_hcd-uhci.c.ll
			$compiler -m64 -mcx16 -o "$libcommon"/hw_usb_hcd-uhci.c.o -c "$libcommon"/instrumented_hw_usb_hcd-uhci.c.ll
			cp "$libcommon"/hw_usb_hcd-uhci.c.o ../
			;;
		"vmware-svga-upstream")
			make "$libcommon"/hw_display_vmware-vga.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_display_vmware-vga.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_display_vmware-vga.c.ll \
				"$workdir"/vmware-svga-depend.txt "$libcommon"/instrumented_hw_display_vmware-vga.c.ll
			$compiler -m64 -mcx16 -o "$libcommon"/hw_display_vmware-vga.c.o -c "$libcommon"/instrumented_hw_display_vmware-vga.c.ll
			cp "$libcommon"/hw_display_vmware-vga.c.o ../
			;;
		"xgmac-upstream")
			make "$libcommon"/hw_net_xgmac.c.ll
			../get_device_module.sh ../Kekule.so "$workdir"/qemu/build ./"$libcommon"/hw_net_xgmac.c.ll &> log.txt
			../instrument.sh "$platform" ../Kekule.so "$workdir"/qemu/build "$libcommon"/hw_net_xgmac.c.ll \
				"$workdir"/xgmac-depend.txt "$libcommon"/instrumented_hw_net_xgmac.c.ll &> error.txt
			$compiler -m64 -mcx16 -o "$libcommon"/hw_net_xgmac.c.o -c "$libcommon"/instrumented_hw_net_xgmac.c.ll
			cp "$libcommon"/hw_net_xgmac.c.o ../
			;;
		*)
			echo "Not supported target"
			;;
	esac
	if [[ "$mode" == "san" ]]
	then
		if [[ "$version" == "esp1" || "$version" == "esp1-fpe" || "$version" == "sdhci" || "$version" == "nvme" || "$version" == "e1000" ]]
		then
			CC=$compiler CXX=$compilerx ../configure \
				--enable-fuzzing \
				--disable-werror $sanitizer_option --enable-spice \
				--enable-slirp --disable-download $less_crashes \
				--target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		else
			CC=$compiler CXX=$compilerx ../configure \
				--enable-fuzzing \
				--disable-werror $sanitizer_option --enable-spice \
				--enable-slirp $less_crashes \
				--target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		fi
	elif [[ "$mode" == "dep" ]]
	then
		if [[ "$version" == "esp1" || "$version" == "esp1-fpe" || "$version" == "sdhci" || "$version" == "nvme" || "$version" == "e1000" ]]
		then
			CC=$compiler CXX=$compilerx ../configure \
				--enable-fuzzing \
				--disable-werror $sanitizer_option --enable-spice \
				--enable-slirp --disable-download \
				--extra-cflags="-DLESS_CRASHES" \
				--target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		else
			CC=$compiler CXX=$compilerx ../configure \
				--enable-fuzzing \
				--disable-werror $sanitizer_option --enable-spice \
				--enable-slirp \
				--extra-cflags="-DLESS_CRASHES" \
				--target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		fi
	elif [[ "$mode" == "cov" ]]
	then
		if [[ "$version" == "esp1" || "$version" == "esp1-fpe" || "$version" == "sdhci" || "$version" == "nvme" || "$version" == "e1000" ]]
		then
			CLANG_COV_DUMP=1 \
			CC=$compiler CXX=$compilerx ../configure \
			    --enable-fuzzing \
			    --disable-werror $dis_sanitizer_option --enable-spice \
			    --enable-slirp --disable-download \
			    --extra-cflags="-DCLANG_COV_DUMP -DLESS_CRASHES -fprofile-instr-generate -fcoverage-mapping" \
			    --target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		else
			CLANG_COV_DUMP=1 \
			CC=$compiler CXX=$compilerx ../configure \
			    --enable-fuzzing \
			    --disable-werror $dis_sanitizer_option --enable-spice \
			    --enable-slirp \
			    --extra-cflags="-DCLANG_COV_DUMP -DLESS_CRASHES -fprofile-instr-generate -fcoverage-mapping" \
			    --target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		fi
	fi
	case "$version" in
		"upstream")
			;;
		"ac97-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_audio_ac97.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_audio_ac97.c.o "$fuzzer"
			;;
		"acpi-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_acpi_erst.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_acpi_erst.c.o "$fuzzer"
			;;
		"ahci-hd-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_ide_ahci.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_ide_ahci.c.o "$fuzzer"
			;;
		"ati1"|"ati2"|"ati-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_display_ati.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_display_ati.c.o "$fuzzer"
			;;
		"cirrus-vga"|"cirrus-vga-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_display_cirrus_vga.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_display_cirrus_vga.c.o "$fuzzer"
			;;
		"cs4231-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_audio_cs4231.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_audio_cs4231.c.o "$fuzzer"
			;;
		"cs4231a-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_audio_cs4231a.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_audio_cs4231a.c.o "$fuzzer"
			;;
		"ctu-can-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_net_can_ctucan_pci.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_net_can_ctucan_pci.c.o "$fuzzer"
			;;
		"ehci-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_usb_hcd-ehci-pci.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_usb_hcd-ehci-pci.c.o "$fuzzer"
			;;
		"es1370-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_audio_es1370.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_audio_es1370.c.o "$fuzzer"
			;;
		"esp1"|"esp1-fpe"|"esp3"|"esp3-buf"|"esp4"|"esp5"|"esp5-buf"|"esp-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_scsi_esp-pci.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_scsi_esp-pci.c.o "$fuzzer"
			;;
		"fdc-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_block_fdc-sysbus.c.o ./"$libcommon"/hw_block_fdc.c.o
			ninja -d keeprsp -n qemu-fuzz-x86_64
			$compiler -m64 -mcx16 @qemu-fuzz-x86_64.rsp
			;;
		"imx-usb-phy-upstream")
			make qemu-fuzz-arm -j12
			cp ../hw_usb_imx-usb-phy.c.o ./"$libcommon"/
			link_to_binary arm "$libcommon"/hw_usb_imx-usb-phy.c.o "$fuzzer"
			;;
		"kvaser-can-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_net_can_can_kvaser_pci.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_net_can_can_kvaser_pci.c.o "$fuzzer"
			;;
		"lan9118"|"lan9118-2"|"lan9118-upstream")
			make qemu-fuzz-arm -j12
			cp ../hw_net_lan9118.c.o ./"$libcommon"/
			link_to_binary arm "$libcommon"/hw_net_lan9118.c.o "$fuzzer"
			;;
		"lsi53c895a"|"lsi53c895a-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_scsi_lsi53c895a.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_scsi_lsi53c895a.c.o "$fuzzer"
			;;
		"megasas-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_scsi_megasas.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_scsi_megasas.c.o "$fuzzer"
			;;
		"ne2000-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_net_ne2000-pci.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_net_ne2000-pci.c.o "$fuzzer"
			;;
		"nvme"|"nvme-2"|"nvme-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_nvme_ctrl.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_nvme_ctrl.c.o "$fuzzer"
			;;
		"ohci"|"ohci-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_usb_hcd-ohci-pci.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_usb_hcd-ohci-pci.c.o "$fuzzer"
			;;
		"parallel-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_char_parallel.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_char_parallel.c.o "$fuzzer"
			;;
		"pcm3680-can-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_net_can_can_pcm3680_pci.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_net_can_can_pcm3680_pci.c.o "$fuzzer"
			;;
		"pcnet-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_net_pcnet-pci.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_net_pcnet-pci.c.o "$fuzzer"
			;;
		"qxl-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_display_qxl.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_display_qxl.c.o "$fuzzer"
			;;
		"rocker-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_net_rocker_rocker.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_net_rocker_rocker.c.o "$fuzzer"
			;;
		"rtl8139-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_net_rtl8139.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_net_rtl8139.c.o "$fuzzer"
			;;
		"sdhci"|"sdhci-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_sd_sdhci-pci.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_sd_sdhci-pci.c.o "$fuzzer"
			;;
		"smc91c111"|"smc91c111-2"|"smc91c111-3"|"smc91c111-4"|"smc91c111-upstream")
			make qemu-fuzz-arm -j12
			cp ../hw_net_smc91c111.c.o ./"$libcommon"/
			link_to_binary arm "$libcommon"/hw_net_smc91c111.c.o "$fuzzer"
			;;
		"std-vga"|"std-vga-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_display_vga-mmio.c.o ./"$libcommon"/hw_display_vga.c.o
			link_to_binary x86_64 "$libcommon"/hw_display_vga.c.o "$fuzzer"
			;;
		"stellaris-enet-upstream")
			make qemu-fuzz-arm -j12
			cp ../hw_net_stellaris_enet.c.o ./"$libcommon"/
			link_to_binary arm "$libcommon"/hw_net_stellaris_enet.c.o "$fuzzer"
			;;
		"tulip-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_net_tulip.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_net_tulip.c.o "$fuzzer"
			;;
		"uhci-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_usb_hcd-uhci.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_usb_hcd-uhci.c.o "$fuzzer"
			;;
		"vmware-svga-upstream")
			make qemu-fuzz-x86_64 -j12
			cp ../hw_display_vmware-vga.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_display_vmware-vga.c.o "$fuzzer"
			;;
		"xgmac-upstream")
			make qemu-fuzz-arm -j12
			cp ../hw_net_xgmac.c.o ./"$libcommon"/
			link_to_binary arm "$libcommon"/hw_net_xgmac.c.o "$fuzzer"
			;;
		*)
			echo "Not supported target"
			;;
	esac
fi
cd ..
