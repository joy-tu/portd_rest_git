#!/bin/bash
cd bin
mkdir -p nng
cd nng
cmake -DCMAKE_C_COMPILER=$1 -DCMAKE_TOOLCHAIN_FILE=../../src/http_server/arm-toolchain.cmake ../../src/http_server/nng/1.1.1
make
mv libnng.a ../
