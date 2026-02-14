#include "common.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

namespace bsd_common {

static bool parse_hex_u32(const std::string& s, uint32_t& out) {
    char* end = nullptr;
    unsigned long v = std::strtoul(s.c_str(), &end, 16);
    if (!end || end == s.c_str()) return false;
    out = (uint32_t)v;
    return true;
}

std::string trim(std::string s) {
    auto notspace = [](unsigned char c){ return c != ' ' && c != '\t' && c != '\n' && c != '\r'; };
    while (!s.empty() && !notspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && !notspace((unsigned char)s.back())) s.pop_back();
    return s;
}

std::string to_lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

bool contains_any(const std::string& haystack, std::initializer_list<const char*> needles) {
    for (const char* needle : needles) {
        if (haystack.find(needle) != std::string::npos) return true;
    }
    return false;
}

std::vector<PciconfGpuRecord> parse_pciconf_gpu_records(const char* cmd, PciconfFormat format) {
    std::vector<PciconfGpuRecord> out;
    FILE* f = popen(cmd, "r");
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
        std::string line = trim(raw);
        if (line.empty()) {
            if (format == PciconfFormat::FreeBsdStyle) {
                flush();
            }
            continue;
        }

        if (format == PciconfFormat::DragonFlyStyle) {
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
                    if (parse_hex_u32(cls_hex, cls) && (cls & 0xFF0000u) == 0x030000u) {
                        cur.is_gpu = true;
                    }
                }
                continue;
            }
        } else {
            in_record = true;
        }

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = trim(line.substr(0, pos));
        std::string val = trim(line.substr(pos + 1));

        if (format == PciconfFormat::FreeBsdStyle && key == "class") {
            if (val.rfind("0x", 0) == 0) val = val.substr(2);
            uint32_t cls = 0;
            if (parse_hex_u32(val, cls) && (cls & 0xFF0000u) == 0x030000u) {
                cur.is_gpu = true;
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

void apply_name_hints(GpuCandidate& gpu, const std::string& name_lower) {
    if (contains_any(name_lower, {"nvidia", "amd", "radeon", "geforce", "quadro"})) {
        gpu.is_discrete_hint = true;
    }
    if (contains_any(name_lower, {"qxl", "virtio", "vmware", "virtualbox", "bochs", "cirrus"})) {
        gpu.is_virtual_hint = true;
    }
    if (name_lower.find("arc") != std::string::npos) {
        gpu.is_intel_arc_hint = true;
    }
}

void apply_vendor_device_hints(GpuCandidate& gpu,
                               const std::string& vendor_lower,
                               const std::string& device_lower,
                               bool treat_virtual_vendor_as_virtual) {
    if (vendor_lower.find("nvidia") != std::string::npos ||
        vendor_lower.find("advanced micro devices") != std::string::npos ||
        vendor_lower.find("amd") != std::string::npos ||
        device_lower.find("nvidia") != std::string::npos ||
        device_lower.find("amd") != std::string::npos ||
        device_lower.find("radeon") != std::string::npos ||
        device_lower.find("geforce") != std::string::npos ||
        device_lower.find("quadro") != std::string::npos) {
        gpu.is_discrete_hint = true;
    }

    if ((treat_virtual_vendor_as_virtual && contains_any(vendor_lower, {"red hat", "vmware", "virtualbox", "bochs", "cirrus"})) ||
        contains_any(device_lower, {"qxl", "virtio", "vmware", "virtualbox", "bochs", "cirrus"})) {
        gpu.is_virtual_hint = true;
    }

    if (device_lower.find("arc") != std::string::npos) {
        gpu.is_intel_arc_hint = true;
    }
}

GpuCandidate pick_best_gpu(const std::vector<GpuCandidate>& gpus) {
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

void apply_gpu_candidate_to_hw(HwFacts& hw, const GpuCandidate& best) {
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
}

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
bool sysctlbyname_u64(const char* name, uint64_t& out) {
    uint64_t v = 0;
    size_t len = sizeof(v);
    if (sysctlbyname(name, &v, &len, nullptr, 0) != 0) return false;
    out = v;
    return true;
}

bool sysctlbyname_i64(const char* name, int64_t& out) {
    int64_t v = 0;
    size_t len = sizeof(v);
    if (sysctlbyname(name, &v, &len, nullptr, 0) != 0) return false;
    out = v;
    return true;
}

bool sysctlbyname_int(const char* name, int& out) {
    int v = 0;
    size_t len = sizeof(v);
    if (sysctlbyname(name, &v, &len, nullptr, 0) != 0) return false;
    out = v;
    return true;
}
#endif

} // namespace bsd_common
