cmake_minimum_required(VERSION 3.20)

project(bison VERSION 0.1.0 LANGUAGES CXX)

option(BISON_BUILD_TESTS "Build Bison host tests" ON)

add_library(bison_cli
    src/cli.cpp
    src/run_client.cpp
)
add_library(bison::cli ALIAS bison_cli)
target_compile_features(bison_cli PUBLIC cxx_std_20)
target_include_directories(
    bison_cli
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

add_executable(bison
    src/main.cpp
)
target_link_libraries(bison PRIVATE bison::cli)

if(BISON_BUILD_TESTS)
    enable_testing()

    add_executable(bison_cli_test
        tests/cli_test.cpp
    )
    target_link_libraries(bison_cli_test PRIVATE bison::cli)
    add_test(NAME bison.cli COMMAND bison_cli_test)

    add_executable(bison_run_client_test
        tests/run_client_test.cpp
    )
    target_link_libraries(bison_run_client_test PRIVATE bison::cli)
    add_test(NAME bison.run_client COMMAND bison_run_client_test)
endif()
