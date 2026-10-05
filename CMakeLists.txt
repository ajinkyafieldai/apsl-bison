cmake_minimum_required(VERSION 3.20)

project(bison VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

set(APSL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(APSL_BUILD_HOST_TESTS OFF CACHE BOOL "" FORCE)
set(APSL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(APSL_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(APSL_BUILD_INSTALLATION_TESTS OFF CACHE BOOL "" FORCE)
set(APSL_BUILD_LUA ON CACHE BOOL "" FORCE)
set(APSL_BUILD_LOG OFF CACHE BOOL "" FORCE)
set(APSL_BUILD_FREERTOS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    apsl
    GIT_REPOSITORY https://github.com/ajinkyafieldai/apsl.git
    GIT_TAG devel
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(apsl)

option(BISON_BUILD_TESTS "Build Bison host tests" ON)

add_library(bison_cli
    src/apsl_run_transport.cpp
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
target_link_libraries(bison_cli PUBLIC apsl::core apsl::net)

add_executable(bison
    src/main.cpp
)
target_link_libraries(bison PRIVATE bison::cli)

add_executable(bison-server
    src/server_main.cpp
)
target_link_libraries(bison-server PRIVATE bison::cli apsl::net apsl::lua)

if(BISON_BUILD_TESTS)
    enable_testing()
    find_package(Threads REQUIRED)

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

    add_executable(bison_server_integration_test
        tests/server_integration_test.cpp
    )
    target_link_libraries(
        bison_server_integration_test
        PRIVATE
            bison::cli
            apsl::net
            apsl::lua
            Threads::Threads
    )
    add_test(
        NAME bison.server_integration
        COMMAND bison_server_integration_test
    )
endif()
