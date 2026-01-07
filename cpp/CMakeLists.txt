add_library(util STATIC util.cpp)
add_library(arb SHARED arb.cpp)
add_library(mask SHARED mask.cpp)
add_executable(test test.cpp)
add_executable(installer WIN32 install.cpp)


set(MANIFEST_FILE ${CMAKE_CURRENT_SOURCE_DIR}/installer.manifest)

if (MSVC)
    target_compile_options(util PRIVATE /utf-8)
    target_compile_options(arb PRIVATE /utf-8)
    target_compile_options(mask PRIVATE /utf-8)
    target_compile_options(test PRIVATE /utf-8)
    target_compile_options(installer PRIVATE /utf-8)

    add_custom_command(
        TARGET installer
        POST_BUILD
        COMMAND mt.exe
            -nologo
            -manifest ${MANIFEST_FILE}
            -outputresource:$<TARGET_FILE:installer>\;1
    )
endif()

# cuda版のtorchをインストールするかどうか
target_compile_definitions(installer PRIVATE USE_CUDA)

# インストール時に既にあるvenvを初期化するかどうか
target_compile_definitions(installer PRIVATE DEBUG)

include_directories(${CMAKE_SOURCE_DIR}/aviutl2_sdk)

find_package(Python REQUIRED COMPONENTS Interpreter Development)
target_include_directories(arb PRIVATE ${Python3_INCLUDE_DIRS})
find_package(CUDAToolkit REQUIRED)

target_compile_definitions(
    arb PRIVATE
    UNICODE
    _UNICODE
)

set_target_properties(arb PROPERTIES
    PREFIX ""
    SUFFIX ".aux2"
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

target_compile_definitions(
    installer PRIVATE
    UNICODE
    _UNICODE
)

set_target_properties(arb PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/result/Plugin
)

set_target_properties(mask PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/result/Script
)

set_target_properties(installer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/result/
)

file(GLOB PY_FILES
    "${CMAKE_SOURCE_DIR}/python/*.py"
)

file(GLOB RQ_TXT
    "${CMAKE_SOURCE_DIR}/python/requirements.txt"
)

file(GLOB SCRIPTS
    "${CMAKE_SOURCE_DIR}/Script/*.anm2"
)

file(COPY ${SCRIPTS}
    DESTINATION ${CMAKE_BINARY_DIR}/result/Script
)

file(COPY ${PY_FILES}
    DESTINATION ${CMAKE_BINARY_DIR}/result/Plugin/ARB/Python
)

file(COPY ${RQ_TXT}
    DESTINATION ${CMAKE_BINARY_DIR}/result/Plugin/ARB/Python
)


target_link_libraries(arb PRIVATE util)
target_link_libraries(arb PRIVATE shlwapi)
target_link_libraries(test PRIVATE util)
target_link_libraries(installer PRIVATE shlwapi)
target_link_libraries(installer PRIVATE CUDA::cudart)