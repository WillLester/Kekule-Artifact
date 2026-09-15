#!/bin/bash
vanilla="$1"
version="$2"
mode="$3"
extra_option="$4"

if [[ "$#" -lt 3 ]]; then
	echo "Usage: vanilla (-v or -n) target device, mode (san or cov), extra option (-p = path feedback, -a = no priority)."
	exit 1
fi

if [[ "$vanilla" == "-v" ]]
then
	cd llvm-project-15.0.0.src/llvm
else
	cd llvm-project-15.0.0-mod.src/llvm
fi
rm -rf build
if [[ "$mode" == "dep" ]]
then
	if [[ "$extra_option" == "-b" ]]
	then
		patch ../compiler-rt/lib/fuzzer/FuzzerLoop.cpp < ~/videzzo/dep-base.patch
	else
		patch ../compiler-rt/lib/fuzzer/FuzzerLoop.cpp < ~/videzzo/dep.patch
	fi
elif [[ "$mode" == "san" ]]
then
	if [[ "$extra_option" == "-a" ]]
	then
		patch ../compiler-rt/lib/fuzzer/FuzzerLoop.cpp < ~/videzzo/no-priority.patch
	elif [[ "$extra_option" == "-p" ]]
	then
		patch ../compiler-rt/lib/fuzzer/FuzzerLoop.cpp < ~/videzzo/FuzzerLoop.cpp.patch
		patch ../compiler-rt/lib/fuzzer/FuzzerTracePC.h < ~/videzzo/FuzzerTracePC.h.patch
	fi
fi
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_CXX_FLAGS="-fconcepts" -DLLVM_ENABLE_PROJECTS="clang;compiler-rt" -DCMAKE_INSTALL_PREFIX="/root/llvm-project" ..
make install -j24
if [[ "$vanilla" != "-v" ]]
then
	cd ../../../pass
	mkdir build && cd build
	cmake ../ && make -j4
	cp pass/Kekule.so ../
	cd ../../videzzo_qemu
else
	cd ../../../videzzo_qemu
fi
make qemu-dep
cd qemu
less_crashes=""
fuzzer="videzzo"
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
		less_crashes="--extra-cflags=-DVIDEZZO_LESS_CRASHES"
		;;
	"esp1"|"nvme")
		git checkout c167c80b
		less_crashes="--extra-cflags=-DVIDEZZO_LESS_CRASHES"
		patch hw/scsi/scsi-disk.c < ../esp-cov.patch
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
	"esp4")
		git checkout 5915139a
		;;
	"esp5")
		git checkout c80a3395
		;;
	"ohci"|"lan9118-2"|"nvme-2")
		git checkout 5134cf9b
		;;
	"lsi53c895a"|"smc91c111"|"lan9118")
		git checkout fea445e8
		;;
	"sdhci")
		git checkout c0c6a0e3
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
			;;
		"ohci-upstream")
			patch hw/usb/core.c < ../core.c.patch
			;;
		"sdhci-upstream")
			patch hw/sd/sdhci.c < ../sdhci.c.patch
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
		"smc91c111-upstream")
			git checkout 7c89e226
			patch hw/net/smc91c111.c < ../smc91c111-4.c.patch
			;;
		*)
			;;
	esac
fi
# The commit hash or reference to check against
TARGET_COMMIT="5915139a"
SANITIZER_COMMIT="cb771ac1f"
MESON_COMMIT="07f0d3264"

# Get the current commit hash
CURRENT_COMMIT=$(git rev-parse HEAD)

# Check if the current commit is before the target commit
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

SYSEMU_COMMIT="63cda19446"
EXEC_COMMIT="548a01650c9"
MAINSTONE_COMMIT="6e5a2d771"

# Patches for compatiblity
if git merge-base --is-ancestor "$CURRENT_COMMIT" "$SYSEMU_COMMIT"
then
	echo "The current commit is before the target commit [$SYSEMU_COMMIT]"
	patch ~/videzzo/videzzo_qemu/videzzo_qemu.c < ~/videzzo/videzzo_qemu-sysemu.patch
else
	echo "The current commit is at or after the target commit [$SYSEMU_COMMIT]"
fi

if git merge-base --is-ancestor "$MAINSTONE_COMMIT" "$CURRENT_COMMIT"
then
	echo "The current commit is at or after the target commit [$MAINSTONE_COMMIT]"
	patch ~/videzzo/videzzo_qemu/videzzo_qemu.c < ~/videzzo/videzzo_qemu-mainstone.patch
else
	echo "The current commit is before the target commit [$MAINSTONE_COMMIT]"
fi

if git merge-base --is-ancestor "$EXEC_COMMIT" "$CURRENT_COMMIT"
then
	echo "The current commit is at or after the target commit [$EXEC_COMMIT]"
	patch ~/videzzo/videzzo_qemu/videzzo_qemu.c < ~/videzzo/videzzo_qemu-exec.patch
else
	echo "The current commit is before the target commit [$MAINSTONE_COMMIT]"
fi

