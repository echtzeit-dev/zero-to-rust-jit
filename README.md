# From Zero to Rust JIT with the LLVM C-API

LLVM C-API demo for runtime compilation with OrcJIT. We will start from an [example in upstream LLVM](https://github.com/llvm/llvm-project/blob/release/17.x/llvm/examples/OrcV2Examples/OrcV2CBindingsBasicUsage/OrcV2CBindingsBasicUsage.c) and move on step-by-step:

![steps](2023-zero-to-rust-jit.png)

## Build against LLVM release version

### Linux

Install LLVM 17 with `apt` and build:
```
➜ wget https://apt.llvm.org/llvm.sh
➜ chmod +x llvm.sh
➜ sudo ./llvm.sh 17
➜ sudo apt install -y libzstd-dev
➜ git clone https://github.com/echtzeit-dev/zero-to-rust-jit
➜ rustc --version -v | tail -1
LLVM version: 17.0.4
➜ CC=clang CXX=clang++ cmake -GNinja -Bbuild -S. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DLLVM_DIR=/usr/lib/llvm-17/lib/cmake/llvm
➜ ninja -Cbuild
```

### macOS

Install LLVM 17 with `brew` and build:
```
➜ git clone https://github.com/echtzeit-dev/zero-to-rust-jit
➜ rustc --version -v | tail -1
LLVM version: 17.0.4
➜ brew install llvm@17
➜ brew info llvm@17 | head -1
==> llvm: stable 17.0.6 (bottled), HEAD [keg-only]
➜ ls -lh /usr/local/opt/llvm@17
/usr/local/opt/llvm@17 -> ../Cellar/llvm/17.0.6
➜ CC=clang CXX=clang++ cmake -GNinja -Bbuild -S. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DLLVM_DIR=/usr/local/opt/llvm@17/lib/cmake/llvm
➜ ninja -Cbuild
```

## Run

Pass bitcode for the `sum()` function as first command-line argument:
```
➜ build/zero-to-rust-jit build/sum_c.bc
a = 1
b = 2
Oh hello, that's JITed code!
1 + 2 = 3
Again? (y/n) n
```

## Build against LLVM mainline

Build mainline LLVM from source. This will take a while. We are using the C-API so we can easily switch to `Release` mode if we don't need to debug into LLVM (doesn't affect debugging of JITed code):
```
➜ git clone https://github.com/llvm/llvm-project
➜ cmake -GNinja -Sllvm-project/llvm -Bllvm-project/build \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DLLVM_TARGETS_TO_BUILD=host \
        -DLLVM_BUILD_LLVM_DYLIB=On \
        -DLLVM_USE_LINKER=lld
➜ cd llvm-project/build
➜ ninja LLVM
➜ export LLVM_MAINLINE=$(pwd)
```

Build the project against the just-built LLVM with the rustc version check disabled:
```
➜ git clone https://github.com/echtzeit-dev/zero-to-rust-jit
➜ cd zero-to-rust-jit
➜ rustc --version -v | tail -1
LLVM version: 17.0.4
➜ CC=clang CXX=clang++ cmake -GNinja -Bbuild-mainline -S. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=On \
    -DLLVM_DIR=$LLVM_MAINLINE/lib/cmake/llvm
➜ ninja -Cbuild-mainline
```

## Transpile the C code to Rust
```
cmake .. -D LLVM_DIR=/usr/lib/llvm-17/lib/cmake/llvm -DCMAKE_EXPORT_COMPILE_COMMANDS=1
c2rust transpile compile_commands.json
```

## Build and run the Rust version

### Linux
```
➜ cargo run build/sum_c.bc
```

### macOS
```
➜ LLVM_SYS_170_PREFIX=/usr/local/opt/llvm@17 cargo run build/prime_factors_rs.bc
   Compiling zero-to-rust-jit v0.1.0
    Finished dev [unoptimized + debuginfo] target(s) in 1.25s
     Running `target/debug/zero-to-rust-jit build/prime_factors_rs.bc`
n = 12
prime_factors(12) = [2, 2, 3]
Again? (Y/n)
```
