# Battle City client (SDL2 + Dear ImGui). Works on Linux, Windows and (with Emscripten) the web.
if(EMSCRIPTEN)
  # SDL2 comes from Emscripten's own port (-sUSE_SDL=2); there is nothing to find
  add_library(SDL2::SDL2 INTERFACE IMPORTED)
  target_compile_options(SDL2::SDL2 INTERFACE -sUSE_SDL=2)
  target_link_options(SDL2::SDL2 INTERFACE -sUSE_SDL=2)
else()
  find_package(SDL2 QUIET)
  if(NOT SDL2_FOUND)
    message(WARNING "SDL2 development files not found: skipping the client (install libsdl2-dev, or pass -DBC_BUILD_CLIENT=OFF)")
    return()
  endif()
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

if(EMSCRIPTEN)
  # Browser build: index.html + index.js + index.wasm + index.data
  set(BC_WEB_DATA ${CMAKE_BINARY_DIR}/web-data CACHE PATH "Staged game data preloaded into the browser build")
  add_custom_target(bc-web-data
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../tools/stage-web-data.sh ${BC_WEB_DATA})
  add_dependencies(bc-client bc-web-data)
  set_target_properties(bc-client PROPERTIES OUTPUT_NAME index SUFFIX ".html")
  target_link_options(bc-client PRIVATE
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864 -sFORCE_FILESYSTEM=1
    -sEXIT_RUNTIME=0 -sASSERTIONS=1
    --preload-file ${BC_WEB_DATA}@/
    --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/../web/shell.html)
  add_custom_command(TARGET bc-client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/../data/TimGM6mb.sf2 $<TARGET_FILE_DIR:bc-client>/TimGM6mb.sf2)
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/platform/main_sdl.cpp PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../web/shell.html)
endif()
