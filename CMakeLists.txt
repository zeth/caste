cmake_minimum_required(VERSION 3.20)

project(caste VERSION 0.1.0 LANGUAGES CXX)

option(CASTE_BUILD_TESTS "Build caste tests" ON)

add_library(caste
    src/caste.cpp
    src/platforms/linux.cpp
    src/platforms/mac.cpp
    src/platforms/bsd.cpp
    src/platforms/win.cpp
)

if (WIN32)
    target_link_libraries(caste PRIVATE dxgi)
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

if (CASTE_BUILD_TESTS)
    find_package(Catch2 3 QUIET)
    if (NOT Catch2_FOUND)
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
