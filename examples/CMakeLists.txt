project(reckless_examples)
CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

################################################################################
# Build Examples
################################################################################
add_executable(severity_log severity_log.cpp )
target_link_libraries(severity_log reckless)

add_executable(stderr_log stderr_log.cpp )
target_link_libraries(stderr_log reckless)

add_executable(ucs2_log ucs2_log.cpp )
target_link_libraries(ucs2_log reckless)

add_executable(custom_formatter custom_formatter.cpp )
target_link_libraries(custom_formatter reckless)

if (UNIX)
target_link_libraries(severity_log pthread)
target_link_libraries(stderr_log pthread)
target_link_libraries(ucs2_log pthread)
target_link_libraries(custom_formatter pthread)
elseif(WIN32)
target_link_libraries(severity_log Synchronization)
target_link_libraries(stderr_log Synchronization)
target_link_libraries(ucs2_log Synchronization)
target_link_libraries(custom_formatter Synchronization)
endif()
