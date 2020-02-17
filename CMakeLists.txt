project(reckless)
CMAKE_MINIMUM_REQUIRED(VERSION 3.5)


################################################################################
# Build Libraries
################################################################################

add_library(reckless SHARED
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
# Add Flags
################################################################################
set_target_properties(reckless PROPERTIES CXX_STANDARD 17)
# set(CMAKE_CXX_FLAGS "-Wall -Wextra -O3 -g")


################################################################################
# Add Dependencies
################################################################################
target_include_directories(reckless
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/reckless/include>
    $<INSTALL_INTERFACE:include>
)


find_package(Boost REQUIRED)
target_include_directories(reckless PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(reckless LINK_PUBLIC ${Boost_LIBRARIES})


################################################################################
# Build Examples
################################################################################
message (STATUS "Making example applications")
add_subdirectory(examples)


#########################################
######			INSTALL 		   ######
#########################################
include(CMakePackageConfigHelpers)

if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif() 

install(TARGETS reckless
        EXPORT recklessTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        INCLUDES DESTINATION ${CMAKE_INSTALL_PREFIX}
        )

# This installs the include folder
install(DIRECTORY reckless/include DESTINATION ${CMAKE_INSTALL_PREFIX} FILES_MATCHING PATTERN "*.h")
install(DIRECTORY reckless/include DESTINATION ${CMAKE_INSTALL_PREFIX} FILES_MATCHING PATTERN "*.inl")
install(DIRECTORY reckless/include DESTINATION ${CMAKE_INSTALL_PREFIX} FILES_MATCHING PATTERN "*.hpp")

export(TARGETS reckless NAMESPACE reckless:: FILE recklessTargets.cmake)

# This generates recklessTargets.cmake
install(EXPORT recklessTargets
    FILE recklessTargets.cmake
    NAMESPACE reckless::
    DESTINATION lib/cmake/reckless
    )

configure_file(recklessConfig.cmake.in recklessConfig.cmake @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/recklessConfig.cmake"
        DESTINATION lib/cmake/reckless
        )
    

# uninstall target 
if(NOT TARGET uninstall) 
  configure_file( 
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in" 
      "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" 
      IMMEDIATE @ONLY) 
 
  add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake) 
endif() 
 
