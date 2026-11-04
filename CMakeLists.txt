cmake_minimum_required(VERSION 3.20)
project(colorful_talk VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/utf-8 /W3)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wextra)
endif()

set(CT_SOURCES
  src/utf8.c
  src/kana_table.c
  src/parser.c
  src/prosody.c
  src/phoneme.c
  src/track.c
  src/klatt.c
  src/wav.c
  src/api.c
)

# 静的ライブラリ
add_library(colorful_talk_static STATIC ${CT_SOURCES})
target_include_directories(colorful_talk_static PUBLIC include PRIVATE src)
target_compile_definitions(colorful_talk_static PUBLIC CT_STATIC)

# DLL / 共有ライブラリ
add_library(colorful_talk SHARED ${CT_SOURCES})
target_include_directories(colorful_talk PUBLIC include PRIVATE src)
target_compile_definitions(colorful_talk PRIVATE CT_BUILD_DLL)
set_target_properties(colorful_talk PROPERTIES C_VISIBILITY_PRESET hidden)

if(NOT WIN32)
  target_link_libraries(colorful_talk_static PUBLIC m)
  target_link_libraries(colorful_talk PRIVATE m)
endif()

# CLI
add_executable(ctalk cli/ctalk.c)
target_link_libraries(ctalk PRIVATE colorful_talk_static)
if(WIN32)
  target_link_libraries(ctalk PRIVATE winmm)
endif()

# テスト
enable_testing()
add_executable(test_parser tests/test_parser.c)
target_include_directories(test_parser PRIVATE src)
target_link_libraries(test_parser PRIVATE colorful_talk_static)
add_test(NAME parser COMMAND test_parser)

add_executable(test_synth tests/test_synth.c)
target_include_directories(test_synth PRIVATE src)
target_link_libraries(test_synth PRIVATE colorful_talk_static)
add_test(NAME synth COMMAND test_synth)
