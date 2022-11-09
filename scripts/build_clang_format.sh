#!/bin/bash

base_path=$(dirname $(realpath "$0"))

cd ${base_path}

if [ -f clang_build/bin/clang-format ]; then
  exit
fi

git clone --depth 1 https://github.com/llvm/llvm-project.git clang_build/source
cd clang_build/source
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${base_path}/clang_build" -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" ../llvm
cmake --build . --target install
