cmake_minimum_required(VERSION 3.16)
project(battlecity-ng LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(BC_BUILD_SERVER "Build the game server" ON)

if(BC_BUILD_SERVER)
  add_subdirectory(server)
endif()
