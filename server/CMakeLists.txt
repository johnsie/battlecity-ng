# Battle City server. Linux and Windows (MSVC) via CMake; the old bc-server.vcxproj is no longer needed.
file(GLOB BC_SERVER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(bc-server ${BC_SERVER_SOURCES})

if(WIN32)
  # SQLite is compiled in from the vendored amalgamation (the system has none, and 32-bit .lib files do not link into x64)
  add_library(bc-sqlite STATIC ${CMAKE_SOURCE_DIR}/external/sqlite/sqlite3.c)
  target_include_directories(bc-sqlite PUBLIC ${CMAKE_SOURCE_DIR}/external/sqlite)
  target_compile_definitions(bc-sqlite PRIVATE SQLITE_THREADSAFE=1)
  target_link_libraries(bc-server PRIVATE bc-sqlite ws2_32 winmm dbghelp)
else()
  find_package(Threads REQUIRED)
  find_package(SQLite3 REQUIRED)
  target_include_directories(bc-server PRIVATE ${SQLite3_INCLUDE_DIRS})
  target_link_libraries(bc-server PRIVATE Threads::Threads ${SQLite3_LIBRARIES})
endif()

if(MSVC)
  target_compile_definitions(bc-server PRIVATE _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS NOMINMAX)
  target_compile_options(bc-server PRIVATE /utf-8 /Zi /wd4996 /wd4244 /wd4267 /wd4018 /wd4101 /wd4305)
  target_link_options(bc-server PRIVATE /DEBUG)      # keep the .pdb so crash reports show function names and lines
elseif(NOT WIN32)
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
