#include "caste.hpp"
#include "common.hpp"

#if defined(__OpenBSD__)

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

static std::vector<bsd_common::GpuCandidate> parse_dmesg_gpus() {
    std::vector<bsd_common::GpuCandidate> out;
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

        bsd_common::GpuCandidate g{};
        bsd_common::apply_name_hints(g, line.substr(quote_a + 1, quote_b - quote_a - 1));
        out.push_back(g);
    }

    pclose(f);
    return out;
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

    bsd_common::GpuCandidate best = bsd_common::pick_best_gpu(gpus);
    bsd_common::apply_gpu_candidate_to_hw(hw, best);

    return hw;
}

#endif
