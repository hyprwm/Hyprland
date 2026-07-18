#!/bin/sh

set -eu

export LC_ALL=C

SHADERS_SRC="./src/render/shaders/glsl"
SHADERS_OUT="./src/render/shaders"
SHADERS_HPP="${SHADERS_OUT}/Shaders.hpp"
SHADERS_TMP="${SHADERS_HPP}.tmp.$$"

replace_if_changed() {
	generated="$1"
	target="$2"

	if [ -f "${target}" ] && cmp -s "${generated}" "${target}"; then
		rm -f "${generated}"
	else
		mv "${generated}" "${target}"
	fi
}

cleanup() {
	rm -f "${SHADERS_TMP}" "${SHADERS_OUT}"/*.tmp.$$ 2>/dev/null || true
}
trap cleanup EXIT HUP INT TERM

printf '%s\n' "-- Generating shader includes" >&2
mkdir -p "${SHADERS_OUT}"

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

	for shader in "${SHADERS_SRC}"/*; do
		filename=${shader##*/}
		include="${SHADERS_OUT}/${filename}.inc"
		include_tmp="${include}.tmp.$$"

		printf '%s\n' "--	${filename}" >&2
		{
			printf 'R"#('
			cat "${shader}"
			printf ')#"\n'
		} >"${include_tmp}"
		replace_if_changed "${include_tmp}" "${include}"

		printf '{"%s",\n' "${filename}"
		printf '#include "./%s.inc"\n' "${filename}"
		echo '},'
	done

	echo '}));'
} >"${SHADERS_TMP}"

replace_if_changed "${SHADERS_TMP}" "${SHADERS_HPP}"
