
add_executable(regctrl_main regctrl_main.cc)
target_link_libraries(regctrl_main PRIVATE altel-rbcp mycommon)

add_executable(regctrl_only_main regctrl_only_main.cc)
target_link_libraries(regctrl_only_main PRIVATE altel-rbcp mycommon)

add_executable(testserver testserver.cc)
target_link_libraries(testserver PRIVATE altel-rbcp mycommon)


add_executable(dmarx_dump dmarx_dump.cc)

target_include_directories(dmarx_dump
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
  )
target_link_libraries(dmarx_dump PRIVATE mycommon stdc++fs Threads::Threads)

install(TARGETS regctrl_main dmarx_dump regctrl_only_main  testserver
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
  )

