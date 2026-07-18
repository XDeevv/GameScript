cmake_minimum_required(VERSION 3.12)
project(GameScript
  VERSION 3.2
  DESCRIPTION "A lightweight fork of the language GameScript specifically for game engine scripting."
  HOMEPAGE_URL "http://script.gs/"
  LANGUAGES C CXX
  )

option(DISABLE_STATIC "Avoid building/installing static libraries.")
option(LONG_OUTPUT_NAMES "Use longer names for binaries and libraries: GameScript3 (not GS).")

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif ()

include(GNUInstallDirs)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")
set(CMAKE_CXX_STANDARD 11)

if(CMAKE_COMPILER_IS_GNUCXX)
  add_compile_options(
    "$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti;-fno-exceptions>"
    -fno-strict-aliasing
    -Wall
    -Wextra
    -pedantic
    -Wcast-qual
    "$<$<CONFIG:Release>:-O3>"
    "$<$<CONFIG:RelWithDebInfo>:-O3;-g>"
    "$<$<CONFIG:MinSizeRel>:-Os>"
    "$<$<CONFIG:Debug>:-pg;-pie;-gstabs;-g3;-Og>"
    )
elseif(MSVC)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# Need to be defined here to be visible to GameScript.pc.in
if(LONG_OUTPUT_NAMES)
  set(LIBGameScript_NAME GameScript3)
  set(GSSTDLIB_NAME gstdlib3)
else()
  set(LIBGameScript_NAME GameScript)
  set(GSSTDLIB_NAME gstdlib)
endif()

add_subdirectory(GameScript)
add_subdirectory(gstdlib)
if(NOT GS_DISABLE_INTERPRETER)
  add_subdirectory(GS)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(tgts)
  if(NOT DISABLE_DYNAMIC)
    list(APPEND tgts GameScript gstdlib)
    if(NOT GS_DISABLE_INTERPRETER)
      list(APPEND tgts gamescript)
    endif()
  endif()
  if(NOT DISABLE_STATIC)
    list(APPEND tgts GameScript_static gstdlib_static)
    if(NOT GS_DISABLE_INTERPRETER)
      list(APPEND tgts gamescript_static)
    endif()
  endif()
  
  foreach(t ${tgts})
    if(TARGET ${t})
      target_compile_definitions(${t} PUBLIC -D_GS64)
    endif()
  endforeach()
endif()

if(NOT DISABLE_DYNAMIC)
  set_target_properties(GameScript gstdlib PROPERTIES SOVERSION 0 VERSION 0.0.0)
endif()

if(NOT GS_DISABLE_INSTALLER AND NOT GS_DISABLE_HEADER_INSTALLER)
  install(FILES
    include/GSconfig.h
    include/GameScript.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT Development
    )
  install(FILES
    include/gstdaux.h
    include/gstdblob.h
    include/gstdio.h
    include/gstdmath.h
    include/gstdstring.h
    include/gstdsystem.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT Development
    )
endif()

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}/cmake/GameScript/GameScript-config-version.cmake"
  VERSION "${GameScript_VERSION}"
  COMPATIBILITY AnyNewerVersion
  )

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/GameScript-config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}/cmake/GameScript/GameScript-config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/GameScript"
  )

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/GameScript.pc.in"
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
  @ONLY
)

if(NOT GS_DISABLE_INSTALLER AND NOT GS_DISABLE_CMAKE_INSTALLER)
  export(EXPORT GameScript
    NAMESPACE GameScript::
    FILE "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}/cmake/GameScript/GameScript-targets.cmake"
    )

  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}/cmake/GameScript/GameScript-config-version.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}/cmake/GameScript/GameScript-config.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/GameScript"
    COMPONENT Development
    )

  # pc(5) only allows static variant to have extra flags, not an entirely new
  # set of flags. While it's uncommon, a way to do it would be to generate a
  # ${PROJECT_NAME}_static.pc file.
  if(NOT DISABLE_DYNAMIC)
    install(
      FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
      COMPONENT Development
    )
  endif()

  install(EXPORT GameScript
    NAMESPACE GameScript::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/GameScript"
    FILE "GameScript-targets.cmake"
    COMPONENT Development
    )
endif()