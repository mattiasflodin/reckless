project(reckless)
CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

################################################################################
# Add Flags
################################################################################
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -O3 -g")



################################################################################
# Build Libraries
################################################################################
include_directories("boost")
include_directories("reckless/include")

add_library(reckless STATIC
reckless/src/thread_input_buffer.cpp
reckless/src/output_buffer.cpp
reckless/src/ntoa.cpp
reckless/src/crash_handler.cpp
reckless/src/template_formatter.cpp
reckless/src/writer.cpp
reckless/src/basic_log.cpp
reckless/src/policy_log.cpp
reckless/src/file_writer.cpp
reckless/src/posix_error_category.cpp
reckless/src/utility.cpp
reckless/src/fd_writer.cpp
)



################################################################################
# Build Examples
################################################################################
message (STATUS "Making example applications")
add_subdirectory(examples)

