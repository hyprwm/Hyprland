#!/bin/sh

echo '#pragma once' > ./src/render/shaders/Shaders.hpp
echo '#include <map>' >> ./src/render/shaders/Shaders.hpp
echo 'static const std::map<std::string, std::string> SHADERS = {' >> ./src/render/shaders/Shaders.hpp

for filename in `ls ./assets/shaders/`; do
	{ echo 'R"#('; cat ./assets/shaders/${filename}; echo ')#"'; } > ./src/render/shaders/${filename}.inc
	echo "{\"${filename}\"," >> ./src/render/shaders/Shaders.hpp
	echo "#include \"./${filename}.inc\"" >> ./src/render/shaders/Shaders.hpp
	echo "}," >> ./src/render/shaders/Shaders.hpp
done

echo '};' >> ./src/render/shaders/Shaders.hpp
