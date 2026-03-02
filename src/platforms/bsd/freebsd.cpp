#include "caste.hpp"
#include "common.hpp"

#if defined(__FreeBSD__)

#include <algorithm>
#include <vector>

HwFacts fill_hw_facts_platform() {
    HwFacts hw{};

    // RAM
    if (!bsd_common::sysctlbyname_u64("hw.physmem64", hw.ram_bytes)) {
        bsd_common::sysctlbyname_u64("hw.physmem", hw.ram_bytes);
    }

    // CPU
    bsd_common::sysctlbyname_int("hw.ncpu", hw.logical_threads);

    // Best-effort physical cores on FreeBSD.
    int cores = 0;
    int threads_per_core = 0;
    if (bsd_common::sysctlbyname_int("kern.smp.cores", cores) && cores > 0) {
        if (bsd_common::sysctlbyname_int("kern.smp.threads_per_core", threads_per_core) && threads_per_core > 0) {
            hw.physical_cores = cores;
            hw.logical_threads = std::max(hw.logical_threads, cores * threads_per_core);
        } else {
            hw.physical_cores = cores;
        }
    }

    // GPU via pciconf
    auto gpus = bsd_common::parse_pciconf_gpu_records("pciconf -lv", bsd_common::PciconfFormat::FreeBsdStyle);
    std::vector<bsd_common::GpuCandidate> scored;
    for (const auto& g : gpus) {
        if (!g.is_gpu) continue;
        bsd_common::GpuCandidate c{};
        bsd_common::apply_vendor_device_hints(c,
                                              bsd_common::to_lower(g.vendor),
                                              bsd_common::to_lower(g.device),
                                              false);
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
