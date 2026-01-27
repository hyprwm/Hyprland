#!/bin/sh

# dash-compatible and space-safe shader include generator. @Rtur2003
SHADERS_SRC="./src/render/shaders/glsl"
SHADERS_OUT="./src/render/shaders"
SHADERS_HPP="${SHADERS_OUT}/Shaders.hpp"

echo "-- Generating shader includes"

if [ ! -d "${SHADERS_SRC}" ]; then
	echo "!! No shader sources in ${SHADERS_SRC}"
	exit 1
fi

mkdir -p "${SHADERS_OUT}"

{
	echo '#pragma once'
	echo '#include <map>'
	echo 'static const std::map<std::string, std::string> SHADERS = {'
} > "${SHADERS_HPP}"

found=0
for file in "${SHADERS_SRC}"/*; do
	[ -f "${file}" ] || continue
	found=1
	filename=$(basename -- "${file}")
	echo "-- ${filename}"

	{
		echo 'R"#('
		cat "${file}"
		echo ')#"'
	} > "${SHADERS_OUT}/${filename}.inc"

	{
		printf '{ "%s",\n' "${filename}"
		printf '#include "./%s.inc"\n' "${filename}"
		printf "},\n"
	} >> "${SHADERS_HPP}"
done

if [ "${found}" -eq 0 ]; then
	echo "!! No shader files found in ${SHADERS_SRC}"
	rm -f "${SHADERS_HPP}"
	exit 1
fi

echo '};' >> "${SHADERS_HPP}"
