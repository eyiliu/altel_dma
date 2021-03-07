

function(create_cpp_raw_string)
  if( (${ARGC} LESS 2) OR (${ARGC} GREATER 3 ) )
    message(FATAL_ERROR "create_cpp_raw_string require 2 or 3 arguments, but given ${ARGC}")
  endif()
  if(${ARGC} EQUAL 2)
    set(RAW_STRING_DELIM "~~~cpp~raw~~~")
  endif()
  if(${ARGC} EQUAL 3)
    set(RAW_STRING_DELIM ${ARGV2})
  endif()
  if( (NOT EXISTS ${ARGV0}) OR (${ARGV1} IS_NEWER_THAN ${ARGV0}) )
    file(READ ${ARGV1} RAW_STRING_CONTENT)
    set(RAW_STRING_CONTENT "R\"${RAW_STRING_DELIM}(${RAW_STRING_CONTENT})${RAW_STRING_DELIM}\"")
    file(WRITE ${ARGV0} "${RAW_STRING_CONTENT}")
  endif()
endfunction()

create_cpp_raw_string(${CMAKE_CURRENT_BINARY_DIR}/include/altel_reg_cmd_list_json.hh
  ${PROJECT_SOURCE_DIR}/source/lib/misc/altel_reg_cmd_list.json)

add_executable(regctrl_main regctrl_main.cc)

target_include_directories(regctrl_main
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
  )
target_link_libraries(regctrl_main PRIVATE altel-rbcp mycommon)


# add_executable(tcprx_dump tcprx_dump.cc)

# target_include_directories(tcprx_dump
#   PRIVATE
#   $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
#   )
# target_link_libraries(tcprx_dump PRIVATE altel-rbcp mycommon stdc++fs)


add_executable(dmarx_dump dmarx_dump.cc)

target_include_directories(dmarx_dump
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
  )
target_link_libraries(dmarx_dump PRIVATE mycommon stdc++fs)


install(TARGETS regctrl_main dmarx_dump
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
  )

