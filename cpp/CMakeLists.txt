add_library(util STATIC util.cpp)
add_library(window SHARED window.cpp)
add_library(object SHARED object.cpp)
add_library(mask SHARED mask.cpp)
add_executable(test test.cpp)

if (MSVC)
    target_compile_options(util PRIVATE /utf-8)
    target_compile_options(window PRIVATE /utf-8)
    target_compile_options(object PRIVATE /utf-8)
    target_compile_options(mask PRIVATE /utf-8)
    target_compile_options(test PRIVATE /utf-8)
endif()

include_directories(${CMAKE_SOURCE_DIR}/aviutl2_sdk)

find_package(Python REQUIRED COMPONENTS Interpreter Development)
target_include_directories(window PRIVATE ${Python3_INCLUDE_DIRS})

target_compile_definitions(
    window PRIVATE
    UNICODE
    _UNICODE
)

set_target_properties(window PROPERTIES
    PREFIX ""
    SUFFIX ".aux2"
)

target_compile_definitions(
    object PRIVATE
    UNICODE
    _UNICODE
)

set_target_properties(object PROPERTIES
    PREFIX ""
    SUFFIX ".auf2"
)

target_compile_definitions(
    test PRIVATE
    UNICODE
    _UNICODE
)

set_target_properties(mask PROPERTIES
    PREFIX ""
    SUFFIX ".mod2"
)

target_compile_definitions(
    mask PRIVATE
    UNICODE
    _UNICODE
)


target_link_libraries(window PRIVATE util)
target_link_libraries(object PRIVATE util)
target_link_libraries(test PRIVATE util)