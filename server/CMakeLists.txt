# Battle City server. Builds on Linux; the Windows build still uses bc-server.vcxproj.
find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

file(GLOB BC_SERVER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(bc-server ${BC_SERVER_SOURCES})
target_include_directories(bc-server PRIVATE ${SQLite3_INCLUDE_DIRS})
target_link_libraries(bc-server PRIVATE Threads::Threads ${SQLite3_LIBRARIES})
if(NOT MSVC)
  # The packet checksum adds `char` values: pin signedness so every platform (incl. ARM) agrees
  target_compile_options(bc-server PRIVATE -fsigned-char)
  target_compile_options(bc-server PRIVATE -Wall -Wno-unused-variable -Wno-sign-compare -Wno-write-strings)
endif()

install(TARGETS bc-server RUNTIME DESTINATION bin)

# Unit test for the outgoing queue (no game code involved)
if(NOT WIN32)
  add_executable(test-outqueue test/test_outqueue.cpp)
  target_compile_options(test-outqueue PRIVATE -Wall)
  add_test(NAME outqueue COMMAND test-outqueue)
endif()
