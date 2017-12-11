file(GLOB CPPCORO_SRC_FILES cppcoro/lib/*.cpp)
list(REMOVE_ITEM CPPCORO_SRC_FILES "${CMAKE_SOURCE_DIR}/external/cppcoro/lib/file.cpp")
list(REMOVE_ITEM CPPCORO_SRC_FILES "${CMAKE_SOURCE_DIR}/external/cppcoro/lib/io_service.cpp")
list(REMOVE_ITEM CPPCORO_SRC_FILES "${CMAKE_SOURCE_DIR}/external/cppcoro/lib/read_only_file.cpp")
list(REMOVE_ITEM CPPCORO_SRC_FILES "${CMAKE_SOURCE_DIR}/external/cppcoro/lib/read_write_file.cpp")
list(REMOVE_ITEM CPPCORO_SRC_FILES "${CMAKE_SOURCE_DIR}/external/cppcoro/lib/write_only_file.cpp")
list(REMOVE_ITEM CPPCORO_SRC_FILES "${CMAKE_SOURCE_DIR}/external/cppcoro/lib/win32.cpp")
add_library(CppCoroLib ${CPPCORO_SRC_FILES})
target_include_directories(CppCoroLib PUBLIC cppcoro/include)
target_compile_options(CppCoroLib PUBLIC
    $<$<CXX_COMPILER_ID:Clang>:-fcoroutines-ts>
    $<$<CXX_COMPILER_ID:Clang>:-std=gnu++2a>)
