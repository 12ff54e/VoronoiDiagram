cmake_minimum_required(VERSION 3.15.0)
project(BSplineDemo VERSION 0.1.0)

# Requires c++17 and no compiler extension
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(index src/main.cpp)
set_target_properties(index PROPERTIES SUFFIX .html)

string(JOIN "" shell_file ${CMAKE_CURRENT_SOURCE_DIR}/src/shell_minimal.html)

target_link_options(index PUBLIC -sEXPORTED_FUNCTIONS=_main,_draw_some_sites,_set_method)
target_link_options(index PUBLIC --shell-file ${shell_file}) # specify html template
target_link_options(index PUBLIC -sMIN_WEBGL_VERSION=2) # use webgl2
target_link_options(index PUBLIC -sOFFSCREENCANVAS_SUPPORT) # off screen canvas
target_link_options(index PUBLIC --embed-file ${CMAKE_CURRENT_SOURCE_DIR}/src/shader@shader) # embed shader source files
target_compile_options(index PUBLIC -fwasm-exceptions) # allow exceptions
target_link_options(index PUBLIC -fwasm-exceptions) # allow exceptions
# target_compile_definitions(index PUBLIC "EXPLICIT_SWAP")

target_compile_options(index PUBLIC -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-pre-c++14-compat -Wno-exit-time-destructors -Wno-padded -Wno-unsafe-buffer-usage)

# trigger recompile when shader files change
file(GLOB shader_src CONFIGURE_DEPENDS src/shader/*)
set_property(TARGET index PROPERTY LINK_DEPENDS ${shader_src} ${shell_file})
# set_property(TARGET index PROPERTY LINK_DEPENDS ${shell_file})

include(CTest)
# Specify tests
list(APPEND tests "vec-test")

list(LENGTH tests test_num)
message(STATUS)
message(STATUS  ${test_num} " tests found: ")
enable_testing()
foreach(test ${tests})
    string(JOIN "" test_src_file test/ ${test} .cpp)
    add_executable(${test} ${test_src_file})

    string(JOIN "-" test_name vd ${test})
    add_test(NAME ${test_name}
    COMMAND ${CROSSCOMPILING_EMULATOR} ${test})
    message(STATUS ${test} " from source file " ${test_src_file})
endforeach(test ${tests})


set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
