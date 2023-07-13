#!/bin/bash

# usage: ./buildshader.sh shaders/main.vert
# working dir: Primrose/

fname=$(basename "$1") # eg. fname=main.vert
name=${fname%.*} # eg. name=main
stage=${fname##*.} # eg. stage=vert

glslc -fshader-stage="$stage" "$1" -o "src/embed/${name}_$stage.spv"
if [[ $? -ne 0 ]] ; then
  exit 1
fi

./embed.sh "src/embed/${name}_$stage.spv" uint8_t

rm "src/embed/${name}_$stage.spv"
