# Kekule Artifact Evaluation Guide

This document describes the complete setup and evaluation workflow for the
Kekule artifact. Run all commands from the artifact root unless a section says
otherwise.

The evaluation consists of four 24-hour experiments (Sections 5.3--5.6), for
an expected total runtime of **4 days** when the independent fuzzing instances
for each experiment are run in parallel. Section 5.7 is collected while
preparing the coverage experiment and does not require another day.

## 1. Install dependencies

The following command targets Ubuntu 24.04:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential gcc-12 g++-12 binutils cmake ninja-build pkg-config git patch \
  bash coreutils findutils grep sed wget ca-certificates tar xz-utils \
  python3 python3-venv python3-pip python3-setuptools python3-wheel python3-tomli \
  libglib2.0-dev zlib1g-dev libpixman-1-dev \
  libspice-server-dev libspice-protocol-dev libslirp-dev libfdt-dev \
  libffi-dev libncurses-dev libxml2-dev libzstd-dev \
  docker.io
```

Enable Docker and verify that it is accessible. If the current user cannot
access Docker directly, use `sudo docker` in the commands below.

```bash
sudo systemctl enable --now docker
docker info
```

## 2. Install the fuzzers

### 2.1 Build the LLVM variants

`install.sh` downloads LLVM 15.0.0 and prepares or builds all six variants:

- Kekule-V and ViDeZZo LLVM source trees for the Docker workflow;
- Kekule-M and Morphuzz host installations;
- the Kekule-M no-priority (`-a`) and path-dependency (`-p`) variants.

```bash
JOBS=8 ./install.sh all
source .llvm/env.sh
```

Reduce `JOBS` if the LLVM build exhausts memory. The default is 2. The command
must be sourced again in every new shell used for the host-based experiments.
The important outputs are:

```text
.llvm/src/kekule-v/          Kekule-V LLVM source for Docker
.llvm/src/videzzo/           ViDeZZo LLVM source for Docker
.llvm/install/kekule-m/      Kekule-M installation
.llvm/install/morphuzz/      Morphuzz installation
.llvm/install/kekule-m-a/    no-priority Kekule-M installation
.llvm/install/kekule-m-p/    path-dependency Kekule-M installation
.llvm/env.sh                  PATH setup for all host variants
```

Individual variants may be installed instead, for example:

```bash
./install.sh kekule-v videzzo
./install.sh kekule-m morphuzz kekule-m-a kekule-m-p
```

### 2.2 Build the Kekule LLVM pass

The host workflow expects `build/pass/Kekule.so`:

```bash
cmake -S . -B build \
  -DLLVM_DIR="$PWD/.llvm/install/kekule-m/lib/cmake/llvm"
