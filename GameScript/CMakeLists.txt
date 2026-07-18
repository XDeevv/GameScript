set(GameScript_SRC GSapi.cpp
                 GSbaselib.cpp
                 GSclass.cpp
                 GScompiler.cpp
                 GSdebug.cpp
                 GSfuncstate.cpp
                 GSlexer.cpp
                 GSmem.cpp
                 GSobject.cpp
                 GSstate.cpp
                 GStable.cpp
                 GSvm.cpp)

if(NOT DISABLE_DYNAMIC)
  add_library(GameScript SHARED ${GameScript_SRC})
  add_library(GameScript::GameScript ALIAS GameScript)
  set_property(TARGET GameScript PROPERTY EXPORT_NAME GameScript)
  
  set_property(TARGET GameScript PROPERTY OUTPUT_NAME gamescript)
  set_property(TARGET GameScript PROPERTY PREFIX "")

  if(NOT GS_DISABLE_INSTALLER)
    install(TARGETS GameScript EXPORT GameScript
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Libraries
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Libraries NAMELINK_SKIP
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Libraries
      )
    install(TARGETS GameScript EXPORT GameScript
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development NAMELINK_ONLY
      )
  endif()
  target_include_directories(GameScript PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )
endif()

if(NOT DISABLE_STATIC)
  add_library(GameScript_static STATIC ${GameScript_SRC})
  add_library(GameScript::GameScript_static ALIAS GameScript_static)
  set_property(TARGET GameScript_static PROPERTY EXPORT_NAME GameScript_static)
  if(NOT GS_DISABLE_INSTALLER)
    install(TARGETS GameScript_static EXPORT GameScript ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development)
  endif()
  target_include_directories(GameScript_static PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )
endif()

if(LONG_OUTPUT_NAMES)
  if(NOT DISABLE_STATIC)
    set_target_properties(GameScript_static PROPERTIES OUTPUT_NAME GameScript3_static)
  endif()
endif()
