#include "caste.hpp"
#include "common.hpp"

#if defined(__FreeBSD__)

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

namespace {

struct PciconfGpuRecord {
    bool is_gpu = false;
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

static std::vector<PciconfGpuRecord> parse_pciconf_gpus() {
    std::vector<PciconfGpuRecord> out;
    FILE* f = popen("pciconf -lv", "r");
    if (!f) return out;

    char buf[512];
    PciconfGpuRecord cur{};
    bool has_any = false;

    auto flush = [&](){
        if (has_any) out.push_back(cur);
        cur = PciconfGpuRecord{};
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
    return out;
}

} // namespace

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
    auto gpus = parse_pciconf_gpus();
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
