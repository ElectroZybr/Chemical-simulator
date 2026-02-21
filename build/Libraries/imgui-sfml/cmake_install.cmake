# Install script for directory: C:/Users/elect/Documents/Coding/Chemical-simulator/Libraries/imgui-sfml

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/Chemical-simulator")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "C:/mingw64/bin/objdump.exe")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/elect/Documents/Coding/Chemical-simulator/build/Libraries/imgui-sfml/libImGui-SFML.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES
    "C:/Users/elect/Documents/Coding/Chemical-simulator/Libraries/imgui-sfml/imgui-SFML.h"
    "C:/Users/elect/Documents/Coding/Chemical-simulator/Libraries/imgui-sfml/imgui-SFML_export.h"
    "C:/Users/elect/Documents/Coding/Chemical-simulator/Libraries/imgui/imconfig.h"
    "C:/Users/elect/Documents/Coding/Chemical-simulator/Libraries/imgui/imgui.h"
    "C:/Users/elect/Documents/Coding/Chemical-simulator/Libraries/imgui/imgui_internal.h"
    "C:/Users/elect/Documents/Coding/Chemical-simulator/Libraries/imgui/imstb_rectpack.h"
    "C:/Users/elect/Documents/Coding/Chemical-simulator/Libraries/imgui/imstb_textedit.h"
    "C:/Users/elect/Documents/Coding/Chemical-simulator/Libraries/imgui/imstb_truetype.h"
    "C:/Users/elect/Documents/Coding/Chemical-simulator/Libraries/imgui/misc/cpp/imgui_stdlib.h"
    "C:/Users/elect/Documents/Coding/Chemical-simulator/Libraries/imgui-sfml/imconfig-SFML.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ImGui-SFML/ImGui-SFMLConfig.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ImGui-SFML/ImGui-SFMLConfig.cmake"
         "C:/Users/elect/Documents/Coding/Chemical-simulator/build/Libraries/imgui-sfml/CMakeFiles/Export/761a6a4c7704629aea6d1d08969b2ac8/ImGui-SFMLConfig.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ImGui-SFML/ImGui-SFMLConfig-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ImGui-SFML/ImGui-SFMLConfig.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/ImGui-SFML" TYPE FILE FILES "C:/Users/elect/Documents/Coding/Chemical-simulator/build/Libraries/imgui-sfml/CMakeFiles/Export/761a6a4c7704629aea6d1d08969b2ac8/ImGui-SFMLConfig.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/ImGui-SFML" TYPE FILE FILES "C:/Users/elect/Documents/Coding/Chemical-simulator/build/Libraries/imgui-sfml/CMakeFiles/Export/761a6a4c7704629aea6d1d08969b2ac8/ImGui-SFMLConfig-debug.cmake")
  endif()
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/elect/Documents/Coding/Chemical-simulator/build/Libraries/imgui-sfml/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
