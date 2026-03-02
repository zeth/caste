#include "caste.hpp"
#include "common.hpp"

#if defined(__NetBSD__)

#include <cstdio>
#include <string>
#include <vector>

namespace {

static std::vector<bsd_common::GpuCandidate> parse_pcictl_gpus() {
    std::vector<bsd_common::GpuCandidate> out;
    FILE* f = popen("pcictl pci0 list 2>/dev/null", "r");
    if (!f) return out;

    char buf[512];
    while (fgets(buf, sizeof(buf), f)) {
        std::string line = bsd_common::trim(buf);
        if (line.empty()) continue;

        // e.g. "000:02:0: Red Hat QXL Video (VGA display, revision 0x05)"
        auto sep = line.find(": ");
        if (sep == std::string::npos) continue;
        std::string desc = line.substr(sep + 2);

        auto lparen = desc.rfind('(');
        auto rparen = desc.rfind(')');
        if (lparen == std::string::npos || rparen == std::string::npos || lparen >= rparen) continue;

        std::string class_desc = bsd_common::to_lower(bsd_common::trim(desc.substr(lparen + 1, rparen - lparen - 1)));
        if (class_desc.find("display") == std::string::npos) continue;

        bsd_common::GpuCandidate g{};
        std::string name = bsd_common::to_lower(bsd_common::trim(desc.substr(0, lparen)));
        bsd_common::apply_name_hints(g, name);
        out.push_back(g);
    }

    pclose(f);
    return out;
}

} // namespace

HwFacts fill_hw_facts_platform() {
    HwFacts hw{};

    // RAM: hw.physmem64 is reliable on NetBSD. hw.physmem may be -1 on some systems.
    if (!bsd_common::sysctlbyname_u64("hw.physmem64", hw.ram_bytes)) {
        int64_t physmem = 0;
        if (bsd_common::sysctlbyname_i64("hw.physmem", physmem) && physmem > 0) {
            hw.ram_bytes = (uint64_t)physmem;
        }
    }

    // CPU threads
    bsd_common::sysctlbyname_int("hw.ncpu", hw.logical_threads);

    // GPU via pcictl list output. Keep this conservative: no VRAM claims.
    auto gpus = parse_pcictl_gpus();
    if (gpus.empty()) {
        hw.gpu_kind = GpuKind::None;
        return hw;
    }

    bsd_common::GpuCandidate best = bsd_common::pick_best_gpu(gpus);
    bsd_common::apply_gpu_candidate_to_hw(hw, best);

    return hw;
}

#endif
