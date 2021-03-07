
aux_source_directory(src LIB_SRC)
add_library(altel-rbcp STATIC ${LIB_SRC} )
target_link_libraries(altel-rbcp PUBLIC mycommon)

# TODO: MAKE FUN
set(ORI_FILE ${CMAKE_CURRENT_LIST_DIR}/misc/altel_reg_cmd_list.json)
set(GEN_FILE  ${CMAKE_BINARY_DIR}/altel_reg_cmd_list.hh)
if( (NOT EXISTS ${GEN_FILE}) OR (${ORI_FILE} IS_NEWER_THAN ${GEN_FILE}) )
  set(GEN_DELIM "~~~cpp~raw~~~")
  file(READ ${ORI_FILE} ORI_FILE_CONTENT)
  set(GEN_FILE_CONTENT "R\"${GEN_DELIM}(${ORI_FILE_CONTENT})${GEN_DELIM}\"")
  file(WRITE ${GEN_FILE} "${GEN_FILE_CONTENT}")
endif()


target_include_directories(altel-rbcp
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
  )

install(TARGETS altel-rbcp
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
  )
