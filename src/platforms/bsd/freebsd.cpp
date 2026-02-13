#include "caste.hpp"
#include "common.hpp"

#if defined(__FreeBSD__)

#include <algorithm>
#include <cstdio>
#include <cstring>
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

static bool sysctl_int(const char* name, int& out) {
    int v = 0;
    size_t len = sizeof(v);
    if (sysctlbyname(name, &v, &len, nullptr, 0) != 0) return false;
    out = v;
    return true;
}

struct GpuCandidate {
    bool is_gpu = false;
    bool is_discrete_hint = false;
    bool is_intel_arc_hint = false;
    std::string vendor;
    std::string device;
};

static bool parse_hex_u32(const std::string& s, uint32_t& out) {
    char* end = nullptr;
    unsigned long v = std::strtoul(s.c_str(), &end, 16);
    if (!end || end == s.c_str()) return false;
    out = (uint32_t)v;
    return true;
}

static std::vector<GpuCandidate> parse_pciconf_gpus() {
    std::vector<GpuCandidate> out;
    FILE* f = popen("pciconf -lv", "r");
    if (!f) return out;

    char buf[512];
    GpuCandidate cur{};
    bool has_any = false;

    auto flush = [&](){
        if (has_any) out.push_back(cur);
        cur = GpuCandidate{};
        has_any = false;
    };

    while (fgets(buf, sizeof(buf), f)) {
        std::string line = bsd_common::trim(buf);
        if (line.empty()) {
            flush();
            continue;
        }
        has_any = true;

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = bsd_common::trim(line.substr(0, pos));
        std::string val = bsd_common::trim(line.substr(pos + 1));

        if (key == "class") {
            // e.g. 0x030000, 0x030200, 0x038000
            if (val.rfind("0x", 0) == 0) val = val.substr(2);
            uint32_t cls = 0;
            if (parse_hex_u32(val, cls)) {
                if ((cls & 0xFF0000u) == 0x030000u) cur.is_gpu = true;
            }
        } else if (key == "vendor") {
            cur.vendor = val;
        } else if (key == "device") {
            cur.device = val;
        }
    }
    flush();

    pclose(f);

    std::vector<GpuCandidate> gpus;
    for (auto& g : out) {
        if (g.is_gpu) gpus.push_back(g);
    }
    return gpus;
}

static GpuCandidate pick_best_gpu(const std::vector<GpuCandidate>& gpus) {
    auto score = [](const GpuCandidate& g) -> int {
        int s = 0;
        if (g.is_discrete_hint) s += 1000;
        if (g.is_intel_arc_hint) s += 100;
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

    // RAM
    if (!sysctl_u64("hw.physmem64", hw.ram_bytes)) {
        sysctl_u64("hw.physmem", hw.ram_bytes);
    }

    // CPU
    sysctl_int("hw.ncpu", hw.logical_threads);

    // Best-effort physical cores on FreeBSD.
    int cores = 0;
    int threads_per_core = 0;
    if (sysctl_int("kern.smp.cores", cores) && cores > 0) {
        if (sysctl_int("kern.smp.threads_per_core", threads_per_core) && threads_per_core > 0) {
            hw.physical_cores = cores;
            hw.logical_threads = std::max(hw.logical_threads, cores * threads_per_core);
        } else {
            hw.physical_cores = cores;
        }
    }

    // GPU via pciconf
    auto gpus = parse_pciconf_gpus();
    if (gpus.empty()) {
        hw.gpu_kind = GpuKind::None;
        return hw;
    }

    for (auto& g : gpus) {
        std::string vendor = bsd_common::to_lower(g.vendor);
        std::string device = bsd_common::to_lower(g.device);

        if (vendor.find("nvidia") != std::string::npos) {
            g.is_discrete_hint = true;
        } else if (vendor.find("advanced micro devices") != std::string::npos ||
                   vendor.find("amd") != std::string::npos) {
            g.is_discrete_hint = true;
        } else if (vendor.find("intel") != std::string::npos) {
            g.is_discrete_hint = false;
        }

        if (device.find("arc") != std::string::npos) {
            g.is_intel_arc_hint = true;
        }
    }

    GpuCandidate best = pick_best_gpu(gpus);
    hw.is_intel_arc = best.is_intel_arc_hint;

    if (best.is_discrete_hint) {
        hw.gpu_kind = GpuKind::Discrete;
        hw.has_discrete_gpu = true;
    } else {
        hw.gpu_kind = GpuKind::Integrated;
        hw.has_discrete_gpu = false;
    }

    return hw;
}

#endif
