project(data-server)

cmake_minimum_required(VERSION 2.8)

# do not allow in-source builds
if( CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND NOT MSVC_IDE )
message(FATAL_ERROR "In-source builds are not allowed. Create a directory and run cmake from there, passing the path to this source directory.")
endif()

find_package(Boost COMPONENTS system thread)

# set_property(TARGET blackadder PROPERTY POSITION_INDEPENDENT_CODE ON)

# add an uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
    
if(Boost_FOUND)

  set(DS-SRC data_server.cpp server_session.cpp client_session.cpp main.cpp)
  
  add_executable (data-server ${DS-SRC})
  include_directories(${Boost_INCLUDE_DIRS})
  target_link_libraries (data-server ${Boost_LIBRARIES})
  
  install(TARGETS data-server DESTINATION bin)

else()
  message("data-server will not be built")
endif()
