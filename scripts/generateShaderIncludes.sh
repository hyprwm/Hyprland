#!/bin/sh

SHADERS_SRC="./src/render/shaders/glsl"

echo "-- Generating shader includes"

if [ ! -d ./src/render/shaders ]; then
	mkdir ./src/render/shaders
fi

echo '#pragma once' > ./src/render/shaders/Shaders.hpp
echo '#include <map>' >> ./src/render/shaders/Shaders.hpp
echo 'static const std::map<std::string, std::string> SHADERS = {' >> ./src/render/shaders/Shaders.hpp

for filename in `ls ${SHADERS_SRC}`; do
	echo "--	${filename}"
	
	{ echo 'R"#('; cat ${SHADERS_SRC}/${filename}; echo ')#"'; } > ./src/render/shaders/${filename}.inc
	echo "{\"${filename}\"," >> ./src/render/shaders/Shaders.hpp
	echo "#include \"./${filename}.inc\"" >> ./src/render/shaders/Shaders.hpp
	echo "}," >> ./src/render/shaders/Shaders.hpp
done

echo '};' >> ./src/render/shaders/Shaders.hpp
