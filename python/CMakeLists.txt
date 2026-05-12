cmake_minimum_required(VERSION 3.20)
project(caste_python LANGUAGES CXX)

if (NOT TARGET caste)
    set(CASTE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(CASTE_BUILD_PYTHON OFF CACHE BOOL "" FORCE)
    set(CASTE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CASTE_BUILD_CLI OFF CACHE BOOL "" FORCE)
    add_subdirectory(".." "${CMAKE_CURRENT_BINARY_DIR}/caste-core")
endif()

find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 CONFIG REQUIRED)

option(CASTE_DEBIAN_INSTALL_LAYOUT "Use Debian Python install layout" OFF)

pybind11_add_module(_caste caste_bindings.cpp)
target_link_libraries(_caste PRIVATE caste)
target_include_directories(_caste PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../src)

set_target_properties(_caste PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)

if (CASTE_DEBIAN_INSTALL_LAYOUT)
    execute_process(
        COMMAND "${Python_EXECUTABLE}" -c "import sysconfig; print(sysconfig.get_path('platlib'))"
        OUTPUT_VARIABLE PYTHON_PLATLIB
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    install(FILES "${CMAKE_CURRENT_LIST_DIR}/caste/__init__.py"
        DESTINATION "${PYTHON_PLATLIB}/caste"
    )
    install(TARGETS _caste
        LIBRARY DESTINATION "${PYTHON_PLATLIB}/caste"
    )
else()
    install(TARGETS _caste
        LIBRARY DESTINATION caste
        RUNTIME DESTINATION caste
        ARCHIVE DESTINATION caste
    )
endif()
