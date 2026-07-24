#!/bin/sh

SHADERS_SRC="./src/render/shaders/glsl"

echo "-- Generating shader includes"

if [ ! -d ./src/render/shaders ]; then
	mkdir ./src/render/shaders
fi

{
	echo '#pragma once'
	echo '#include <algorithm>'
	echo '#include <array>'
	echo '#include <string_view>'
	echo '#include <utility>'
	echo 'inline constexpr auto SHADERS = [](auto shaders) {'
	echo 'std::ranges::sort(shaders, {}, [](auto pair) { return pair.first; });'
	echo 'return shaders;'
	echo '}(std::to_array<std::pair<std::string_view, std::string_view>>({'

	for filename in `ls ${SHADERS_SRC}`; do
		echo "--	${filename}"
		{
			echo -n 'R"#('
			cat ${SHADERS_SRC}/${filename}
			echo ')#"'
		} > ./src/render/shaders/${filename}.inc
		echo "{\"${filename}\","
		echo "#include \"./${filename}.inc\""
		echo "},"
	done

	echo '}));'
} > ./src/render/shaders/Shaders.hpp
