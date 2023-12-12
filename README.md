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
➜ build/zero-to-rust-jit build/sum_rs.bc
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

## Debug the JITed bitcode:

[Debug support just landed on mainline](https://github.com/llvm/llvm-project/pull/73257) for Linux and macOS:
```
➜ lldb -- build-mainline/zero-to-rust-jit
(lldb) version
lldb version 15.0.7
(lldb) log enable lldb jit
(lldb) settings show plugin.jit-loader.gdb.enable
(lldb) b sum
Breakpoint 1: no locations (pending).
(lldb) run build-mainline/sum_c.bc
Process 235912 launched (x86_64)
 JITLoaderGDB::SetJITBreakpoint looking for JIT register hook
 JITLoaderGDB::SetJITBreakpoint setting JIT breakpoint
 JITLoaderGDB::JITDebugBreakpointHit hit JIT breakpoint
 JITLoaderGDB::ReadJITDescriptorImpl registering JIT entry at 0x10011c008 (2140 bytes)
1 location added to breakpoint 1
a = 1
b = 2
Process 235912 stopped
* thread #1, name = 'zero-to-rust-jit', stop reason = breakpoint 1.1
    frame #0: 0x000000010011e00e JIT(0x10011c008)`sum(a=1, b=2) at sum.c:4:3
   1    #include <stdio.h>
   2
   3    int sum(int a, int b) {
-> 4      printf("Oh hello, that's JITed code!\n");
   5      return a + b;
   6    }
(lldb) c
Process 235912 resuming
Oh hello, that's JITed code!
1 + 2 = 3
Again? (y/n) n

Process 235912 exited with status = 0
(lldb) q
```
