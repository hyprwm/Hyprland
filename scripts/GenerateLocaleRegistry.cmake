# Generates i18n_registry.hpp and i18n_registry.cpp from ${CMAKE_BINARY_DIR}/generated/<subdir>
file(WRITE "${OUTPUT_DIR}/i18n_registry.hpp" "#pragma once\n#include \"src/i18n/Engine.hpp\"\nnamespace I18n { void register_all(Hyprutils::I18n::CI18nEngine* huEngine); }\n")
file(WRITE "${OUTPUT_DIR}/i18n_registry.cpp" "#include \"i18n_registry.hpp\"\n")

separate_arguments(LOCALES_LIST UNIX_COMMAND "${LOCALES}")

foreach(L ${LOCALES_LIST})
  file(APPEND "${OUTPUT_DIR}/i18n_registry.cpp" "#include \"${L}.hpp\"\n")
endforeach()

file(APPEND "${OUTPUT_DIR}/i18n_registry.cpp" "\nnamespace I18n {\nvoid register_all(Hyprutils::I18n::CI18nEngine* huEngine) {\n")
foreach(L ${LOCALES_LIST})
  string(REGEX REPLACE "[^a-zA-Z0-9]" "_" SL "${L}")
  file(APPEND "${OUTPUT_DIR}/i18n_registry.cpp" "    register_${SL}(huEngine);\n")
endforeach()
file(APPEND "${OUTPUT_DIR}/i18n_registry.cpp" "}\n} // namespace I18n\n")
