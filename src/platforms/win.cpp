#include "caste.hpp"

#if defined(_WIN32)

HwFacts fill_hw_facts_platform() {
    return HwFacts{};
}

#endif
