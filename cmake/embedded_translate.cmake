set(TRANSLATE_CSV "${CMAKE_SOURCE_DIR}/assets/translate.csv")
set(TRANSLATE_H   "${GENERATED_DIR}/translate/translate.h")

file(MAKE_DIRECTORY "${GENERATED_DIR}/translate")

add_custom_command(
    OUTPUT ${TRANSLATE_H}
    COMMAND ${CMAKE_COMMAND}
        -DCSV_FILE=${TRANSLATE_CSV}
        -DOUT_FILE=${TRANSLATE_H}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_translations.cmake"
    DEPENDS ${TRANSLATE_CSV} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_translations.cmake"
    COMMENT "Generating translations"
    VERBATIM
)

add_custom_target(embedded_translate DEPENDS ${TRANSLATE_H})
