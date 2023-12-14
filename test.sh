#!/bin/bash

assert() {
    expected="$1"

    input="$2"

    ./rvcc "$input" > tmp.s || exit
    
    riscv64-unknown-linux-gnu-gcc -static -o tmp tmp.s

    qemu-riscv64 -L sysroot tmp

    actual="$?"

    if [ "$expected" = "$actual" ]; then
      echo "$input => $actual"
    else
      echo "$input => $expected expected, but got $actual"
      exit 1
    fi
}

assert 0 0
assert 41 41
# assert 34 12-34+56
assert 41 ' 12 + 34 - 5'
assert 26  '3 * 9 - 5/5'
assert 17 '1-8/(2*2)+3*6'
# assert 2 99

echo OK