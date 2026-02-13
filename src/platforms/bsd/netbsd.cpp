#include "caste.hpp"
#include "common.hpp"

#if defined(__NetBSD__)

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/sysctl.h>

namespace {

static bool sysctl_u64(const char* name, uint64_t& out) {
    uint64_t v = 0;
    size_t len = sizeof(v);
    if (sysctlbyname(name, &v, &len, nullptr, 0) != 0) return false;
    out = v;
    return true;
}

static bool sysctl_i64(const char* name, int64_t& out) {
    int64_t v = 0;
    size_t len = sizeof(v);
    if (sysctlbyname(name, &v, &len, nullptr, 0) != 0) return false;
    out = v;
    return true;
}

static bool sysctl_int(const char* name, int& out) {
    int v = 0;
    size_t len = sizeof(v);
    if (sysctlbyname(name, &v, &len, nullptr, 0) != 0) return false;
    out = v;
    return true;
}

struct GpuCandidate {
    bool is_discrete_hint = false;
    bool is_virtual_hint = false;
    bool is_intel_arc_hint = false;
    std::string name;
};

static std::vector<GpuCandidate> parse_pcictl_gpus() {
    std::vector<GpuCandidate> out;
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

        GpuCandidate g{};
        g.name = bsd_common::to_lower(bsd_common::trim(desc.substr(0, lparen)));

        if (g.name.find("nvidia") != std::string::npos ||
            g.name.find("amd") != std::string::npos ||
            g.name.find("radeon") != std::string::npos ||
            g.name.find("geforce") != std::string::npos ||
            g.name.find("quadro") != std::string::npos) {
            g.is_discrete_hint = true;
        }

        if (g.name.find("arc") != std::string::npos) {
            g.is_intel_arc_hint = true;
        }

        if (bsd_common::contains_any(g.name, {"qxl", "virtio", "vmware", "virtualbox", "bochs", "cirrus"})) {
            g.is_virtual_hint = true;
        }

        out.push_back(g);
    }

    pclose(f);
    return out;
}

static GpuCandidate pick_best_gpu(const std::vector<GpuCandidate>& gpus) {
    auto score = [](const GpuCandidate& g) -> int {
        int s = 0;
        if (g.is_discrete_hint) s += 1000;
        if (g.is_intel_arc_hint) s += 100;
        if (g.is_virtual_hint) s -= 500;
        return s;
    };
    if (gpus.empty()) return {};
    return *std::max_element(gpus.begin(), gpus.end(),
                             [&](const GpuCandidate& a, const GpuCandidate& b){
                                 return score(a) < score(b);
                             });
}

} // namespace

HwFacts fill_hw_facts_platform() {
    HwFacts hw{};

    // RAM: hw.physmem64 is reliable on NetBSD. hw.physmem may be -1 on some systems.
    if (!sysctl_u64("hw.physmem64", hw.ram_bytes)) {
        int64_t physmem = 0;
        if (sysctl_i64("hw.physmem", physmem) && physmem > 0) {
            hw.ram_bytes = (uint64_t)physmem;
        }
    }

    // CPU threads
    sysctl_int("hw.ncpu", hw.logical_threads);

    // GPU via pcictl list output. Keep this conservative: no VRAM claims.
    auto gpus = parse_pcictl_gpus();
    if (gpus.empty()) {
        hw.gpu_kind = GpuKind::None;
        return hw;
    }

    GpuCandidate best = pick_best_gpu(gpus);
    hw.is_intel_arc = best.is_intel_arc_hint;

    if (best.is_discrete_hint) {
        hw.gpu_kind = GpuKind::Discrete;
        hw.has_discrete_gpu = true;
    } else if (best.is_virtual_hint) {
        hw.gpu_kind = GpuKind::None;
        hw.has_discrete_gpu = false;
    } else {
        hw.gpu_kind = GpuKind::Integrated;
        hw.has_discrete_gpu = false;
    }

    return hw;
}

#endif
