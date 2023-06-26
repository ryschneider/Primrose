#!/bin/bash

glslc -fshader-stage=$1 Primrose/shaders/main.$1 -o Primrose/shaders/$1.spv
if [[ $? -ne 0 ]] ; then
  exit 1
fi

echo "#ifndef ${1^^}_SPV_H
#define ${1^^}_SPV_H
#include <stdint.h>
extern uint8_t $1SpvData[];
extern size_t $1SpvSize;
#endif" > "Primrose/shaders/$1_spv.h"

echo "#include \"$1_spv.h\"
uint8_t $1SpvData[] = {" > "Primrose/shaders/$1_spv.c"
xxd -plain Primrose/shaders/$1.spv | sed 's/\(.\{2\}\)/0x\1,/g' >> "Primrose/shaders/$1_spv.c"
echo "};
size_t $1SpvSize = sizeof($1SpvData);" >> "Primrose/shaders/$1_spv.c"