cmake --build build --parallel 8
test -f build/pass/Kekule.so
```

### 2.3 Build the ViDeZZo Docker image

The Kekule-V and ViDeZZo experiments run in Docker. Prepare the pinned ViDeZZo
checkout and build its base image:

```bash
mkdir -p third_party
git clone https://github.com/HexHive/ViDeZZo.git third_party/ViDeZZo
git -C third_party/ViDeZZo checkout d698dde482a124863
patch third_party/ViDeZZo/Dockerfile < videzzo/Dockerfile.patch
docker build -t videzzo:latest third_party/ViDeZZo
```

If Docker requires elevated privileges, run the final command with `sudo`.

## 3. Software and hardware requirements

The requirements recorded in `metadata.toml` are:

- **Software:** Ubuntu 24.04 is recommended. The workflow is command-line only
  and requires internet access to download LLVM, ViDeZZo, and QEMU sources.
- **Hardware:** 48 CPU cores, 128 GB RAM, and a 512 GB SSD are recommended.
  More cores allow more independent fuzzing trials to run concurrently.
- **Infrastructure:** no public evaluation infrastructure is provided.

LLVM and QEMU builds are memory- and storage-intensive. Each repetition needs
its own workspace because the setup scripts clone/build QEMU and each run
writes files with fixed names such as `fuzz-log.txt`.

## 4. Common workflow

### 4.1 Terminology and selected targets

The scripts use bug tags rather than paper bug numbers. The complete mapping is
in `Bug-num.md`. The reduced evaluation uses:

| Family | Bug | Setup tag | Architecture | Fuzz target |
| --- | ---: | --- | --- | --- |
| Kekule-V / ViDeZZo | 1 | `esp1` | `x86_64` | `am53c974` |
| Kekule-V / ViDeZZo | 6 | `cirrus-vga` | `x86_64` | `cirrus-vga` |
| Kekule-V / ViDeZZo | 7 | `ati1` | `x86_64` | `ati` |
| Kekule-M / Morphuzz | 1 | `esp1` | `x86_64` | `am53c974` |
| Kekule-M / Morphuzz | 6 | `cirrus-vga` | `x86_64` | `cirrus-vga` |
| Kekule-M / Morphuzz | 19 | `nvme` | `x86_64` | `nvme` |

Use `san` mode for all experiments except coverage (Section 5.4), which uses
`cov`. Passing `-t` to a run command imposes the required 86,400-second limit.

### 4.2 Kekule-M and Morphuzz (host workflow)

Create a fresh workspace for every fuzzer, target, configuration, and
repetition. The initialization interface is:

```text
scripts/qfuzz_init.sh <-n|-v> <arch> <setup-tag> <san|cov|dep> <workspace> [-a|-p]
```

`-n` selects Kekule-M, while `-v` selects Morphuzz. For example, the following
prepares and runs one 24-hour Bug 1 trial for each fuzzer:

```bash
source .llvm/env.sh

./scripts/qfuzz_init.sh -n x86_64 esp1 san "$PWD/runs/kekule-m-esp1-01"
cd runs/kekule-m-esp1-01
/usr/bin/time -p -o elapsed.txt ./qfuzz_run.sh -n x86_64 am53c974 san -t
cd ../..

./scripts/qfuzz_init.sh -v x86_64 esp1 san "$PWD/runs/morphuzz-esp1-01"
cd runs/morphuzz-esp1-01
/usr/bin/time -p -o elapsed.txt ./qfuzz_run.sh -v x86_64 am53c974 san -t
cd ../..
```

Replace the setup tag and fuzz target using the table above. Initialization
clones and compiles QEMU, so it can take substantial time before fuzzing begins.

### 4.3 Kekule-V and ViDeZZo (Docker workflow)

Create a fresh host workspace and container for every trial. The initialization
interface is:

```text
scripts/init_videzzo_docker.sh <-n|-v> <setup-tag> <workspace> <llvm-source>
```

`-n` selects Kekule-V and `-v` selects ViDeZZo. For a Kekule-V Bug 1 trial:

```bash
./scripts/init_videzzo_docker.sh \
  -n esp1 "$PWD/runs/kekule-v-esp1-01" "$PWD/.llvm/src/kekule-v"
docker ps
docker exec -it <container-id> /bin/bash
```

Then, inside the container:

```bash
cd /root/videzzo
./Init.sh -n esp1 san 2>&1 | tee init-log.txt
/usr/bin/time -p -o elapsed.txt ./Run.sh -n x86_64 am53c974 san -t
```

For the ViDeZZo baseline, use `-v` and the vanilla LLVM source:

```bash
./scripts/init_videzzo_docker.sh \
  -v esp1 "$PWD/runs/videzzo-esp1-01" "$PWD/.llvm/src/videzzo"
