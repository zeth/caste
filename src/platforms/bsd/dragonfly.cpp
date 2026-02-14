#include "caste.hpp"
#include "common.hpp"

#if defined(__DragonFly__)

#include <cstdio>
#include <cstdlib>
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
    FILE* f = popen("pciconf -lv 2>/dev/null", "r");
    if (!f) return out;

    char buf[512];
    PciconfGpuRecord cur{};
    bool in_record = false;

    auto flush = [&](){
        if (in_record) out.push_back(cur);
        cur = PciconfGpuRecord{};
        in_record = false;
    };

    while (fgets(buf, sizeof(buf), f)) {
        std::string raw = buf;
        std::string line = bsd_common::trim(raw);
        if (line.empty()) continue;

        // DragonFly record header is non-indented:
        // vgapci0@pci0:0:2:0:  class=0x030000 ...
        bool is_header = !raw.empty() && raw[0] != ' ' && raw[0] != '\t';
        if (is_header) {
            flush();
            in_record = true;
            auto cls_pos = line.find("class=0x");
            if (cls_pos != std::string::npos) {
                std::string cls_hex = line.substr(cls_pos + 8);
                auto sp = cls_hex.find(' ');
                if (sp != std::string::npos) cls_hex = cls_hex.substr(0, sp);
                uint32_t cls = 0;
                if (parse_hex_u32(cls_hex, cls)) {
                    if ((cls & 0xFF0000u) == 0x030000u) cur.is_gpu = true;
                }
            }
            continue;
        }

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = bsd_common::trim(line.substr(0, pos));
        std::string val = bsd_common::trim(line.substr(pos + 1));
        if (key == "vendor") {
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

    if (!bsd_common::sysctlbyname_u64("hw.physmem64", hw.ram_bytes)) {
        bsd_common::sysctlbyname_u64("hw.physmem", hw.ram_bytes);
    }
    bsd_common::sysctlbyname_int("hw.ncpu", hw.logical_threads);

    auto gpus = parse_pciconf_gpus();
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