if [[ "$vanilla" != "-v" ]]
then
	case "$version" in
		"esp1"|"esp1-fpe"|"nvme"|"sdhci")
			patch python/scripts/vendor.py < ../vendor.py.patch
			;;
		"esp2")
			patch hw/scsi/esp.c < ../esp-vanilla-2.patch
			;;
		"esp3")
			;;
		"upstream")
			;;
		*)
			echo "No specific things to do"
			;;
	esac
else
	case "$version" in
		"esp2")
			patch hw/scsi/esp.c < ../esp-vanilla-2.patch
			;;
		*)
			echo "Others"
			;;
	esac
fi
cd ..
pip install tomli
make patch
cd ..
build_dir=""
if [[ "$mode" == "san" ]]
then
	patch -N ./videzzo_qemu/qemu/include/qemu/cutils.h < ./patches/include/qemu/cutils.h.patch
	patch -N ./videzzo_qemu/qemu/util/cutils.c < ./patches/util/cutils.c.patch
	patch -N ./videzzo_qemu/qemu/tests/qtest/libqtest.c < ./patches/tests/qtest/libqtest-ok.patch
fi
if [[ "$mode" == "san" ]] || [[ "$mode" == "dep" ]]
then
	make qemu
	build_dir="out-san"
elif [[ "$mode" == "cov" ]]
then
	make qemu-coverage
	build_dir="out-cov"
fi

mkdir corpus

# function defs

link_to_binary() {
	local arch="$1"
	local core_file="$2"
	if [[ "$meson_version" == "1.2.3" ]] && [[ "$version" != "esp1" && "$version" != "esp1-fpe" && "$version" != "sdhci" && "$version" != "nvme" && "$version" != "e1000" ]] 
	then
		ninja -d keeprsp -n qemu-videzzo-"$arch"
		python3 generate_rsp.py "$arch" "$fuzzer" "$core_file"
		clang -m64 -mcx16 @qemu-videzzo-new-"$arch".rsp
	else
		ninja -v -n qemu-videzzo-"$arch" &> link.tmp
		python3 generate_link_sh.py link.tmp link.sh "$core_file"
		rm link.tmp
		chmod +x link.sh
		./link.sh
	fi
}

link_device() {
	local arch="$1"
	local core_file="$2"
	make qemu-"$fuzzer"-"$arch" -j4
	cp ../../../"$core_file" ./"$libcommon"/
	link_to_binary "$arch" "$libcommon"/"$core_file"
}

