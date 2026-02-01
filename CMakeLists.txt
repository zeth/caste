cmake_minimum_required(VERSION 3.20)

project(caste LANGUAGES CXX)

option(CASTE_BUILD_TESTS "Build caste tests" ON)
option(CASTE_USE_FETCHCONTENT_FALLBACK "Fallback to FetchContent for Catch2" ON)

add_library(caste
    src/caste.cpp
    src/platforms/linux.cpp
    src/platforms/mac.cpp
    src/platforms/bsd.cpp
    src/platforms/win.cpp
)

target_include_directories(caste PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(caste PUBLIC cxx_std_20)

if (CASTE_BUILD_TESTS)
    find_package(Catch2 3 QUIET)
    if (NOT Catch2_FOUND AND CASTE_USE_FETCHCONTENT_FALLBACK)
        include(FetchContent)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.5.4
        )
        FetchContent_MakeAvailable(Catch2)
    endif()
    if (NOT Catch2_FOUND)
        message(FATAL_ERROR "Catch2 v3 not found. Install Catch2 or set CASTE_USE_FETCHCONTENT_FALLBACK=ON.")
    endif()
    enable_testing()
    add_executable(caste_tests tests/test_classify.cpp)
    target_link_libraries(caste_tests PRIVATE caste Catch2::Catch2WithMain)
    add_test(NAME caste_tests COMMAND caste_tests)
endif()
