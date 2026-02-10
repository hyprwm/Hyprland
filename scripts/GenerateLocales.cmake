file(MAKE_DIRECTORY "${OUTPUT_DIR}")
separate_arguments(LOCALES_LIST UNIX_COMMAND "${LOCALES}")

# json parser thing.
foreach(LOCALE ${LOCALES_LIST})
  set(LOCALE_DIR "${INPUT_DIR}/${LOCALE}")
  string(REGEX REPLACE "[^a-zA-Z0-9]" "_" SAFE_LOCALE "${LOCALE}")

  set(CPP_FILE "${OUTPUT_DIR}/${LOCALE}.cpp")
  set(HPP_FILE "${OUTPUT_DIR}/${LOCALE}.hpp")

  if(NOT IS_DIRECTORY "${LOCALE_DIR}")
    continue()
  endif()

  file(WRITE "${HPP_FILE}" "#pragma once\n#include \"src/i18n/Engine.hpp\"\nnamespace I18n { void register_${SAFE_LOCALE}(Hyprutils::I18n::CI18nEngine* huEngine); }\n")
  file(WRITE "${CPP_FILE}" "#include \"${LOCALE}.hpp\"\n#include \"src/i18n/Engine.hpp\"\n#include <string>\n\nnamespace I18n {\n\nvoid register_${SAFE_LOCALE}(Hyprutils::I18n::CI18nEngine* huEngine) {\n")

  file(GLOB JSON_FILES "${LOCALE_DIR}/*.json")
  foreach(JSON_FILE ${JSON_FILES})
    file(READ "${JSON_FILE}" CONTENTS)

    string(REPLACE ";" "[[SEMICOLON]]" SAFE_CONTENTS "${CONTENTS}")

    # @plural matcher.
    string(REGEX MATCHALL "\"([^\"]+)\"[ \t\r\n]*:[ \t\r\n]*\\{[ \t\r\n]*\"@plural\"[ \t\r\n]*:[ \t\r\n]*\\{[^}]+\\}" PLURALS "${SAFE_CONTENTS}")

    foreach(P_BLOCK ${PLURALS})
      string(REGEX MATCH "\"([^\"]+)\"[ \t\r\n]*:[ \t\r\n]*\\{" _ "${P_BLOCK}")
      set(P_KEY "${CMAKE_MATCH_1}")

      string(REGEX MATCH "\"one\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]*)\"" _ "${P_BLOCK}")
      set(P_ONE "${CMAKE_MATCH_1}")
      string(REGEX MATCH "\"many\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]*)\"" _ "${P_BLOCK}")
      set(P_MANY "${CMAKE_MATCH_1}")

      string(REPLACE "[[SEMICOLON]]" ";" P_ONE "${P_ONE}")
      string(REPLACE "[[SEMICOLON]]" ";" P_MANY "${P_MANY}")

      file(APPEND "${CPP_FILE}" "    huEngine->registerEntry(\"${LOCALE}\", ${P_KEY}, [](const Hyprutils::I18n::translationVarMap& vars) {\n")
      file(APPEND "${CPP_FILE}" "        int count = std::stoi(vars.at(\"count\"));\n")
      file(APPEND "${CPP_FILE}" "        if (count == 1) return \"${P_ONE}\";\n")
      file(APPEND "${CPP_FILE}" "        return \"${P_MANY}\";\n")
      file(APPEND "${CPP_FILE}" "    });\n")

      list(APPEND HANDLED_KEYS "${P_KEY}")
    endforeach()

    # k/v matcher.
    string(REGEX MATCHALL "\"([^\"]+)\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]*)\"" STRINGS "${SAFE_CONTENTS}")

    foreach(S_MATCH ${STRINGS})
      string(REGEX MATCH "\"([^\"]+)\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]*)\"" _ "${S_MATCH}")
      set(S_KEY "${CMAKE_MATCH_1}")
      set(S_VAL "${CMAKE_MATCH_2}")

      if(NOT S_KEY MATCHES "^(@plural|one|many)$" AND NOT S_KEY IN_LIST HANDLED_KEYS)
        string(REPLACE "[[SEMICOLON]]" ";" S_VAL "${S_VAL}")
        file(APPEND "${CPP_FILE}" "    huEngine->registerEntry(\"${LOCALE}\", ${S_KEY}, \"${S_VAL}\");\n")
      endif()
    endforeach()

    unset(HANDLED_KEYS)
  endforeach()

  file(APPEND "${CPP_FILE}" "}\n\n} // namespace I18n\n")
endforeach()

# Registry
file(WRITE "${OUTPUT_DIR}/i18n_registry.hpp" "#pragma once\n#include \"src/i18n/Engine.hpp\"\nnamespace I18n { void register_all(Hyprutils::I18n::CI18nEngine* huEngine); }\n")
file(WRITE "${OUTPUT_DIR}/i18n_registry.cpp" "#include \"i18n_registry.hpp\"\n")
foreach(LOCALE ${LOCALES_LIST})
  file(APPEND "${OUTPUT_DIR}/i18n_registry.cpp" "#include \"${LOCALE}.hpp\"\n")
endforeach()
file(APPEND "${OUTPUT_DIR}/i18n_registry.cpp" "\nnamespace I18n {\nvoid register_all(Hyprutils::I18n::CI18nEngine* huEngine) {\n")
foreach(LOCALE ${LOCALES_LIST})
  string(REGEX REPLACE "[^a-zA-Z0-9]" "_" SAFE_LOCALE "${LOCALE}")
  file(APPEND "${OUTPUT_DIR}/i18n_registry.cpp" "    register_${SAFE_LOCALE}(huEngine);\n")
endforeach()
file(APPEND "${OUTPUT_DIR}/i18n_registry.cpp" "}\n} // namespace I18n\n")
