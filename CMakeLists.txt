cmake_minimum_required(VERSION 3.15)
project(cd_frontend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(include)

add_executable(cd_frontend
  src/main.cpp
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
