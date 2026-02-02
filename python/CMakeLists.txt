cmake_minimum_required(VERSION 3.20)
project(caste_python LANGUAGES CXX)

# Build the core library from the repo root, but disable its own Python/CLI/tests.
set(CASTE_BUILD_PYTHON OFF CACHE BOOL "" FORCE)
set(CASTE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CASTE_BUILD_CLI OFF CACHE BOOL "" FORCE)

add_subdirectory(".." "${CMAKE_CURRENT_BINARY_DIR}/caste-core")

# Tell pybind11 to use modern FindPython (reduces noisy warnings on newer CMake)
set(PYBIND11_FINDPYTHON ON)

find_package(pybind11 CONFIG REQUIRED)

# Build the extension module.
pybind11_add_module(_caste caste_bindings.cpp)
target_link_libraries(_caste PRIVATE caste)
target_include_directories(_caste PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../src)

set_target_properties(_caste PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)

install(TARGETS _caste
    LIBRARY DESTINATION caste
    RUNTIME DESTINATION caste
    ARCHIVE DESTINATION caste
)
