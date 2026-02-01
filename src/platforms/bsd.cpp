#include "caste.hpp"

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)

HwFacts fill_hw_facts_platform() {
    return HwFacts{};
}

#endif
