# Mapping between bug numbers and tags

Since all scripts use bug tags instead of bug numbers, please refer to Bug-num.md to get their tags.

Some bugs have the same tag as they are triggerd at the same commit.

# Selected bugs

To scale down the experiments, we use bug 1, 6, 7 for Kekule-V and ViDeZZo experiments, 1, 6, 19 for Kekule-M and Morphuzz experiments.

All experiments should be completed in 24 hours.

# Mode selection

Except 5.4, all other experiments should use `san` mode. Coverage experiments should use `cov`.

# 5.3

Run 10 (or fewer like 5) instances each bug for each fuzzer. Find the last log that prints the time or use `time` to get the detection time.

# 5.4

Run 4 instances for AM53C974. CIRRUS-VGA, and LAN9118.

When running `Init.sh` and `qfuzz_init.sh`, use mode `cov`.

For bug tags, use `esp-cov, cirrus-vga-cov, lan9118` for `init_videzzo_docker.sh`, `esp-upstream, cirrus-vga-upstream, lan9118-upstream` for `Init.sh`.

When finished, use `scripts/gen_cov_data.py` to get the summary of coverage of each device
(e.g. `python3 gen_cov_data.py ./ qemu am53c974 ./videzzo_qemu/out-cov/qemu-videzzo-x86_64-target-videzzo-fuzz-am53c974 ./am53c974-cov.txt videzzo`,
you will see the result in am53c974-cov.txt).

# 5.5

For Kekule-V, use `-a` as the extra option for `videzzo/Init.sh`. For Kekule-M, follow README.md and build clang-a.
When running `qfuzz_init.sh`, use `-a` as the extra option.

Then use the same method as in 5.3 to get the detection time, and compare it against the result in 5.3.

# 5.6

Use `-p` as the extra option in building Kekule-V. For Kekule-M, follow README.md and build clang-p.
When running `qfuzz_init.sh`, use `-p` as the extra option.

Then test as that in 5.3. It can trigger out-of-memory.

# 5.7

This does not require an extra experiment. When running videzzo/Init.sh on cirrus-vga, it will print the elapsed time for running `instrument.sh`.
