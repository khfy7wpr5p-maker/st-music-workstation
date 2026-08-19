cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are prohibited. Configure with: cmake -S . -B build")
endif()

project(STMusicWorkstation
    LANGUAGES CXX
)

include(CTest)

option(ST_STRICT_WARNINGS "Treat compiler warnings as errors for ST-owned targets" ON)

function(st_apply_strict_warnings target)
    if(NOT ST_STRICT_WARNINGS)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    else()
        message(WARNING "ST_STRICT_WARNINGS requested, but no reviewed warning profile exists for compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endfunction()

add_library(st_core STATIC
    src/st/core/identity.cpp
)

target_include_directories(st_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(st_core PUBLIC cxx_std_20)
set_target_properties(st_core PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
st_apply_strict_warnings(st_core)

add_executable(st_build_contract_test
    tests/cpp/build_contract_test.cpp
)

target_compile_features(st_build_contract_test PRIVATE cxx_std_20)
set_target_properties(st_build_contract_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
st_apply_strict_warnings(st_build_contract_test)

add_executable(st_identity_test
    tests/cpp/identity_test.cpp
)

target_link_libraries(st_identity_test PRIVATE st_core)
set_target_properties(st_identity_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
st_apply_strict_warnings(st_identity_test)

if(BUILD_TESTING)
    add_test(NAME st.build.contract COMMAND st_build_contract_test)
    add_test(NAME st.core.identity COMMAND st_identity_test)
endif()
