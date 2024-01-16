#!/bin/bash

# 将下列代码编译为tmp2.o, "-xc"强制以c语言进行编译
# cat << EOF | $RISCV/bin/riscv64-unknown-linux-gnu-gcc -xc -c -o tmp2.o -
cat << EOF | riscv64-unknown-linux-gnu-gcc -xc -c -o tmp2.o -
int ret3() { return 3; }
int ret5() { return 5; }
int add(int x, int y) { return x + y; }
int sub(int x, int y) { return x - y; }
int add6(int a, int b, int c, int d, int e, int f) {
  return a + b + c + d + e + f;
}
EOF

assert() {
    expected="$1"

    input="$2"

    ./rvcc "$input" > tmp.s || exit
    
    # clang -static -o tmp tmp.s tmp2.o
    riscv64-unknown-linux-gnu-gcc -static -o tmp tmp.s tmp2.o

    qemu-riscv64 -L sysroot tmp

    actual="$?"

    if [ "$expected" = "$actual" ]; then
      echo "$input => $actual"
    else
      echo "$input => $expected expected, but got $actual"
      exit 1
    fi
}

assert 0 'int main() { return 0; }'
assert 41 'int main() { return 41; }'
# assert 34 12-34+56
assert 41 'int main() { return 12 + 34 - 5; }'
assert 26 'int main() { return 3 * 9 - 5/5; }'
assert 17 'int main() { return 1-8/(2*2)+3*6; }'
assert 1 'int main() { return  -2----3; }'
assert 1 'int main() { return 1!=2; }'
assert 1 'int main() { return 2>=1; }'
assert 1 'int main() { return 1<=100; }'
assert 1 'int main() { return 1;2;3; }'
assert 81 'int main() { return 9*9;100-101;76/6; }'
# assert 2 '99;'

# [10] 支持单字母变量
assert 3 'int main() { int a=3; return a; }'
assert 8 'int main() { int a=3, z=5; return a + z;}'
assert 6 'int main() { int a, b; a=b=3; return a+b; }'
assert 5 'int main() { int a=3, b=4;a=1; return a+b; }'
assert 4 'int main() { int a=3; return a + 1; }'

# [11] 支持多字母变量
assert 3 'int main() { int foo=3; return foo; }'
assert 74 'int main() { int foo2=70, bar4=4; return foo2+bar4; }'

# [12] 支持return
assert 1 'int main() { return 1; 2; 3; }'
assert 2 'int main() { 1+2; return 2;3; }'
assert 3 'int main() { 1; 2; return 3; }'

assert 10 'int main() { return -10+20; }'

# [13] 支持 {...}
assert 3 'int main() { 1; 2; return 3; }'

# [14]
assert 5 'int main() { ;;; return 5; }'

# [15]
assert 4 'int main() { if (0) return 3; else return 4;}'

# [16]
assert 55 'int main() { int i=0; int j=0; for (i=0; i<=10; i=i+1) j=i+j; return j; }'
assert 3 'int main() { for (;;) {return 3;} return 5; }'

# [17] while
assert 10 'int main() { int i=0; while(i<10) { i=i+1; } return i;}'

# [20] 支持一元& *运算符
assert 3 'int main() { int x=3; return *&x; }'
assert 3 'int main() { int x=3; int *y=&x; int **z=&y; return **z; }'
assert 5 'int main() { int x=3; int *y=&x; *y=5; return x; }'
assert 7 'int main() { int x=3; int y=5; *(&x+1)=7; return y; }'
assert 7 'int main() { int x=3; int y=5; *(&y-1)=7; return x; }'

# assert 2 '{x=1;y=x+1;return *(&x+1);&*y;}'

# [21] 支持指针的算数运算
assert 3 'int main() { int x=3; int y=5; return *(&y-1); }'

# [22] 支持int关键字
assert 8 'int main() { int x, y; x = 3; y = 5; return x + y;}'
assert 8 'int main() { int x=3, y=5; return x+y; }' 

# [23] 支持零参函数调用
assert 3 'int main() { return ret3(); }'
assert 5 'int main() { return ret5(); }'
assert 8 'int main() { return ret3()+ret5(); }'

# [24] 支持最多6个参数的函数调用
assert 8 'int main() { return add(3, 5); }'
assert 2 'int main() { return sub(5, 3); }'
assert 21 'int main() { return add6(1, 2, 3, 4, 5, 6); }'
assert 33 'int main() { return add6(3, 4, 5, 6, 7, 8); }'
assert 136 'int main() { return add6(1, 2, add6(3, add6(4, 5, 6, 7, 8, 9), 10, 11, 12, 13), 14, 15, 16); }'

# [25] 支持零参数
assert 32 'int main() { return ret32(); } int ret32() { return 32; }'

echo OK