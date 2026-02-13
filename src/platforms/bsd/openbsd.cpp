#include "caste.hpp"

#if defined(__OpenBSD__)

HwFacts fill_hw_facts_platform() {
    return HwFacts{};
}

#endif
