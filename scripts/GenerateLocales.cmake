macro(SANITIZE OUT_VAR IN_VAL)
  set(TMP "${IN_VAL}")
  string(REPLACE "\\" "\\\\" TMP "${TMP}")
  string(REPLACE "\"" "\\\"" TMP "${TMP}")
  string(REPLACE "\n" "\\n" TMP "${TMP}")
  string(REPLACE "\r" "\\r" TMP "${TMP}")
  string(REPLACE "\t" "\\t" TMP "${TMP}")
  set(${OUT_VAR} "${TMP}")
endmacro()

string(REGEX REPLACE "[^a-zA-Z0-9]" "_" SAFE_LOCALE "${LOCALE}")
set(CPP_FILE "${OUTPUT_DIR}/${LOCALE}.cpp")
set(HPP_FILE "${OUTPUT_DIR}/${LOCALE}.hpp")

file(WRITE "${HPP_FILE}" "#pragma once\n#include \"src/i18n/Engine.hpp\"\nnamespace I18n { void register_${SAFE_LOCALE}(Hyprutils::I18n::CI18nEngine* huEngine); }\n")
file(WRITE "${CPP_FILE}" "#include \"${LOCALE}.hpp\"\n#include \"src/i18n/Engine.hpp\"\n#include <string>\n\nnamespace I18n {\nvoid register_${SAFE_LOCALE}(Hyprutils::I18n::CI18nEngine* huEngine) {\n")

file(GLOB JSON_FILES "${INPUT_DIR}/${LOCALE}/*.json")

foreach(JSON_FILE ${JSON_FILES})
  file(READ "${JSON_FILE}" JSON_DATA)
  string(JSON NUM_KEYS LENGTH "${JSON_DATA}")

  if(NUM_KEYS GREATER 0)
    math(EXPR LAST_INDEX "${NUM_KEYS} - 1")

    foreach(I RANGE ${LAST_INDEX})
      string(JSON KEY MEMBER "${JSON_DATA}" "${I}")
      string(JSON TYPE TYPE "${JSON_DATA}" "${KEY}")

      if(TYPE STREQUAL "OBJECT")
        string(JSON NUM_RULES LENGTH "${JSON_DATA}" "${KEY}")
        math(EXPR RULE_LIMIT "${NUM_RULES} - 1")

        set(VARS_TO_INIT "")

        foreach(IDX RANGE ${RULE_LIMIT})
          string(JSON R MEMBER "${JSON_DATA}" "${KEY}" ${IDX})
          if(R STREQUAL "*")
            continue()
          endif()

          string(REGEX MATCH "[a-zA-Z_][a-zA-Z0-9_]*" V_NAME "${R}")

          if(V_NAME)
            set(V_TYPE "STRING")
            if("${R}" MATCHES "[0-9<>]")
              set(V_TYPE "INT")
            endif()

            list(APPEND VARS_TO_INIT "${V_NAME}:${V_TYPE}")
          endif()
        endforeach()
        list(REMOVE_DUPLICATES VARS_TO_INIT)

        file(APPEND "${CPP_FILE}" "    huEngine->registerEntry(\"${LOCALE}\", ${KEY}, [](const Hyprutils::I18n::translationVarMap& vars) {\n")

        foreach(V_PAIR ${VARS_TO_INIT})
          string(REPLACE ":" ";" V_PARTS "${V_PAIR}")
          list(GET V_PARTS 0 V_NAME)
          list(GET V_PARTS 1 V_TYPE)

          if(V_TYPE STREQUAL "INT")
            file(APPEND "${CPP_FILE}" "        int ${V_NAME} = std::stoi(vars.at(\"${V_NAME}\"));\n")
          else()
            file(APPEND "${CPP_FILE}" "        auto ${V_NAME} = vars.at(\"${V_NAME}\");\n")
          endif()
        endforeach()

        set(IS_FIRST TRUE)
        set(ELSE_T "")

        foreach(J RANGE ${RULE_LIMIT})
          string(JSON RULE MEMBER "${JSON_DATA}" "${KEY}" "${J}")
          string(JSON TXT GET "${JSON_DATA}" "${KEY}" "${RULE}")
          SANITIZE(SAFE_TXT "${TXT}")

          if(RULE STREQUAL "*")
            set(ELSE_T "${SAFE_TXT}")
          else()
            set(STMT "else if")
            if(IS_FIRST)
              set(STMT "if")
              set(IS_FIRST FALSE)
            endif()
            file(APPEND "${CPP_FILE}" "        ${STMT} (${RULE}) return \"${SAFE_TXT}\";\n")
          endif()
        endforeach()

        file(APPEND "${CPP_FILE}" "        return \"${ELSE_T}\";\n    });\n")

      else()
        string(JSON TXT GET "${JSON_DATA}" "${KEY}")
        SANITIZE(SAFE_TXT "${TXT}")
        file(APPEND "${CPP_FILE}" "    huEngine->registerEntry(\"${LOCALE}\", ${KEY}, \"${SAFE_TXT}\");\n")
      endif()
    endforeach()
  endif()
endforeach()

file(APPEND "${CPP_FILE}" "}\n} // namespace I18n\n")
