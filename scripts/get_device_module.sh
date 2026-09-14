#!/bin/sh
exit_code=0
#set -x
if [ "$#" -ne 3 ]; then
	echo "usage: script.sh llvm_lib_path qemu_build_path target_ll"
	exit 1
fi
qemu_build_path=$2
rm "$qemu_build_path"/related-files.txt
while [ $exit_code -ne 1 ]; do
	opt -load-pass-plugin $1 -passes=devicemodule -qemu_build_path=$qemu_build_path -llvm_lib_path=$1 $3 -disable-output
	chmod +x "$qemu_build_path"/device-module.sh
	"$qemu_build_path"/device-module.sh
	exit_code=$?
done
rm "$qemu_build_path"/device-module.sh
rm "$qemu_build_path"/linked-files.txt
