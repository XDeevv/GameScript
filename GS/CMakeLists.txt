set(CMAKE_C_STANDARD 99)

if(NOT DISABLE_DYNAMIC)
  add_executable(gamescript GS.c)
  add_executable(GameScript::interpreter ALIAS gamescript)
  set_target_properties(gamescript PROPERTIES EXPORT_NAME interpreter)
  target_link_libraries(gamescript GameScript gstdlib)

  if(NOT GS_DISABLE_INSTALLER)
    install(TARGETS gamescript EXPORT GameScript RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
  endif()

  target_include_directories(gamescript PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )
endif()

if(NOT DISABLE_STATIC)
  add_executable(gamescript_static GS.c)
  add_executable(GameScript::interpreter_static ALIAS gamescript_static)
  set_target_properties(gamescript_static PROPERTIES EXPORT_NAME interpreter_static)
  target_link_libraries(gamescript_static GameScript_static gstdlib_static)

  if(NOT GS_DISABLE_INSTALLER)
    install(TARGETS gamescript_static EXPORT GameScript RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
  endif()

  target_include_directories(gamescript_static PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )
endif()

if(LONG_OUTPUT_NAMES)
  if(NOT DISABLE_DYNAMIC)
    set_target_properties(gamescript PROPERTIES OUTPUT_NAME GameScript)
  endif()

  if(NOT DISABLE_STATIC)
    set_target_properties(gamescript_static PROPERTIES OUTPUT_NAME GameScript_static)
  endif()
endif()
