#!/bin/sh

SHADERS_SRC="./src/render/shaders/glsl"

echo "-- Generating shader includes"

if [ ! -d ./src/render/shaders ]; then
	mkdir ./src/render/shaders
fi

echo '#pragma once' > ./src/render/shaders/Shaders.hpp
echo '#include <algorithm>' >> ./src/render/shaders/Shaders.hpp
echo '#include <array>' >> ./src/render/shaders/Shaders.hpp
echo '#include <string_view>' >> ./src/render/shaders/Shaders.hpp
echo '#include <utility>' >> ./src/render/shaders/Shaders.hpp
echo 'inline constexpr auto SHADERS = [](auto shaders) {' >> ./src/render/shaders/Shaders.hpp
echo 'std::ranges::sort(shaders, {}, [](auto pair) { return pair.first; });' >> ./src/render/shaders/Shaders.hpp
echo 'return shaders;' >> ./src/render/shaders/Shaders.hpp
echo '}(std::to_array<std::pair<std::string_view, std::string_view>>({' >> ./src/render/shaders/Shaders.hpp

find "$SHADERS_SRC" -maxdepth 1 -type f | sort -fdi | while read -r filepath; do
	filename=${filepath#"${SHADERS_SRC}"/}
	echo "--	${filename}"
	
	{ echo -n 'R"#('; cat ${SHADERS_SRC}/${filename}; echo ')#"'; } > ./src/render/shaders/${filename}.inc
	echo "{\"${filename}\"," >> ./src/render/shaders/Shaders.hpp
	echo "#include \"./${filename}.inc\"" >> ./src/render/shaders/Shaders.hpp
	echo "}," >> ./src/render/shaders/Shaders.hpp
done

echo '}));' >> ./src/render/shaders/Shaders.hpp
