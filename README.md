# Kekule-V

## Prepare QEMU

1. Clone ViDeZZo git repo from https://github.com/HexHive/ViDeZZo.git.
2. Use the docker instance provided by ViDeZZo to continue the following steps, copy the artifact into the docker.
3. Download LLVM-15.0.0 source codes.
4. Copy the files in llvm-project/kekule-v to LLVM, install llvm with compiler-rt enabled.
5. Build the passes with cmake and make, it will generate kekule.so.
6. Go to videzzo, run `make qemu-dep` to download QEMU.
7. Run `make patch`.
8. Run `make qemu` to build QEMU.

## Analyze and Run

1. Copy the meson .whl in patches to videzzo\_qemu/qemu/python/wheels.
2. Run `make update-buildoptions`.
3. Configure QEMU with the option --enable-llvm.
4. Use `make` to generate the core file of a device.
5. Use scripts/get\_device\_module.sh to generate the device module.
6. Use scripts/instrument.sh to generate the dependency file and the instrumented .ll.
7. Compile the .ll to .o.
8. Configure QEMU without --enable-llvm, `make` the target qemu-videzzo-[arch].
9. Link the .o of the core file with other files to get the binary.
10. By setting the DEPENDENCY\_FUZZ environmental variable to the path to the dependency file, run the binary following ViDeZZo's guide.

# Kekule-M

The process is similar to Kekule-V. The differences are:
1. It does not need a docker, just download QEMU and Morphuzz is inside.
2. Use llvm-project/kekule-m files instead to install llvm.
3. Skip all ViDeZZo-related steps.
