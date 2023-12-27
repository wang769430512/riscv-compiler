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

assert 0 '{ return 0; }'
assert 41 '{ 41; }'
# assert 34 12-34+56
assert 41 '{ 12 + 34 - 5; }'
assert 26  '{ 3 * 9 - 5/5; }'
assert 17 '{1-8/(2*2)+3*6;}'
assert 1 '{ -2----3; }'
assert 1 '{ 1!=2; }'
assert 1 '{ 2>=1; }'
assert 1 '{ 1<=100; }'
assert 3 '{ 1;2;3; }'
assert 12 '{ 9*9;100-101;76/6; }'
# assert 2 '99;'

# [10] 支持单字母变量
assert 3 '{ a=3; a; }'
assert 5 '{ a=3; z=5; }'
assert 6 '{ a=b=3; a+b; }'
assert 5 '{ a=3;b=4;a=1;a+b; }'
assert 4 '{ a=3; a+ 1; }'

# [11] 支持多字母变量
assert 3 '{ foo=3; foo; }'
assert 74 '{ foo2=70; bar4=4; foo2+bar4; }'

# [12] 支持return
assert 1 '{ return 1; 2; 3; }'
assert 2 '{ 1+2; return 2;3; }'
assert 3 '{ 1; 2; return 3; }'

assert 10 '{ return -10+20; }'

# [13] 支持 {...}
assert 3 '{ 1; 2; 3; }'

# [14]
assert 5 '{ ;;; return 5; }'

# [15]
assert 4 '{ if (0) {3;} else {4;}}'

# [16]
assert 55 '{ i=0; j=0; for (i=0; i<=10; i=i+1) j=i+j; return j; }'
assert 3 '{ for (;;) {return 3;} return 5; }'

# [17] while
assert 10 '{ i=0; while(i<10) { i=i+1; } return i;}'

echo OK