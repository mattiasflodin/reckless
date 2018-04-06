project(reckless_examples)
CMAKE_MINIMUM_REQUIRED(VERSION 3.5)



################################################################################
# Build Examples
################################################################################
add_executable(severity_log severity_log.cpp )
target_link_libraries(severity_log reckless pthread)

add_executable(stderr_log stderr_log.cpp )
target_link_libraries(stderr_log reckless pthread)

add_executable(ucs2_log ucs2_log.cpp )
target_link_libraries(ucs2_log reckless pthread)

add_executable(custom_formatter custom_formatter.cpp )
target_link_libraries(custom_formatter reckless pthread)