docker ps
docker exec -it <container-id> /bin/bash
```

Inside that container:

```bash
cd /root/videzzo
./Init.sh -v esp1 san 2>&1 | tee init-log.txt
/usr/bin/time -p -o elapsed.txt ./Run.sh -v x86_64 am53c974 san -t
```

Substitute the selected setup tag, architecture, and target for other trials.
The host workspace is bind-mounted at `/root/videzzo`, so logs remain available
on the host after the container stops. If Docker is only accessible through
`sudo`, use `sudo docker ps` and `sudo docker exec`.

## 5. Experiments and four-day schedule

### Day 1: Section 5.3 -- bug-detection time

Run Kekule-V against ViDeZZo on Bugs 1, 6, and 7, and Kekule-M against Morphuzz
on Bugs 1, 6, and 19. Use `san` mode and the workflows in Section 4.

Run 10 independent trials for every bug/fuzzer pair; five trials are an
acceptable reduced evaluation. Give every trial a unique workspace and run the
trials in parallel when resources permit. Every run must include `-t`, giving
it a maximum duration of 24 hours.

For each trial, retain:

- `fuzz-log.txt`, containing libFuzzer progress and any sanitizer failure;
- `elapsed.txt`, containing wall-clock time from `/usr/bin/time`;
- any generated crashing input (`crash-*` or a similarly named artifact).

For a detected bug, use the final reported time or the wall-clock time at which
the process exits. Treat a trial that reaches 86,400 seconds without the target
bug as a timeout. Report each trial, the arithmetic mean detection time for
successful trials, and the number of timeouts. Do not report a timeout as a
successful 24-hour detection.

The expected error message for each bug is listed in Table 1 of the paper. Use
that table to confirm that a sanitizer failure corresponds to the intended bug
before recording its detection time.

### Day 2: Section 5.4 -- coverage

Run four independent 24-hour instances for each of AM53C974, CIRRUS-VGA, and
LAN9118, for Kekule and its corresponding baseline. Use `cov` mode.

| Device | Docker initialization tag | `Init.sh` / host setup tag | Arch | Target |
| --- | --- | --- | --- | --- |
| AM53C974 | `esp-cov` | `esp-upstream` | `x86_64` | `am53c974` |
| CIRRUS-VGA | `cirrus-vga-cov` | `cirrus-vga-upstream` | `x86_64` | `cirrus-vga` |
| LAN9118 | `lan9118` | `lan9118-upstream` | `arm` | `lan9118` |

Use the same setup/run sequence as Section 4, replacing `san` with `cov`. For
example, the Docker-side commands for AM53C974 are:

```bash
./Init.sh -n esp-upstream cov 2>&1 | tee init-log.txt
./Run.sh -n x86_64 am53c974 cov -t
```

The runs produce `clangcovdump.profraw-*` profiles and `fuzz-log.txt`. Convert
the profiles to a time/coverage series with `scripts/gen_cov_data.py` (a copy is
also placed in each initialized workspace). The path to the profile directory
must end in `/`. For the ViDeZZo AM53C974 example from `experiments.md`:

```bash
python3 gen_cov_data.py ./ qemu am53c974 \
  ./videzzo_qemu/out-cov/qemu-videzzo-x86_64-target-videzzo-fuzz-am53c974 \
  ./am53c974-cov.txt videzzo
