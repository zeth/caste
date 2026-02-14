#include "caste.hpp"

#include <iostream>
#include <string>

static const char* gpu_kind_name(GpuKind k) {
    switch (k) {
        case GpuKind::None: return "None";
        case GpuKind::Integrated: return "Integrated";
        case GpuKind::Unified: return "Unified";
        case GpuKind::Discrete: return "Discrete";
    }
    return "Unknown";
}

int main(int argc, char** argv) {
    bool want_reason = false;
    bool want_help = false;
    bool want_version = false;
    bool want_hwfacts = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--reason") {
            want_reason = true;
        } else if (arg == "--hwfacts") {
            want_hwfacts = true;
        } else if (arg == "--help" || arg == "-h") {
            want_help = true;
        } else if (arg == "--version") {
            want_version = true;
        }
    }

    if (want_help) {
        std::cout << "Usage: caste [--reason] [--hwfacts]\n"
                     "  Prints a single-word hardware class.\n"
                     "  --reason  Include a short explanation.\n"
                     "  --hwfacts Print detected hardware facts.\n"
                     "  --version Show version.\n"
                     "  -h, --help Show this help.\n";
        return 0;
    }

    if (want_version) {
        std::cout << "caste " << CASTE_VERSION << "\n";
        return 0;
    }

    if (want_hwfacts) {
        HwFacts hw = detect_hw_facts();
        std::cout << "ram_bytes=" << hw.ram_bytes << "\n";
        std::cout << "physical_cores=" << hw.physical_cores << "\n";
        std::cout << "logical_threads=" << hw.logical_threads << "\n";
        std::cout << "gpu_kind=" << gpu_kind_name(hw.gpu_kind) << "\n";
        std::cout << "vram_bytes=" << hw.vram_bytes << "\n";
        std::cout << "has_discrete_gpu=" << (hw.has_discrete_gpu ? "true" : "false") << "\n";
        std::cout << "is_apple_silicon=" << (hw.is_apple_silicon ? "true" : "false") << "\n";
        std::cout << "is_intel_arc=" << (hw.is_intel_arc ? "true" : "false") << "\n";
        if (want_reason) std::cout << "\n";
    }

    if (!want_reason) {
        std::cout << detect_caste_word() << "\n";
        return 0;
    }

    CasteResult result = detect_caste();
    std::cout << caste_name(result.caste);
    if (!result.reason.empty()) {
        std::cout << ": " << result.reason;
    }
    std::cout << "\n";
    return 0;
}
