project(reckless)
CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

################################################################################
# Add Flags
################################################################################
set(CMAKE_CXX_STANDARD 11)
if(MSVC)
    add_compile_options("/W4")
else()
    add_compile_options("-Wall -Wextra -O3 -g")
endif()

################################################################################
# Build Libraries
################################################################################
include_directories("reckless/include")
include_directories("performance_log/include")


set (SRC_LIST
reckless/src/output_buffer.cpp
reckless/src/ntoa.cpp
reckless/src/template_formatter.cpp
reckless/src/writer.cpp
reckless/src/basic_log.cpp
reckless/src/policy_log.cpp
reckless/src/file_writer.cpp
reckless/src/fd_writer.cpp
reckless/src/mpsc_ring_buffer.cpp
reckless/src/platform.cpp
reckless/src/lockless_cv.cpp
)

if(WIN32)
   set (SRC_LIST ${SRC_LIST} reckless/src/spsc_event_win32.cpp)
endif()

add_library(reckless STATIC ${SRC_LIST})

################################################################################
# Build Examples
################################################################################
message (STATUS "Making example applications")
add_subdirectory(examples)
