#!/bin/sh

build/tc -p ${1}/test_${2}.tig -o build/test_${1}_${2} -lsrc/utils/runtime.cpp -i build/test_${1}_${2}.ll