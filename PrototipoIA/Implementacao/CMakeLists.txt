cmake_minimum_required(VERSION 3.20)
project(bancada_cbd LANGUAGES CXX)

# Plataforma oficial: Linux, incluindo WSL2 (decisão 156). macOS compila, mas não está no CI.
if(WIN32)
  message(FATAL_ERROR "Windows nativo não é suportado: use o WSL2 (decisão 156)")
endif()
include(CheckSymbolExists)
check_symbol_exists(pread "unistd.h" HAVE_PREAD)
if(NOT HAVE_PREAD)
  message(FATAL_ERROR "pread/pwrite (POSIX) são obrigatórios (decisão 156)")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)

option(CBD_SANITIZE "ASan + UBSan" OFF)
if(CBD_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

file(GLOB CBD_LIB_SRC CONFIGURE_DEPENDS
  src/common/*.cpp src/layer4/*.cpp src/layer3/*.cpp src/layer2/*.cpp)
add_library(cbd_motor STATIC ${CBD_LIB_SRC})
target_include_directories(cbd_motor PUBLIC src)

add_executable(bancada src/layer1/bench.cpp)
target_link_libraries(bancada PRIVATE cbd_motor)
target_compile_definitions(bancada PRIVATE CBD_FLAGS="${CMAKE_CXX_FLAGS} ${CMAKE_BUILD_TYPE}")

# Testes: GoogleTest se estiver instalado; senão, o shim tests/mini_test.hpp (mesma sintaxe).
file(GLOB CBD_TEST_SRC CONFIGURE_DEPENDS tests/test_*.cpp)
find_package(GTest QUIET)
if(GTest_FOUND)
  add_executable(testes ${CBD_TEST_SRC})
  target_compile_definitions(testes PRIVATE CBD_USE_GTEST)
  target_link_libraries(testes PRIVATE cbd_motor GTest::gtest_main)
else()
  message(STATUS "GoogleTest não encontrado: usando o shim tests/mini_test.hpp")
  add_executable(testes ${CBD_TEST_SRC} tests/mini_main.cpp)
  target_link_libraries(testes PRIVATE cbd_motor)
endif()
target_include_directories(testes PRIVATE tests)
enable_testing()
add_test(NAME testes COMMAND testes)
