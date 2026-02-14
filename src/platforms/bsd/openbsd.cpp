#include "caste.hpp"
#include "common.hpp"

#if defined(__OpenBSD__)

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

namespace {

static bool sysctl_u64(const char* name, uint64_t& out) {
    std::string cmd = "sysctl -n ";
    cmd += name;
    cmd += " 2>/dev/null";

    FILE* f = popen(cmd.c_str(), "r");
    if (!f) return false;

    char buf[128];
    if (!fgets(buf, sizeof(buf), f)) {
        pclose(f);
        return false;
    }
    pclose(f);

    std::string s = bsd_common::trim(buf);
    if (s.empty()) return false;

    errno = 0;
    char* end = nullptr;
    unsigned long long v = std::strtoull(s.c_str(), &end, 10);
    if (errno != 0 || end == s.c_str()) return false;
    out = static_cast<uint64_t>(v);
    return true;
}

static bool sysctl_int(const char* name, int& out) {
    uint64_t v = 0;
    if (!sysctl_u64(name, v)) return false;
    if (v > static_cast<uint64_t>(std::numeric_limits<int>::max())) return false;
    int iv = static_cast<int>(v);
    if (iv <= 0) return false;
    out = iv;
    return true;
}

struct GpuCandidate {
    bool is_discrete_hint = false;
    bool is_virtual_hint = false;
    bool is_intel_arc_hint = false;
    std::string name;
};

static std::vector<GpuCandidate> parse_dmesg_gpus() {
    std::vector<GpuCandidate> out;
    FILE* f = popen("dmesg 2>/dev/null", "r");
    if (!f) return out;

    char buf[512];
    while (fgets(buf, sizeof(buf), f)) {
        std::string line = bsd_common::to_lower(bsd_common::trim(buf));
        if (line.empty()) continue;

        // Example:
        // vga1 at pci0 dev 2 function 0 "Red Hat QXL Video" rev 0x05
        if (line.find(" vga") == std::string::npos && line.rfind("vga", 0) != 0) continue;
        auto quote_a = line.find('"');
        auto quote_b = (quote_a == std::string::npos) ? std::string::npos : line.find('"', quote_a + 1);
        if (quote_a == std::string::npos || quote_b == std::string::npos || quote_b <= quote_a + 1) continue;

        GpuCandidate g{};
        g.name = line.substr(quote_a + 1, quote_b - quote_a - 1);

        if (bsd_common::contains_any(g.name, {"nvidia", "amd", "radeon", "geforce", "quadro"})) {
            g.is_discrete_hint = true;
        }
        if (bsd_common::contains_any(g.name, {"qxl", "virtio", "vmware", "virtualbox", "bochs", "cirrus"})) {
            g.is_virtual_hint = true;
        }
        if (g.name.find("arc") != std::string::npos) {
            g.is_intel_arc_hint = true;
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

    // OpenBSD reports physical memory as bytes in hw.physmem.
    sysctl_u64("hw.physmem", hw.ram_bytes);
    sysctl_int("hw.ncpu", hw.logical_threads);

    auto gpus = parse_dmesg_gpus();
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
