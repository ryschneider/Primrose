#!/bin/bash

# usage: ./embed.sh scene.schema.json uint8_t
# working dir: Primrose/

fname=$(basename "$1") # eg. fname=scene.schema.json
snake=${fname//\./_} # eg. snake=scene_schema_json
camel=$(sed -r 's/(_)(\w)/\U\2/g' <<< "$snake") # eg. camel=sceneSchemaJson

type=${2:-char} # eg. type=uint8_t

echo "#ifndef ${snake^^}_SPV_H
#define ${snake^^}_SPV_H
#include <cstddef>
${type} ${camel}Data[] = {" > "src/embed/${snake}.h"
xxd -plain "$1" | sed 's/\(.\{2\}\)/0x\1,/g' >> "src/embed/${snake}.h"
echo "};
std::size_t ${camel}Size = sizeof(${camel}Data);
#endif" >> "src/embed/${snake}.h"
