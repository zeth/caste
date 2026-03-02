#include "caste.hpp"
#include "common.hpp"

#if defined(__DragonFly__)

#include <vector>

HwFacts fill_hw_facts_platform() {
    HwFacts hw{};

    if (!bsd_common::sysctlbyname_u64("hw.physmem64", hw.ram_bytes)) {
        bsd_common::sysctlbyname_u64("hw.physmem", hw.ram_bytes);
    }
    bsd_common::sysctlbyname_int("hw.ncpu", hw.logical_threads);

    auto gpus = bsd_common::parse_pciconf_gpu_records("pciconf -lv 2>/dev/null", bsd_common::PciconfFormat::DragonFlyStyle);
    std::vector<bsd_common::GpuCandidate> scored;
    for (const auto& g : gpus) {
        if (!g.is_gpu) continue;
        bsd_common::GpuCandidate c{};
        bsd_common::apply_vendor_device_hints(c,
                                              bsd_common::to_lower(g.vendor),
                                              bsd_common::to_lower(g.device),
                                              true);
        scored.push_back(c);
    }

    if (scored.empty()) {
        hw.gpu_kind = GpuKind::None;
        return hw;
    }

    bsd_common::GpuCandidate best = bsd_common::pick_best_gpu(scored);
    bsd_common::apply_gpu_candidate_to_hw(hw, best);

    return hw;
}

#endif
