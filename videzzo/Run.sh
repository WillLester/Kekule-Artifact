#!/bin/bash
vanilla="$1"
arch="$2"
target="$3"
mode="$4"
version="$5"
if [[ "$mode" == "san" ]] && [[ "$version" == "-t" ]]
then
    san_time="-max_total_time=86400"
elif [[ "$mode" != "san" ]]
then
    san_time="-max_total_time=86400"
else
    san_time=""
fi
if [ "$mode" == "san" ]; then
	if [[ "$vanilla" == "-v" ]]; then
		if [[ "$version" == "upstream" ]]; then
			UBSAN_OPTIONS=halt_on_error=1:symbolize=1:print_stacktrace=1 ASAN_OPTIONS=detect_leaks=0 \
				DEFAULT_INPUT_MAXSIZE=10000000 ./videzzo_qemu/out-san/qemu-videzzo-"$arch"-target-videzzo-fuzz-"$target" \
				-max_len=10000000 -rss_limit_mb=20480 -detect_leaks=0 corpus &> fuzz-log.txt
		else
			UBSAN_OPTIONS=halt_on_error=1:symbolize=1:print_stacktrace=1 ASAN_OPTIONS=detect_leaks=0 \
				DEFAULT_INPUT_MAXSIZE=10000000 ./videzzo_qemu/out-san/qemu-videzzo-"$arch"-target-videzzo-fuzz-"$target" \
				-max_len=10000000 -rss_limit_mb=20480 -detect_leaks=0 $san_time &> fuzz-log.txt
		fi
	else
		if [[ "$version" == "upstream" ]]; then
			UBSAN_OPTIONS=halt_on_error=1:symbolize=1:print_stacktrace=1 DEPENDENCY_FUZZ=./"$target"-depend.txt \
				ASAN_OPTIONS=detect_leaks=0 DEFAULT_INPUT_MAXSIZE=10000000 \
				./videzzo_qemu/out-san/qemu-videzzo-"$arch"-target-videzzo-fuzz-"$target" -rss_limit_mb=20480 -max_len=10000000 \
				-detect_leaks=0 corpus &> fuzz-log.txt
		else
			UBSAN_OPTIONS=halt_on_error=1:symbolize=1:print_stacktrace=1 DEPENDENCY_FUZZ=./"$target"-depend.txt \
				ASAN_OPTIONS=detect_leaks=0 DEFAULT_INPUT_MAXSIZE=10000000 \
				./videzzo_qemu/out-san/qemu-videzzo-"$arch"-target-videzzo-fuzz-"$target" -rss_limit_mb=20480 -max_len=10000000 \
				-detect_leaks=0 $san_time &> fuzz-log.txt
		fi
	fi
elif [[ "$mode" == "cov" ]]; then
	rm -rf clangcovdump.profraw*
	if [[ "$vanilla" == "-v" ]]; then
		ASAN_OPTIONS=detect_leaks=0 \
			DEFAULT_INPUT_MAXSIZE=10000000 ./videzzo_qemu/out-cov/qemu-videzzo-"$arch"-target-videzzo-fuzz-"$target" \
			-max_len=10000000 -rss_limit_mb=20480 -detect_leaks=0 $san_time -timeout=60 &> fuzz-log.txt
	else
		DEPENDENCY_FUZZ=./"$target"-depend.txt \
			ASAN_OPTIONS=detect_leaks=0 DEFAULT_INPUT_MAXSIZE=10000000 \
			./videzzo_qemu/out-cov/qemu-videzzo-"$arch"-target-videzzo-fuzz-"$target" -rss_limit_mb=20480 -max_len=10000000 \
			-detect_leaks=0 $san_time -timeout=60 &> fuzz-log.txt
	fi
elif [[ "$mode" == "dep" ]]; then
	rm dep-cov.txt
	DEPENDENCY_FUZZ=./"$target"-depend.txt \
		ASAN_OPTIONS=detect_leaks=0 DEFAULT_INPUT_MAXSIZE=10000000 \
		./videzzo_qemu/out-san/qemu-videzzo-"$arch"-target-videzzo-fuzz-"$target" -rss_limit_mb=20480 -max_len=10000000 \
		-detect_leaks=0 $san_time -timeout=120 &> fuzz-log.txt
fi
