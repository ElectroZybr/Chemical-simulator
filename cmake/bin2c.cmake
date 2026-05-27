if(TEXT_MODE)
    file(READ ${INPUT_FILE} FILE_CONTENT)

    string(REPLACE "\\" "\\\\" FILE_CONTENT "${FILE_CONTENT}")
    string(REPLACE "\"" "\\\"" FILE_CONTENT "${FILE_CONTENT}")
    string(REPLACE "\n" "\\n\"\n\"" FILE_CONTENT "${FILE_CONTENT}")

    file(WRITE ${OUTPUT_FILE} "#pragma once\n#include <string_view>\n\n")
    file(APPEND ${OUTPUT_FILE} "constexpr std::string_view ${VAR_NAME} =\n\"${FILE_CONTENT}\";\n")
else()
    file(READ ${INPUT_FILE} HEX_CONTENT HEX)

    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " C_ARRAY ${HEX_CONTENT})

    file(WRITE ${OUTPUT_FILE} "#pragma once\n#include <cstdint>\n\n")
    file(APPEND ${OUTPUT_FILE} "const uint8_t ${VAR_NAME}[] = {\n${C_ARRAY}\n};\n")
endif()
