#!/bin/bash

# usage: ./buildshader.sh Primrose/shaders/main.vert

dir=${1%/*} # eg. Primrose/shaders
fname=$(basename "$1") # eg. fname=main.vert
name=${fname%.*} # eg. name=main
stage=${fname##*.} # eg. stage=vert

glslc -fshader-stage="$stage" "$1" -o "$dir/../embed/${name}_$stage.spv"
if [[ $? -ne 0 ]] ; then
  exit 1
fi

#echo "#ifndef ${name^^}_${stage^^}_SPV_H
##define ${name^^}_${stage^^}_SPV_H
##include <stdint.h>
#extern uint8_t $name${stage^}SpvData[];
#extern size_t $name${stage^}SpvSize;
##endif" > "$dir/../embed/${name}_${stage}_spv.h"
#
#echo "#include \"${name}_${stage}_spv.h\"
#uint8_t $name${stage^}SpvData[] = {" > "$dir/../embed/${name}_${stage}_spv.c"
#xxd -plain "$dir/../embed/${name}_$stage.spv" | sed 's/\(.\{2\}\)/0x\1,/g' >> "$dir/../embed/${name}_${stage}_spv.c"
#echo "};
#size_t $name${stage^}SpvSize = sizeof($name${stage^}SpvData);" >> "$dir/../embed/${name}_${stage}_spv.c"

echo "#ifndef ${name^^}_${stage^^}_SPV_H
#define ${name^^}_${stage^^}_SPV_H
uint8_t $name${stage^}SpvData[] = {" > "$dir/../embed/${name}_${stage}_spv.h"
xxd -plain "$dir/../embed/${name}_$stage.spv" | sed 's/\(.\{2\}\)/0x\1,/g' >> "$dir/../embed/${name}_${stage}_spv.h"
echo "};
size_t $name${stage^}SpvSize = sizeof($name${stage^}SpvData);
#endif" >> "$dir/../embed/${name}_${stage}_spv.h"

rm "$dir/../embed/${name}_$stage.spv"
