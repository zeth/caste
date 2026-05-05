cmake_minimum_required(VERSION 3.20)

project(caste VERSION 0.1.6 LANGUAGES CXX)

option(CASTE_BUILD_TESTS "Build caste tests" ON)
option(CASTE_BUILD_PYTHON "Build Python extension module" OFF)

set(CASTE_PLATFORM_SOURCES)
if (WIN32)
    list(APPEND CASTE_PLATFORM_SOURCES src/platforms/win.cpp)
elseif (APPLE)
    list(APPEND CASTE_PLATFORM_SOURCES src/platforms/mac.cpp)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND CASTE_PLATFORM_SOURCES src/platforms/linux.cpp)
elseif (CMAKE_SYSTEM_NAME MATCHES "^(FreeBSD|NetBSD|OpenBSD|DragonFly)$")
    list(APPEND CASTE_PLATFORM_SOURCES
        src/platforms/bsd/common.cpp
        src/platforms/bsd/dragonfly.cpp
        src/platforms/bsd/freebsd.cpp
        src/platforms/bsd/netbsd.cpp
        src/platforms/bsd/openbsd.cpp
    )
endif()


option(CASTE_BUILD_SHARED "Build libcaste as a shared library" ON)

if (CASTE_BUILD_SHARED)
    add_library(caste SHARED
        src/caste.cpp
        ${CASTE_PLATFORM_SOURCES}
    )
else()
    add_library(caste STATIC
        src/caste.cpp
        ${CASTE_PLATFORM_SOURCES}
    )
endif()



set_target_properties(caste PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    POSITION_INDEPENDENT_CODE ON
)

if (WIN32)
    set_target_properties(caste PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
    target_link_libraries(caste PRIVATE dxgi)
endif()
if (APPLE)
    target_link_libraries(caste PRIVATE "-framework CoreFoundation" "-framework IOKit")
endif()

target_include_directories(caste
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/caste>
)
target_compile_features(caste PUBLIC cxx_std_20)
target_compile_definitions(caste PUBLIC CASTE_VERSION="${PROJECT_VERSION}")

option(CASTE_BUILD_CLI "Build caste command-line binary" ON)
if (CASTE_BUILD_CLI)
    add_executable(caste_cli src/caste_cli.cpp)
    set_target_properties(caste_cli PROPERTIES OUTPUT_NAME caste)
    target_link_libraries(caste_cli PRIVATE caste)
endif()

if (CASTE_BUILD_PYTHON)
    add_subdirectory(python)
endif()

if (CASTE_BUILD_TESTS)
    find_package(Catch2 3 QUIET)
    if (NOT Catch2_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG abb467ecd60fae9a727afca033c1eb5d20af2c12 # v3.5.4
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(Catch2)
    endif()
    if (NOT TARGET Catch2::Catch2WithMain)
        message(FATAL_ERROR "Catch2 v3 not found and FetchContent failed. Install Catch2 or allow FetchContent to download it.")
    endif()
    enable_testing()
    add_executable(caste_tests tests/test_classify.cpp)
    target_link_libraries(caste_tests PRIVATE caste Catch2::Catch2WithMain)
    add_test(NAME caste_tests COMMAND caste_tests)
    if (CASTE_BUILD_CLI)
        add_executable(caste_cli_tests tests/test_cli.cpp)
        target_link_libraries(caste_cli_tests PRIVATE Catch2::Catch2WithMain)
        target_compile_features(caste_cli_tests PRIVATE cxx_std_20)
        target_compile_definitions(caste_cli_tests PRIVATE CASTE_CLI_PATH="$<TARGET_FILE:caste_cli>")
        add_test(NAME caste_cli_tests COMMAND caste_cli_tests)
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS caste
    EXPORT casteTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/caste.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/caste
)

if (CASTE_BUILD_CLI)
    install(TARGETS caste_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/man/caste.1
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
)

install(EXPORT casteTargets
    NAMESPACE caste::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/caste
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/casteConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/casteConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/casteConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/caste
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/casteConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/casteConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/caste
)
