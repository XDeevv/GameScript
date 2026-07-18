set(GSSTDLIB_SRC gstdaux.cpp
                 gstdblob.cpp
                 gstdio.cpp
                 gstdmath.cpp
                 gstdrex.cpp
                 gstdstream.cpp
                 gstdstring.cpp
                 gstdsystem.cpp)

if(NOT DISABLE_DYNAMIC)
  add_library(gstdlib SHARED ${GSSTDLIB_SRC})
  add_library(GameScript::gstdlib ALIAS gstdlib)
  set_property(TARGET gstdlib PROPERTY EXPORT_NAME gstdlib)
  
  set_property(TARGET gstdlib PROPERTY OUTPUT_NAME gstdlib)
  set_property(TARGET gstdlib PROPERTY PREFIX "")
  
  target_link_libraries(gstdlib GameScript)
  if(NOT GS_DISABLE_INSTALLER)
    install(TARGETS gstdlib EXPORT GameScript
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Libraries
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Libraries NAMELINK_SKIP
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Libraries
      )
    install(TARGETS gstdlib EXPORT GameScript
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development NAMELINK_ONLY
      )
  endif()
  target_include_directories(gstdlib PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )
endif()

if(NOT DISABLE_STATIC)
  add_library(gstdlib_static STATIC ${GSSTDLIB_SRC})
  add_library(GameScript::gstdlib_static ALIAS gstdlib_static)
  set_property(TARGET gstdlib_static PROPERTY EXPORT_NAME gstdlib_static)
  if(NOT GS_DISABLE_INSTALLER)
    install(TARGETS gstdlib_static EXPORT GameScript ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development)
  endif()
  target_include_directories(gstdlib_static PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )
endif()

if(LONG_OUTPUT_NAMES)
  if(NOT DISABLE_STATIC)
    set_target_properties(gstdlib_static PROPERTIES OUTPUT_NAME gstdlib3_static)
  endif()
endif()
