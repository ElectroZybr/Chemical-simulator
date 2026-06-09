# Убирает -fPIC из cmake/compilers/Clang.cmake в исходниках oneTBB.
# TBB v2023.0.0 безусловно добавляет -fPIC для всех Clang-таргетов, но
# clang на Windows с target x86_64-pc-windows-msvc не поддерживает -fPIC
# (это PIC по-умолчанию для PE-формата) и падает с unsupported option.

set(TBB_CLANG_CMAKE "${SRC_DIR}/cmake/compilers/Clang.cmake")
if(NOT EXISTS "${TBB_CLANG_CMAKE}")
    message(WARNING "patch_tbb_clang_fpic: ${TBB_CLANG_CMAKE} not found, skipping")
    return()
endif()

file(READ "${TBB_CLANG_CMAKE}" CONTENT)
string(REPLACE " -fPIC " " " PATCHED "${CONTENT}")
if(NOT CONTENT STREQUAL PATCHED)
    file(WRITE "${TBB_CLANG_CMAKE}" "${PATCHED}")
    message(STATUS "patch_tbb_clang_fpic: removed -fPIC from Clang.cmake")
endif()
