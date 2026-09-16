# Introduction

For installation and starting fuzzers, please check the steps in this file.

For experiments, please refer to experiments.md.

# Kekule-V

## Download LLVM 15.0.0

1. Download LLVM-15.0.0 source code.
2. Extract the source to two directories, one for Kekule-V, one for the vanilla ViDeZZo.
3. Copy the files in llvm-project/kekule-v to one LLVM source to get the LLVM for Kekule-V.
4. Copy the files in llvm-project/videzzo to the other LLVM source to get the LLVM for ViDeZZo.

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

## Install LLVM 15.0.0

Since Morphuzz runs locally, installing LLVM for both Kekule-M and Morphuzz is required.

1. Extract LLVM 15.0.0 source code.
2. For Kekule-M, copy the files under llvm-project/kekule-m to the source code.
3. Install LLVM with `cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_CXX_FLAGS="-fconcepts" -DLLVM_ENABLE_PROJECTS="clang;compiler-rt"` in LLVM source directory. The install prefix can be custom. Then `make install`.
4. Link the binaries clang and clang++ to clang-n and clang++-n, and add the binaries' paths to PATH.
5. [optional] To build ablation version, apply  `videzzo/no-priority.patch` to `{LLVM_dir}/compiler-rt/lib/fuzzer/FuzzerLoop.cpp`, and install that version of LLVM. Link the binaries to clang-a and clang++-a.
6. [optional] To build the path-level dependency version, apply the two patches under `{artifact_dir}/llvm-project/kekule-v/compiler-rt/lib/fuzzer/` to the corresponding files in LLVM source code, and install. Link the binaries to clang-p and clang++-p.
7. Run `scripts/qfuzz_init.sh` to initialize a Kekule-M instance in the workspace.
8. Run `qfuzz_run.sh` in the workspace to run fuzzing.

# Morphuzz

1. Install the vanilla LLVM 15.0.0. The binaries should be linked to clang and clang++.
2. Run `scripts/qfuzz_init.sh` to initialize a Morphuzz instance in the workspace.
3. Run `qfuzz_run.sh` in the workspace to run fuzzing.