```

Repeat this for each device, fuzzer, and trial, changing the binary path and
output filename as appropriate. Preserve the generated `*-cov.txt` files; each
line contains elapsed seconds and branch-coverage percentage.

While preparing the instrumented CIRRUS-VGA build, `videzzo/Init.sh` prints:

```text
instrument.sh (cirrus-vga-upstream) elapsed time: <seconds> seconds
```

Save this line from `init-log.txt`; it is the Section 5.7 static-analysis
scalability output. Section 5.7 requires no separate fuzzing run.

### Day 3: Section 5.5 -- no-priority ablation

Repeat the Section 5.3 Kekule trials without seed prioritization. Baseline
results from Day 1 can be reused.

- For Kekule-V, append `-a` to `Init.sh`:

  ```bash
  ./Init.sh -n esp1 san -a
  ./Run.sh -n x86_64 am53c974 san -t
  ```

- For Kekule-M, append `-a` to `qfuzz_init.sh`; the setup script selects the
  `clang-a` compiler installed in Section 2:

  ```bash
  ./scripts/qfuzz_init.sh \
    -n x86_64 esp1 san "$PWD/runs/kekule-m-a-esp1-01" -a
  cd runs/kekule-m-a-esp1-01
  /usr/bin/time -p -o elapsed.txt ./qfuzz_run.sh -n x86_64 am53c974 san -t
  ```

Collect `fuzz-log.txt`, `elapsed.txt`, crash inputs, detection times, and
timeouts exactly as on Day 1, then compare the ablation with both full Kekule
and the baselines.

### Day 4: Section 5.6 -- path-level dependency feedback

Repeat the Section 5.3 Kekule trials with path-level dependency feedback.
Baseline results from Day 1 can again be reused.

- For Kekule-V, append `-p` to `Init.sh`:

  ```bash
  ./Init.sh -n esp1 san -p
  ./Run.sh -n x86_64 am53c974 san -t
  ```

- For Kekule-M, append `-p` to `qfuzz_init.sh`; the setup script selects the
  `clang-p` compiler:

  ```bash
  ./scripts/qfuzz_init.sh \
    -n x86_64 esp1 san "$PWD/runs/kekule-m-p-esp1-01" -p
  cd runs/kekule-m-p-esp1-01
  /usr/bin/time -p -o elapsed.txt ./qfuzz_run.sh -n x86_64 am53c974 san -t
  ```

This configuration may run out of memory. Preserve the log and record an OOM
as an OOM outcome rather than a timeout or successful detection.

## 6. Outputs to report

The experiment outputs specified by `experiments.md` are:

| Section | Required output |
| --- | --- |
| 5.3 | Per-trial detection time or timeout from `fuzz-log.txt`/`elapsed.txt`; mean successful detection time and timeout count for each fuzzer/bug pair |
| 5.4 | Per-trial `*-cov.txt` time/branch-coverage series for AM53C974, CIRRUS-VGA, and LAN9118 |
| 5.5 | Detection times and timeouts for the no-priority variants, compared with Section 5.3 |
| 5.6 | Detection times, timeouts, and any OOM outcome for the path-dependency variants |
| 5.7 | The CIRRUS-VGA `instrument.sh` elapsed-time line from `init-log.txt` |

Keep the raw logs and profiles as well as summaries so that every reported
value can be traced back to an individual run.

## 7. Expected results

The expectations recorded in `metadata.toml` are qualitative:

1. Kekule should have a smaller average bug-detection time than the baselines,
   or fewer trials that time out.
2. For some devices, Kekule and the baselines should have similar code
   coverage; a clear coverage increase is not expected everywhere.
3. Removing seed prioritization should reduce Kekule's benefit, so the
   no-priority variants should show little efficiency improvement over the
   baselines.
4. Path-level dependency feedback should increase detection time or cause an
   out-of-memory failure.
5. Static analysis should scale to devices with many edges; CIRRUS-VGA analysis
   is expected to finish in minutes.

These are expected trends, not exact pass/fail numbers. Report the measurements
from the current evaluation even when an individual run differs.

## 8. Limitations and troubleshooting

Fuzzing is random, so bug-detection time is inherently non-deterministic. A bug
may be found at a different time, or not found within 24 hours, in different
runs.

If launching a fuzzer immediately produces a segmentation fault and an empty
log, retry that trial in a fresh process; this is a known random startup error.
Do not replace a non-empty crash report without first preserving it.

If a ViDeZZo build does not produce the requested `x86_64` target, recreate the
workspace/container and repeat initialization. ViDeZZo can occasionally omit
that target without a clear diagnostic.

The path-level experiment can exhaust memory. Record that outcome as part of
the experiment rather than repeatedly discarding it.
