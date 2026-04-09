cmake_minimum_required(VERSION 3.15)
project(cd_frontend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(include)

set(COMPILER_SOURCES
  src/compiler/ast_nodes.cpp
  src/compiler/cfg.cpp
  src/compiler/grammar_tools.cpp
  src/compiler/io_utils.cpp
  src/compiler/lexer.cpp
  src/compiler/parser.cpp
  src/compiler/pipeline.cpp
  src/compiler/semantic.cpp
  src/compiler/symbol_table.cpp
  src/compiler/tokens.cpp
)

add_executable(cd_frontend
  src/main.cpp
  ${COMPILER_SOURCES}
)

enable_testing()

add_executable(cd_tests
  tests/test_main.cpp
  ${COMPILER_SOURCES}
)

target_compile_definitions(cd_tests PRIVATE CD_PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_test(NAME cd_tests COMMAND cd_tests)