# Update compilation framework
if [[ "$vanilla" != "-v" ]]
then
	cd meson-"$meson_version"
	pip wheel .
	cp meson-"$meson_version"-py3-none-any.whl ../videzzo_qemu/qemu/python/wheels/
	cd ../videzzo_qemu/qemu
	patch configure < ../../configure-us.patch
	patch meson_options.txt < ../../meson_options-vd.patch
	make update-buildoptions
	cd "$build_dir"
	cp ../../../generate_rsp.py ./
	cp ../../../generate_link_sh.py ./
	if [[ "$mode" == "san" ]]
	then
		if [[ "$version" == "esp1" || "$version" == "esp1-fpe" || "$version" == "sdhci" || "$version" == "nvme" || "$version" == "e1000" ]]
		then
			CC=clang CXX=clang++ ../configure \
				--enable-videzzo --enable-fuzzing \
				--disable-werror $sanitizer_option --enable-spice \
				--enable-slirp --enable-llvm --disable-download $less_crashes \
				--disable-gtk --disable-sdl \
				--target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		else
			CC=clang CXX=clang++ ../configure \
				--enable-videzzo --enable-fuzzing \
				--disable-werror $sanitizer_option --enable-spice \
				--enable-slirp --enable-llvm --disable-download $less_crashes \
				--disable-gtk --disable-sdl \
				--target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		fi
	elif [[ "$mode" == "dep" ]]
	then
		if [[ "$version" == "esp1" || "$version" == "esp1-fpe" || "$version" == "sdhci" || "$version" == "nvme" || "$version" == "e1000" ]]
		then
			CC=clang CXX=clang++ ../configure \
				--enable-videzzo --enable-fuzzing \
				--disable-werror $sanitizer_option --enable-spice \
				--enable-slirp --enable-llvm --disable-download \
				--extra-cflags="-DVIDEZZO_LESS_CRASHES" \
				--disable-gtk --disable-sdl \
				--target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		else
			CC=clang CXX=clang++ ../configure \
				--enable-videzzo --enable-fuzzing \
				--disable-werror $sanitizer_option --enable-spice \
				--enable-slirp --enable-llvm --disable-download \
				--extra-cflags="-DVIDEZZO_LESS_CRASHES" \
				--disable-gtk --disable-sdl \
				--target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		fi
	elif [[ "$mode" == "cov" ]]
	then
		if [[ "$version" == "esp1" || "$version" == "esp1-fpe" || "$version" == "sdhci" || "$version" == "nvme" || "$version" == "e1000" ]]
		then
			CLANG_COV_DUMP=1 \
			CC=clang CXX=clang++ ../configure \
			    --enable-videzzo --enable-fuzzing \
			    --disable-werror $dis_sanitizer_option --enable-spice \
			    --enable-slirp --enable-llvm --disable-download \
				--disable-gtk --disable-sdl \
			    --extra-cflags="-DCLANG_COV_DUMP -DVIDEZZO_LESS_CRASHES -fprofile-instr-generate -fcoverage-mapping" \
			    --target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		else
			CLANG_COV_DUMP=1 \
			CC=clang CXX=clang++ ../configure \
			    --enable-videzzo --enable-fuzzing \
			    --disable-werror $dis_sanitizer_option --enable-spice \
			    --enable-slirp --enable-llvm --disable-download \
				--disable-gtk --disable-sdl \
			    --extra-cflags="-DCLANG_COV_DUMP -DVIDEZZO_LESS_CRASHES -fprofile-instr-generate -fcoverage-mapping" \
			    --target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		fi
	fi
	case "$version" in
		"upstream")
			;;
		"ac97-upstream")
			make "$libcommon"/hw_audio_ac97.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" \
				./"$libcommon"/hw_audio_ac97.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_audio_ac97.c.ll \
				~/videzzo/ac97-depend.txt "$libcommon"/instrumented_hw_audio_ac97.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_audio_ac97.c.o -c "$libcommon"/instrumented_hw_audio_ac97.c.ll
			cp "$libcommon"/hw_audio_ac97.c.o ../../../
			;;
		"acpi-upstream")
			make "$libcommon"/hw_acpi_erst.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" \
				./"$libcommon"/hw_acpi_erst.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_acpi_erst.c.ll \
				~/videzzo/acpi-erst-depend.txt "$libcommon"/instrumented_hw_acpi_erst.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_acpi_erst.c.o -c "$libcommon"/instrumented_hw_acpi_erst.c.ll
			cp "$libcommon"/hw_acpi_erst.c.o ../../../
			;;
		"ahci-hd-upstream")
			make "$libcommon"/hw_ide_ahci.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" \
				./"$libcommon"/hw_ide_ahci.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_ide_ahci.c.ll \
				~/videzzo/ahci-hd-depend.txt "$libcommon"/instrumented_hw_ide_ahci.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_ide_ahci.c.o -c "$libcommon"/instrumented_hw_ide_ahci.c.ll
			cp "$libcommon"/hw_ide_ahci.c.o ../../../
			;;
		"ati1"|"ati2"|"ati-upstream")
			make "$libcommon"/hw_display_ati.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" \
				./"$libcommon"/hw_display_ati.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_display_ati.c.ll \
				~/videzzo/ati-depend.txt "$libcommon"/instrumented_hw_display_ati.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_display_ati.c.o -c "$libcommon"/instrumented_hw_display_ati.c.ll
			cp "$libcommon"/hw_display_ati.c.o ../../../
			;;
		"cirrus-vga"|"cirrus-vga-upstream")
			make "$libcommon"/hw_display_cirrus_vga.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" \
				./"$libcommon"/hw_display_cirrus_vga.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_display_cirrus_vga.c.ll \
				~/videzzo/cirrus-vga-depend.txt "$libcommon"/instrumented_hw_display_cirrus_vga.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_display_cirrus_vga.c.o -c "$libcommon"/instrumented_hw_display_cirrus_vga.c.ll
			cp "$libcommon"/hw_display_cirrus_vga.c.o ../../../
			;;
		"cs4231-upstream")
			make "$libcommon"/hw_audio_cs4231.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" \
				./"$libcommon"/hw_audio_cs4231.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_audio_cs4231.c.ll \
				~/videzzo/cs4231-depend.txt "$libcommon"/instrumented_hw_audio_cs4231.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_audio_cs4231.c.o -c "$libcommon"/instrumented_hw_audio_cs4231.c.ll
			cp "$libcommon"/hw_audio_cs4231.c.o ../../../
			;;
		"cs4231a-upstream")
			make "$libcommon"/hw_audio_cs4231a.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" \
				./"$libcommon"/hw_audio_cs4231a.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_audio_cs4231a.c.ll \
				~/videzzo/cs4231a-depend.txt "$libcommon"/instrumented_hw_audio_cs4231a.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_audio_cs4231a.c.o -c "$libcommon"/instrumented_hw_audio_cs4231a.c.ll
			cp "$libcommon"/hw_audio_cs4231a.c.o ../../../
			;;
		"ctu-can-upstream")
			make "$libcommon"/hw_net_can_ctucan_pci.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" \
				./"$libcommon"/hw_net_can_ctucan_pci.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_net_can_ctucan_pci.c.ll \
				~/videzzo/ctu-can-depend.txt "$libcommon"/instrumented_hw_net_can_ctucan_pci.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_net_can_ctucan_pci.c.o -c "$libcommon"/instrumented_hw_net_can_ctucan_pci.c.ll
			cp "$libcommon"/hw_net_can_ctucan_pci.c.o ../../../
			;;
		"ehci-upstream")
			make "$libcommon"/hw_usb_hcd-ehci-pci.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_usb_hcd-ehci-pci.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_usb_hcd-ehci-pci.c.ll \
				~/videzzo/ehci-depend.txt "$libcommon"/instrumented_hw_usb_hcd-ehci-pci.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_usb_hcd-ehci-pci.c.o -c "$libcommon"/instrumented_hw_usb_hcd-ehci-pci.c.ll
			cp "$libcommon"/hw_usb_hcd-ehci-pci.c.o ../../../
			;;
		"es1370-upstream")
			make "$libcommon"/hw_audio_es1370.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_audio_es1370.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_audio_es1370.c.ll \
				~/videzzo/es1370-depend.txt "$libcommon"/instrumented_hw_audio_es1370.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_audio_es1370.c.o -c "$libcommon"/instrumented_hw_audio_es1370.c.ll
			cp "$libcommon"/hw_audio_es1370.c.o ../../../
			;;
		"esp1"|"esp1-fpe"|"esp3"|"esp4"|"esp5"|"esp-upstream")
			make "$libcommon"/hw_scsi_esp-pci.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_scsi_esp-pci.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_scsi_esp-pci.c.ll \
				~/videzzo/am53c974-depend.txt "$libcommon"/instrumented_hw_scsi_esp-pci.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_scsi_esp-pci.c.o -c "$libcommon"/instrumented_hw_scsi_esp-pci.c.ll
			cp "$libcommon"/hw_scsi_esp-pci.c.o ../../../
			;;
		"fdc-upstream")
			make "$libcommon"/hw_block_fdc-sysbus.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_block_fdc-sysbus.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_block_fdc-sysbus.c.ll \
				~/videzzo/fdc-depend.txt "$libcommon"/instrumented_hw_block_fdc-sysbus.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_block_fdc-sysbus.c.o -c "$libcommon"/instrumented_hw_block_fdc-sysbus.c.ll
			cp "$libcommon"/hw_block_fdc-sysbus.c.o ../../../
			;;
		"imx-usb-phy-upstream")
			make "$libcommon"/hw_usb_imx-usb-phy.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_usb_imx-usb-phy.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_usb_imx-usb-phy.c.ll \
				~/videzzo/imx-usb-phy-depend.txt "$libcommon"/instrumented_hw_usb_imx-usb-phy.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_usb_imx-usb-phy.c.o -c "$libcommon"/instrumented_hw_usb_imx-usb-phy.c.ll
			cp "$libcommon"/hw_usb_imx-usb-phy.c.o ../../../
			;;
		"kvaser-can-upstream")
			make "$libcommon"/hw_net_can_can_kvaser_pci.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_net_can_can_kvaser_pci.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_net_can_can_kvaser_pci.c.ll \
				~/videzzo/kvaser-can-depend.txt "$libcommon"/instrumented_hw_net_can_can_kvaser_pci.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_net_can_can_kvaser_pci.c.o -c "$libcommon"/instrumented_hw_net_can_can_kvaser_pci.c.ll
			cp "$libcommon"/hw_net_can_can_kvaser_pci.c.o ../../../
			;;
		"lan9118"|"lan9118-2"|"lan9118-upstream")
			make "$libcommon"/hw_net_lan9118.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_net_lan9118.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_net_lan9118.c.ll \
				~/videzzo/lan9118-depend.txt "$libcommon"/instrumented_hw_net_lan9118.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_net_lan9118.c.o -c "$libcommon"/instrumented_hw_net_lan9118.c.ll
			cp "$libcommon"/hw_net_lan9118.c.o ../../../
			;;
		"lsi53c895a"|"lsi53c895a-upstream")
			make "$libcommon"/hw_scsi_lsi53c895a.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_scsi_lsi53c895a.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_scsi_lsi53c895a.c.ll \
				~/videzzo/lsi53c895a-depend.txt "$libcommon"/instrumented_hw_scsi_lsi53c895a.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_scsi_lsi53c895a.c.o -c "$libcommon"/instrumented_hw_scsi_lsi53c895a.c.ll
			cp "$libcommon"/hw_scsi_lsi53c895a.c.o ../../../
			;;
		"megasas-upstream")
			make "$libcommon"/hw_scsi_megasas.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_scsi_megasas.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_scsi_megasas.c.ll \
				~/videzzo/megasas-depend.txt "$libcommon"/instrumented_hw_scsi_megasas.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_scsi_megasas.c.o -c "$libcommon"/instrumented_hw_scsi_megasas.c.ll
			cp "$libcommon"/hw_scsi_megasas.c.o ../../../
			;;
		"ne2000-upstream")
			make "$libcommon"/hw_net_ne2000-pci.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_net_ne2000-pci.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_net_ne2000-pci.c.ll \
				~/videzzo/ne2000-depend.txt "$libcommon"/instrumented_hw_net_ne2000-pci.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_net_ne2000-pci.c.o -c "$libcommon"/instrumented_hw_net_ne2000-pci.c.ll
			cp "$libcommon"/hw_net_ne2000-pci.c.o ../../../
			;;
		"nvme"|"nvme-2"|"nvme-upstream")
			make "$libcommon"/hw_nvme_ctrl.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_nvme_ctrl.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_nvme_ctrl.c.ll \
				~/videzzo/nvme-depend.txt "$libcommon"/instrumented_hw_nvme_ctrl.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_nvme_ctrl.c.o -c "$libcommon"/instrumented_hw_nvme_ctrl.c.ll
			cp "$libcommon"/hw_nvme_ctrl.c.o ../../../
			;;
		"ohci"|"ohci-2"|"ohci-upstream")
			make "$libcommon"/hw_usb_hcd-ohci-pci.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" \
				./"$libcommon"/hw_usb_hcd-ohci-pci.c.ll
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_usb_hcd-ohci-pci.c.ll \
				~/videzzo/ohci-depend.txt "$libcommon"/instrumented_hw_usb_hcd-ohci-pci.c.ll
			clang -m64 -mcx16 -o "$libcommon"/hw_usb_hcd-ohci-pci.c.o -c "$libcommon"/instrumented_hw_usb_hcd-ohci-pci.c.ll
			cp "$libcommon"/hw_usb_hcd-ohci-pci.c.o ../../../
			;;
		"parallel-upstream")
			make "$libcommon"/hw_char_parallel.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" \
				./"$libcommon"/hw_char_parallel.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_char_parallel.c.ll \
				~/videzzo/parallel-depend.txt "$libcommon"/instrumented_hw_char_parallel.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_char_parallel.c.o -c "$libcommon"/instrumented_hw_char_parallel.c.ll
			cp "$libcommon"/hw_char_parallel.c.o ../../../
			;;
		"pcm3680-can-upstream")
			make "$libcommon"/hw_net_can_can_pcm3680_pci.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_net_can_can_pcm3680_pci.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_net_can_can_pcm3680_pci.c.ll \
				~/videzzo/pcm3680-can-depend.txt "$libcommon"/instrumented_hw_net_can_can_pcm3680_pci.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_net_can_can_pcm3680_pci.c.o -c "$libcommon"/instrumented_hw_net_can_can_pcm3680_pci.c.ll
			cp "$libcommon"/hw_net_can_can_pcm3680_pci.c.o ../../../
			;;
		"pcnet-upstream")
			make "$libcommon"/hw_net_pcnet-pci.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_net_pcnet-pci.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_net_pcnet-pci.c.ll \
				~/videzzo/pcnet-depend.txt "$libcommon"/instrumented_hw_net_pcnet-pci.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_net_pcnet-pci.c.o -c "$libcommon"/instrumented_hw_net_pcnet-pci.c.ll
			cp "$libcommon"/hw_net_pcnet-pci.c.o ../../../
			;;
		"qxl-upstream")
			make "$libcommon"/hw_display_qxl.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_display_qxl.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_display_qxl.c.ll \
				~/videzzo/qxl-depend.txt "$libcommon"/instrumented_hw_display_qxl.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_display_qxl.c.o -c "$libcommon"/instrumented_hw_display_qxl.c.ll
			cp "$libcommon"/hw_display_qxl.c.o ../../../
			;;
		"rocker-upstream")
			make "$libcommon"/hw_net_rocker_rocker.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_net_rocker_rocker.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_net_rocker_rocker.c.ll \
				~/videzzo/rocker-depend.txt "$libcommon"/instrumented_hw_net_rocker_rocker.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_net_rocker_rocker.c.o -c "$libcommon"/instrumented_hw_net_rocker_rocker.c.ll
			cp "$libcommon"/hw_net_rocker_rocker.c.o ../../../
			;;
		"rtl8139-upstream")
			make "$libcommon"/hw_net_rtl8139.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_net_rtl8139.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_net_rtl8139.c.ll \
				~/videzzo/rtl8139-depend.txt "$libcommon"/instrumented_hw_net_rtl8139.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_net_rtl8139.c.o -c "$libcommon"/instrumented_hw_net_rtl8139.c.ll
			cp "$libcommon"/hw_net_rtl8139.c.o ../../../
			;;
		"sdhci"|"sdhci-upstream")
			make "$libcommon"/hw_sd_sdhci-pci.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_sd_sdhci-pci.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_sd_sdhci-pci.c.ll \
				~/videzzo/sdhci-v3-depend.txt "$libcommon"/instrumented_hw_sd_sdhci-pci.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_sd_sdhci-pci.c.o -c "$libcommon"/instrumented_hw_sd_sdhci-pci.c.ll
			cp "$libcommon"/hw_sd_sdhci-pci.c.o ../../../
			;;
		"smc91c111"|"smc91c111-2"|"smc91c111-3"|"smc91c111-4"|"smc91c111-upstream")
			make "$libcommon"/hw_net_smc91c111.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_net_smc91c111.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_net_smc91c111.c.ll \
				~/videzzo/smc91c111-depend.txt "$libcommon"/instrumented_hw_net_smc91c111.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_net_smc91c111.c.o -c "$libcommon"/instrumented_hw_net_smc91c111.c.ll
			cp "$libcommon"/hw_net_smc91c111.c.o ../../../
			;;
		"std-vga"|"std-vga-upstream")
			make "$libcommon"/hw_display_vga-mmio.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_display_vga-mmio.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_display_vga-mmio.c.ll \
				~/videzzo/std-vga-depend.txt "$libcommon"/instrumented_hw_display_vga-mmio.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_display_vga-mmio.c.o -c "$libcommon"/instrumented_hw_display_vga-mmio.c.ll
			cp "$libcommon"/hw_display_vga-mmio.c.o ../../../
			;;
		"stellaris-enet-upstream")
			make "$libcommon"/hw_net_stellaris_enet.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_net_stellaris_enet.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_net_stellaris_enet.c.ll \
				~/videzzo/stellaris-enet-depend.txt "$libcommon"/instrumented_hw_net_stellaris_enet.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_net_stellaris_enet.c.o -c "$libcommon"/instrumented_hw_net_stellaris_enet.c.ll
			cp "$libcommon"/hw_net_stellaris_enet.c.o ../../../
			;;
		"tulip-upstream")
			make "$libcommon"/hw_net_tulip.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_net_tulip.c.ll
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_net_tulip.c.ll \
				~/videzzo/tulip-depend.txt "$libcommon"/instrumented_hw_net_tulip.c.ll
			clang -m64 -mcx16 -o "$libcommon"/hw_net_tulip.c.o -c "$libcommon"/instrumented_hw_net_tulip.c.ll
			cp "$libcommon"/hw_net_tulip.c.o ../../../
			;;
		"uhci-upstream")
			make "$libcommon"/hw_usb_hcd-uhci.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_usb_hcd-uhci.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_usb_hcd-uhci.c.ll \
				~/videzzo/uhci-depend.txt "$libcommon"/instrumented_hw_usb_hcd-uhci.c.ll
			clang -m64 -mcx16 -o "$libcommon"/hw_usb_hcd-uhci.c.o -c "$libcommon"/instrumented_hw_usb_hcd-uhci.c.ll
			cp "$libcommon"/hw_usb_hcd-uhci.c.o ../../../
			;;
		"vmware-svga-upstream")
			make "$libcommon"/hw_display_vmware-vga.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_display_vmware-vga.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_display_vmware-vga.c.ll \
				~/videzzo/vmware-svga-depend.txt "$libcommon"/instrumented_hw_display_vmware-vga.c.ll
			clang -m64 -mcx16 -o "$libcommon"/hw_display_vmware-vga.c.o -c "$libcommon"/instrumented_hw_display_vmware-vga.c.ll
			cp "$libcommon"/hw_display_vmware-vga.c.o ../../../
			;;
		"xgmac-upstream")
			make "$libcommon"/hw_net_xgmac.c.ll
			../../../pass/get_device_module.sh ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" ./"$libcommon"/hw_net_xgmac.c.ll &> log.txt
			../../../pass/instrument.sh "$platform" ../../../pass/Kekule.so ~/videzzo/videzzo_qemu/qemu/"$build_dir" "$libcommon"/hw_net_xgmac.c.ll \
				~/videzzo/xgmac-depend.txt "$libcommon"/instrumented_hw_net_xgmac.c.ll &> error.txt
			clang -m64 -mcx16 -o "$libcommon"/hw_net_xgmac.c.o -c "$libcommon"/instrumented_hw_net_xgmac.c.ll
			cp "$libcommon"/hw_net_xgmac.c.o ../../../
			;;
		*)
			echo "Not supported target"
			;;
	esac
	if [[ "$mode" == "san" ]]
	then
		if [[ "$version" == "esp1" || "$version" == "esp1-fpe" || "$version" == "sdhci" || "$version" == "nvme" || "$version" == "e1000" ]]
		then
			CC=clang CXX=clang++ ../configure \
				--enable-videzzo --enable-fuzzing \
				--disable-werror $sanitizer_option --enable-spice \
				--enable-slirp --disable-download $less_crashes \
				--disable-gtk --disable-sdl \
				--target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		else
			CC=clang CXX=clang++ ../configure \
				--enable-videzzo --enable-fuzzing \
				--disable-werror $sanitizer_option --enable-spice \
				--enable-slirp $less_crashes \
				--disable-gtk --disable-sdl \
				--target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		fi
	elif [[ "$mode" == "dep" ]]
	then
		if [[ "$version" == "esp1" || "$version" == "esp1-fpe" || "$version" == "sdhci" || "$version" == "nvme" || "$version" == "e1000" ]]
		then
			CC=clang CXX=clang++ ../configure \
				--enable-videzzo --enable-fuzzing \
				--disable-werror $sanitizer_option --enable-spice \
				--enable-slirp --disable-download \
				--extra-cflags="-DVIDEZZO_LESS_CRASHES" \
				--disable-gtk --disable-sdl \
				--target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		else
			CC=clang CXX=clang++ ../configure \
				--enable-videzzo --enable-fuzzing \
				--disable-werror $sanitizer_option --enable-spice \
				--enable-slirp \
				--extra-cflags="-DVIDEZZO_LESS_CRASHES" \
				--disable-gtk --disable-sdl \
				--target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		fi
	elif [[ "$mode" == "cov" ]]
	then
		if [[ "$version" == "esp1" || "$version" == "esp1-fpe" || "$version" == "sdhci" || "$version" == "nvme" || "$version" == "e1000" ]]
		then
			CLANG_COV_DUMP=1 \
			CC=clang CXX=clang++ ../configure \
			    --enable-videzzo --enable-fuzzing \
			    --disable-werror $dis_sanitizer_option --enable-spice \
			    --enable-slirp --disable-download \
			    --extra-cflags="-DCLANG_COV_DUMP -DVIDEZZO_LESS_CRASHES -fprofile-instr-generate -fcoverage-mapping" \
				--disable-gtk --disable-sdl \
			    --target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		else
			CLANG_COV_DUMP=1 \
			CC=clang CXX=clang++ ../configure \
			    --enable-videzzo --enable-fuzzing \
			    --disable-werror $dis_sanitizer_option --enable-spice \
			    --enable-slirp \
			    --extra-cflags="-DCLANG_COV_DUMP -DVIDEZZO_LESS_CRASHES -fprofile-instr-generate -fcoverage-mapping" \
				--disable-gtk --disable-sdl \
			    --target-list="i386-softmmu x86_64-softmmu arm-softmmu aarch64-softmmu"
		fi
	fi
	case "$version" in
		"upstream")
			;;
		"ac97-upstream")
			link_device x86_64 hw_audio_ac97.c.o
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-ac97
			;;
		"acpi-upstream")
			link_device x86_64 hw_acpi_erst.c.o
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-acpi-erst
			;;
		"ahci-hd-upstream")
			link_device x86_64 hw_ide_ahci.c.o
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-ahci-hd
			;;
		"ati1"|"ati2"|"ati-upstream")
			link_device x86_64 hw_display_ati.c.o
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-ati
			;;
		"cirrus-vga"|"cirrus-vga-upstream")
			link_device x86_64 hw_display_cirrus_vga.c.o
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-cirrus-vga
			;;
		"cs4231-upstream")
			link_device x86_64 hw_audio_cs4231.c.o
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-cs4231
			;;
		"cs4231a-upstream")
			link_device x86_64 hw_audio_cs4231a.c.o
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-cs4231a
			;;
		"ctu-can-upstream")
			link_device x86_64 hw_net_can_ctucan_pci.c.o
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-ctu-can
			;;
		"ehci-upstream")
			link_device x86_64 hw_usb_hcd-ehci-pci.c.o
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-ehci
			;;
		"es1370-upstream")
			link_device x86_64 hw_audio_es1370.c.o
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-es1370
			;;
		"esp1"|"esp1-fpe"|"esp3"|"esp4"|"esp5"|"esp-upstream")
			link_device x86_64 hw_scsi_esp-pci.c.o
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-am53c974
			;;
		"fdc-upstream")
			make qemu-videzzo-x86_64 -j4
			cp ../../../hw_block_fdc-sysbus.c.o ./"$libcommon"/hw_block_fdc.c.o
			ninja -d keeprsp -n qemu-videzzo-x86_64
			clang -m64 -mcx16 @qemu-videzzo-x86_64.rsp
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-fdc
			;;
		"imx-usb-phy-upstream")
			make qemu-videzzo-arm -j4
			cp ../../../hw_usb_imx-usb-phy.c.o ./"$libcommon"/
			link_to_binary arm "$libcommon"/hw_usb_imx-usb-phy.c.o "$fuzzer"
			ln -f qemu-videzzo-arm ../../"$build_dir"/qemu-videzzo-arm-target-videzzo-fuzz-imx-usb-phy
			;;
		"kvaser-can-upstream")
			make qemu-videzzo-x86_64 -j4
			cp ../../../hw_net_can_can_kvaser_pci.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_net_can_can_kvaser_pci.c.o "$fuzzer"
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-kvaser-can
			;;
		"lan9118"|"lan9118-2"|"lan9118-upstream")
			make qemu-videzzo-arm -j4
			cp ../../../hw_net_lan9118.c.o ./"$libcommon"/
			link_to_binary arm "$libcommon"/hw_net_lan9118.c.o "$fuzzer"
			ln -f qemu-videzzo-arm ../../"$build_dir"/qemu-videzzo-arm-target-videzzo-fuzz-lan9118
			;;
		"lsi53c895a"|"lsi53c895a-upstream")
			make qemu-videzzo-x86_64 -j4
			cp ../../../hw_scsi_lsi53c895a.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_scsi_lsi53c895a.c.o "$fuzzer"
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-lsi53c895a
			;;
		"megasas-upstream")
			make qemu-videzzo-x86_64 -j4
			cp ../../../hw_scsi_megasas.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_scsi_megasas.c.o "$fuzzer"
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-megasas
			;;
		"ne2000-upstream")
			link_device x86_64 hw_net_ne2000-pci.c.o
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-ne2000
			;;
		"nvme"|"nvme-2"|"nvme-upstream")
			make qemu-videzzo-x86_64 -j4
			cp ../../../hw_nvme_ctrl.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_nvme_ctrl.c.o "$fuzzer"
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-nvme
			;;
		"ohci"|"ohci-2"|"ohci-upstream")
			make qemu-videzzo-x86_64 -j4
			cp ../../../hw_usb_hcd-ohci-pci.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_usb_hcd-ohci-pci.c.o "$fuzzer"
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-ohci
			;;
		"parallel-upstream")
			make qemu-videzzo-x86_64 -j4
			cp ../../../hw_char_parallel.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_char_parallel.c.o "$fuzzer"
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-parallel
			;;
		"pcm3680-can-upstream")
			make qemu-videzzo-x86_64 -j4
			cp ../../../hw_net_can_can_pcm3680_pci.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_net_can_can_pcm3680_pci.c.o "$fuzzer"
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-pcm3680-can
			;;
		"pcnet-upstream")
			make qemu-videzzo-x86_64 -j4
			cp ../../../hw_net_pcnet-pci.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_net_pcnet-pci.c.o "$fuzzer"
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-pcnet
			;;
		"qxl-upstream")
			make qemu-videzzo-x86_64 -j4
			cp ../../../hw_display_qxl.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_display_qxl.c.o "$fuzzer"
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-qxl
			;;
		"rocker-upstream")
			make qemu-videzzo-x86_64 -j4
			cp ../../../hw_net_rocker_rocker.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_net_rocker_rocker.c.o "$fuzzer"
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-rocker
			;;
		"rtl8139-upstream")
			make qemu-videzzo-x86_64 -j4
			cp ../../../hw_net_rtl8139.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_net_rtl8139.c.o "$fuzzer"
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-rtl8139
			;;
		"sdhci"|"sdhci-upstream")
			make qemu-videzzo-x86_64 -j4
			cp ../../../hw_sd_sdhci-pci.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_sd_sdhci-pci.c.o "$fuzzer"
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-sdhci-v3
			;;
		"smc91c111"|"smc91c111-2"|"smc91c111-3"|"smc91c111-4"|"smc91c111-upstream")
			make qemu-videzzo-arm -j4
			cp ../../../hw_net_smc91c111.c.o ./"$libcommon"/
			link_to_binary arm "$libcommon"/hw_net_smc91c111.c.o "$fuzzer"
			ln -f qemu-videzzo-arm ../../"$build_dir"/qemu-videzzo-arm-target-videzzo-fuzz-smc91c111
			;;
		"std-vga"|"std-vga-upstream")
			make qemu-videzzo-x86_64 -j4
			cp ../../../hw_display_vga-mmio.c.o ./"$libcommon"/hw_display_vga.c.o
			link_to_binary x86_64 "$libcommon"/hw_display_vga.c.o "$fuzzer"
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-std-vga
			;;
		"stellaris-enet-upstream")
			make qemu-videzzo-arm -j4
			cp ../../../hw_net_stellaris_enet.c.o ./"$libcommon"/
			link_to_binary arm "$libcommon"/hw_net_stellaris_enet.c.o "$fuzzer"
			ln -f qemu-videzzo-arm ../../"$build_dir"/qemu-videzzo-arm-target-videzzo-fuzz-stellaris-enet
			;;
		"tulip-upstream")
			make qemu-videzzo-x86_64 -j4
			cp ../../../hw_net_tulip.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_net_tulip.c.o "$fuzzer"
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-tulip
			;;
		"uhci-upstream")
			make qemu-videzzo-x86_64 -j4
			cp ../../../hw_usb_hcd-uhci.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_usb_hcd-uhci.c.o "$fuzzer"
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-uhci
			;;
		"vmware-svga-upstream")
			make qemu-videzzo-x86_64 -j4
			cp ../../../hw_display_vmware-vga.c.o ./"$libcommon"/
			link_to_binary x86_64 "$libcommon"/hw_display_vmware-vga.c.o "$fuzzer"
			ln -f qemu-videzzo-x86_64 ../../"$build_dir"/qemu-videzzo-x86_64-target-videzzo-fuzz-vmware-svga
			;;
		"xgmac-upstream")
			make qemu-videzzo-arm -j4
			cp ../../../hw_net_xgmac.c.o ./"$libcommon"/
			link_to_binary arm "$libcommon"/hw_net_xgmac.c.o "$fuzzer"
			ln -f qemu-videzzo-arm ../../"$build_dir"/qemu-videzzo-arm-target-videzzo-fuzz-xgmac
			;;
		*)
			echo "Not supported target"
			;;
	esac
	cd ../../../
fi
tmux
