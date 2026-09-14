#!/bin/sh
exit_code=0
#set -x
if [ "$#" -ne 6 ]; then
	echo "usage: script.sh platform llvm_lib_path qemu_build_path target_ll depend_path output_ll"
	exit 1
fi
platform=$1
llvm_lib_path=$2
qemu_build_path=$3
target_ll=$4
depend_path=$5
output_ll=$6
opt -S -load-pass-plugin "$llvm_lib_path" -passes="instrumentation" -platform="$platform" -depend_output_path="$depend_path" "$qemu_build_path/$target_ll" > "$qemu_build_path/$output_ll"
