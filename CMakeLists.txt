cmake_minimum_required(VERSION 3.24)

project(UltraMarkdown LANGUAGES C CXX)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/utf-8 /W4 /permissive-)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
endif()

set(CMARK_TESTS OFF CACHE BOOL "" FORCE)
set(CMARK_SHARED OFF CACHE BOOL "" FORCE)
set(CMARK_STATIC ON CACHE BOOL "" FORCE)
set(CMARK_LIB_FUZZER OFF CACHE BOOL "" FORCE)
set(CMARK_FUZZ_QUADRATIC OFF CACHE BOOL "" FORCE)

set(CMARK_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/third_party/cmark-gfm")
add_subdirectory(third_party/cmark-gfm "${CMARK_BINARY_DIR}" EXCLUDE_FROM_ALL)

file(GLOB SCINTILLA_CORE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/scintilla/src/*.cxx"
)

add_library(scintilla STATIC
  ${SCINTILLA_CORE_SOURCES}
  third_party/scintilla/win32/HanjaDic.cxx
  third_party/scintilla/win32/PlatWin.cxx
  third_party/scintilla/win32/ScintillaWin.cxx
)

target_include_directories(scintilla
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/scintilla/include"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/scintilla/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/scintilla/win32"
)

target_link_libraries(scintilla
  PUBLIC
    gdi32
    imm32
    ole32
    oleaut32
    advapi32
)

add_executable(ultra_markdown WIN32
  src/main.cpp
  src/AppWindow.cpp
  src/Document.cpp
  src/EditorHost.cpp
  src/MarkdownRenderer.cpp
  src/PreviewView.cpp
  src/Utf8.cpp
)

target_include_directories(ultra_markdown
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/scintilla/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cmark-gfm/src"
    "${CMARK_BINARY_DIR}/src"
    "${CMARK_BINARY_DIR}/extensions"
)

target_link_libraries(ultra_markdown
  PRIVATE
    scintilla
    libcmark-gfm_static
    libcmark-gfm-extensions_static
    comctl32
    shlwapi
)

set_target_properties(ultra_markdown PROPERTIES
  OUTPUT_NAME "UltraMarkdown"
)
