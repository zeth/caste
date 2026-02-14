#include "caste.hpp"
#include "common.hpp"

#if defined(__DragonFly__)

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
    bool is_virtual_hint = false;
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
    FILE* f = popen("pciconf -lv 2>/dev/null", "r");
    if (!f) return out;

    char buf[512];
    GpuCandidate cur{};
    bool in_record = false;

    auto flush = [&](){
        if (in_record) out.push_back(cur);
        cur = GpuCandidate{};
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

    if (!sysctl_u64("hw.physmem64", hw.ram_bytes)) {
        sysctl_u64("hw.physmem", hw.ram_bytes);
    }
    sysctl_int("hw.ncpu", hw.logical_threads);

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
        if (bsd_common::contains_any(vendor, {"red hat", "vmware", "virtualbox", "bochs", "cirrus"}) ||
            bsd_common::contains_any(device, {"qxl", "virtio", "vmware", "virtualbox", "bochs", "cirrus"})) {
            g.is_virtual_hint = true;
        }
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
