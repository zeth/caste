#include "caste.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    bool want_reason = false;
    bool want_help = false;
    bool want_version = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--reason") {
            want_reason = true;
        } else if (arg == "--help" || arg == "-h") {
            want_help = true;
        } else if (arg == "--version") {
            want_version = true;
        }
    }

    if (want_help) {
        std::cout << "Usage: caste [--reason]\n"
                     "  Prints a single-word hardware class.\n"
                     "  --reason  Include a short explanation.\n"
                     "  --version Show version.\n"
                     "  -h, --help Show this help.\n";
        return 0;
    }

    if (want_version) {
        std::cout << "caste " << CASTE_VERSION << "\n";
        return 0;
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
