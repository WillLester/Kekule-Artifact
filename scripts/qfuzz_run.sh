#!/bin/bash
vanilla="$1"
arch="$2"
target="$3"
mode="$4"
time_limit="$5"


if [[ "$#" -lt 4 ]]; then
	echo "Usage: vanilla (-v or -n) arch, target device, mode (san, cov or dep), time_limit (-t)."
	exit 1
fi

if [[ "$mode" == "san" ]] && [[ "$time_limit" == "-t" ]]
then
    san_time="-max_total_time=172800"
elif [[ "$mode" != "san" ]]
then
    san_time="-max_total_time=86400"
else
    san_time=""
fi

if [[ "$mode" == "cov" ]]
then
    rm -rf clangcovdump.*
elif [[ "$mode" == "dep" ]]
then
    rm dep-cov.txt
fi

if [[ "$vanilla" == "-v" ]]
then
    if [[ "$mode" == "cov" ]]
    then
        rm -rf clangcovdump.*
        UBSAN_OPTIONS=halt_on_error=1:symbolize=1:print_stacktrace=1 ASAN_OPTIONS=detect_leaks=0 \
            DEFAULT_INPUT_MAXSIZE=10000000 ./qemu/build/qemu-fuzz-"$arch" --fuzz-target=generic-fuzz-"$target" -max_len=10000000 $san_time -rss_limit_mb=20480 -detect_leaks=0 &> fuzz-log.txt
    else
        UBSAN_OPTIONS=halt_on_error=1:symbolize=1:print_stacktrace=1 ASAN_OPTIONS=detect_leaks=0 \
            DEFAULT_INPUT_MAXSIZE=10000000 ./qemu/build/qemu-fuzz-"$arch" --fuzz-target=generic-fuzz-"$target" -max_len=10000000 $san_time -rss_limit_mb=20480 -detect_leaks=0 &> fuzz-log.txt
    fi
else
    if [[ "$mode" == "cov" ]]
    then
        rm -rf clangcovdump.*
        UBSAN_OPTIONS=halt_on_error=1:symbolize=1:print_stacktrace=1 ASAN_OPTIONS=detect_leaks=0 DEPENDENCY_FUZZ=./"$target"-depend.txt \
            DEFAULT_INPUT_MAXSIZE=10000000 ./qemu/build/qemu-fuzz-"$arch" --fuzz-target=generic-fuzz-"$target" -max_len=10000000 $san_time -rss_limit_mb=20480 -detect_leaks=0 &> fuzz-log.txt
    else
        UBSAN_OPTIONS=halt_on_error=1:symbolize=1:print_stacktrace=1 ASAN_OPTIONS=detect_leaks=0 DEPENDENCY_FUZZ=./"$target"-depend.txt \
            DEFAULT_INPUT_MAXSIZE=10000000 ./qemu/build/qemu-fuzz-"$arch" --fuzz-target=generic-fuzz-"$target" -max_len=10000000 $san_time -rss_limit_mb=20480 -detect_leaks=0 &> fuzz-log.txt
    fi
fi
