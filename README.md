# Introduction

For installation and starting fuzzers, please check the steps in this file.

For experiments, please refer to experiments.md.

# Required Dependencies

Check DEPENDENCIES.md to install the packages.

# Install LLVM 15.0.0

Run the installer from the artifact directory to download LLVM 15.0.0, prepare
the sources, and build and install the host compilers:

```bash
./install.sh
source .llvm/env.sh
```

The script prepares Kekule-V and ViDeZZo sources under `.llvm/src/kekule-v`
and `.llvm/src/videzzo`; pass the corresponding directory to
`scripts/init_videzzo_docker.sh`. Their LLVM builds happen inside Docker.

It installs Kekule-M and Morphuzz under `.llvm/install/kekule-m` and
`.llvm/install/morphuzz`, respectively. Links in `.llvm/bin` provide
`clang-n`/`clang++-n` for Kekule-M and `clang`/`clang++` for Morphuzz.

It also installs the Kekule-M ablation and path-level dependency variants under
`.llvm/install/kekule-m-a` and `.llvm/install/kekule-m-p`, providing
`clang-a`/`clang++-a` and `clang-p`/`clang++-p`, respectively.

The default (or `./install.sh all`) includes all six variants. Select individual
variants with, for example, `./install.sh kekule-v videzzo` or
`./install.sh kekule-m-a kekule-m-p`.

Use `JOBS=8 ./install.sh` to change build parallelism (default: 2), and
`LLVM_WORK_DIR` or `LLVM_INSTALL_ROOT` to customize storage and install paths.
Run `./install.sh --help` for prerequisites and options. The ablation and
path-level dependency builds require `patch`.

# Kekule-V

## Prepare ViDeZZo

1. `git clone https://github.com/HexHive/ViDeZZo.git`
2. Enter the ViDeZZo directory and `git checkout d698dde482a124863`
3. Apply Dockerfile.patch under `{artifact_root}/videzzo` to the ViDeZZo directory.
4. Build a ViDeZZo docker through `sudo docker build -t videzzo:latest .`
5. Run `scripts/init_videzzo_docker.sh` with `-n` to initialize a ViDeZZo docker instance.
6. Use `[sudo] docker exec -it {docker_id} /bin/bash` to enter the docker.
7. `cd videzzo` and run Init.sh with `-n`.
8. When the script finishes, run Run.sh with `-n` to start fuzzing.

# ViDeZZo

When Kekule-V is ready, replace `-n` with `-v` in its steps.

Also, pass the LLVM 15.0.0 source for ViDeZZo into the docker when running `init_videzzo_docker.sh`.

# Kekule-M

After running `install.sh` and sourcing `.llvm/env.sh` as above:

1. Run `scripts/qfuzz_init.sh` to initialize a Kekule-M instance in the workspace.
2. Run `qfuzz_run.sh` in the workspace to start fuzzing.

Use `-n` for the default variant, `-a` for ablation, or `-p` for path-level
dependencies when running the initialization and fuzzing scripts.

# Morphuzz

After running `install.sh` and sourcing `.llvm/env.sh` as above:

1. Run `scripts/qfuzz_init.sh` to initialize a Morphuzz instance in the workspace.
2. Run `qfuzz_run.sh` in the workspace to run fuzzing.

