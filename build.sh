#!/bin/bash

cmake -GNinja  -Bbuild -S. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DLLVM_DIR=/Users/staefsn/Develop/LLVM/llvm-project/build/lib/cmake/llvm
rm compile_commands.json
ln -s build/compile_commands.json compile_commands.json
ninja -Cbuild
./build/llvm-jit-c build/sum_rs.bc
