add_executable(testserver testserver.cc)
target_link_libraries(testserver PRIVATE altel-rbcp mycommon)

add_executable(testclient testclient.cc)
target_link_libraries(testclient PRIVATE altel-rbcp mycommon)

add_executable(testdecode testdecode.cc)
target_link_libraries(testclient PRIVATE altel-rbcp mycommon)

add_executable(testdump testdump.cc)
target_link_libraries(testdump PRIVATE altel-rbcp mycommon)

add_executable(testctrl testctrl.cc)
target_link_libraries(testctrl PRIVATE altel-rbcp mycommon)

install(TARGETS testdump testserver testclient testdecode testctrl
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
  )
