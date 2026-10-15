# Battle City client (SDL2 + Dear ImGui). Works on Linux, Windows and (with Emscripten) the web.
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
  message(WARNING "SDL2 development files not found: skipping the client (install libsdl2-dev, or pass -DBC_BUILD_CLIENT=OFF)")
  return()
endif()

file(GLOB BC_CLIENT_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/platform/*.cpp)
# Legacy DirectX/Win32 sources, replaced by platform/
list(FILTER BC_CLIENT_SOURCES EXCLUDE REGEX "/(DDUtil|Main)\\.cpp$")

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_impl_sdl2.cpp ${IMGUI_DIR}/imgui_impl_sdlrenderer2.cpp)
target_include_directories(imgui PUBLIC ${IMGUI_DIR})
target_link_libraries(imgui PUBLIC SDL2::SDL2)

add_executable(bc-client ${BC_CLIENT_SOURCES})
target_include_directories(bc-client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/platform)
target_compile_options(bc-client PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/platform/compat.h)
target_link_libraries(bc-client PRIVATE SDL2::SDL2 imgui)
if(NOT MSVC)
  # The packet checksum adds `char` values: pin signedness so every platform (incl. ARM) agrees
  target_compile_options(bc-client PRIVATE -fsigned-char)
  target_compile_options(bc-client PRIVATE -w)
endif()
