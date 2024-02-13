cmake_minimum_required( VERSION 3.10.0 )

project( rvcc )

add_executable( rvcc
  main.c
  string.c
  tokenize.c
  parse.c
  codegen.c
  type.c
)

SET( CMAKE_C_FLAGS "-std=c11 -g -fno-common" )
