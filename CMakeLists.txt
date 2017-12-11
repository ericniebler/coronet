# coronet - An experimental networking library that supports both the
#           Universal Model of the Networking TS and the coroutines of
#           the Coroutines TS.
#
#  Copyright Eric Niebler 2017
#
#  Use, modification and distribution is subject to the
#  Boost Software License, Version 1.0. (See accompanying
#  file LICENSE_1_0.txt or copy at
#  http:#www.boost.org/LICENSE_1_0.txt)
#
# Project home: https://github.com/ericniebler/coronet

cmake_minimum_required(VERSION 3.8)

project(coronet-project CXX)

add_library(coronet INTERFACE)
target_include_directories(coronet INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/cppcoro/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/networking-ts-impl/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/meta/include>
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/cppcoro>
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/net>
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/meta>)
#target_compile_features(coronet INTERFACE cxx_std_17)
target_compile_options(coronet INTERFACE
    $<$<CXX_COMPILER_ID:Clang>:-fcoroutines-ts>
    $<$<CXX_COMPILER_ID:Clang>:-std=gnu++2a>)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include)
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/external/cppcoro/include/
    DESTINATION include/cppcoro)
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/external/networking-ts-impl/include/
    DESTINATION include/net)
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/external/meta/include/
    DESTINATION include/meta)
install(TARGETS coronet EXPORT coronet-project)
install(EXPORT coronet-project DESTINATION coronet-project)

add_subdirectory(external)
add_subdirectory(scratch)

enable_testing()
include(CTest)
# add_subdirectory(test)
