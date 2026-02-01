#include "caste.hpp"

#if defined(__APPLE__) && defined(__MACH__)

HwFacts fill_hw_facts_platform() {
    return HwFacts{};
}

#endif
