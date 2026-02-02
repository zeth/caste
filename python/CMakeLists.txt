find_package(pybind11 CONFIG REQUIRED)

# Build the extension module.
pybind11_add_module(_caste
    caste_bindings.cpp
)

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
