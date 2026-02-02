#include "caste.hpp"

#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(_caste, m) {
    m.doc() = "Caste hardware classification (native extension)";

    m.def("detect_caste_word", &detect_caste_word,
        "Return the hardware classification as a single string (e.g., 'User', 'Developer').");

    m.def("detect_caste", []() {
        CasteResult r = detect_caste();
        return py::make_tuple(std::string(caste_name(r.caste)), r.reason);
    }, "Return a tuple of (caste_name, reason_string) explaining the classification.");

    m.def("detect_hw_facts", []() {
        HwFacts hw = detect_hw_facts();
        py::dict d;
        d["ram_bytes"] = py::int_(hw.ram_bytes);
        d["physical_cores"] = py::int_(hw.physical_cores);
        d["logical_threads"] = py::int_(hw.logical_threads);
        d["gpu_kind"] = py::int_(static_cast<int>(hw.gpu_kind));
        d["vram_bytes"] = py::int_(hw.vram_bytes);
        d["has_discrete_gpu"] = py::bool_(hw.has_discrete_gpu);
        d["is_apple_silicon"] = py::bool_(hw.is_apple_silicon);
        d["is_intel_arc"] = py::bool_(hw.is_intel_arc);
        return d;
    }, "Return a dictionary containing raw hardware facts detected on the system.");

#ifdef CASTE_VERSION
    m.attr("__version__") = CASTE_VERSION;
#else
    m.attr("__version__") = "0.0.0";
#endif
}
