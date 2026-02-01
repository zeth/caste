// linux_hw_facts.cpp
//
// Fill HwFacts on Linux with NO third-party deps.
// Uses: /proc, /sys, sysinfo(), and optional runtime NVML (dlopen) for NVIDIA VRAM.
//
// Build: g++ -std=c++20 -O2 linux_hw_facts.cpp -ldl
//
// Notes:
// - Works cross-distro (kernel interfaces).
// - VRAM:
//    * NVIDIA: best via NVML if driver present.
//    * AMD amdgpu: often via /sys/.../mem_info_vram_total.
//    * Intel iGPU: shared memory -> don't fake VRAM.
// - Intel Arc detection: heuristic on device-id range (good enough for tiering).

#include "caste.hpp"

#if defined(__linux__)

#include <algorithm>
#include <cstdint>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <sys/sysinfo.h>
#include <thread>
#include <utility>
#include <vector>

namespace {

static inline std::string trim(std::string s) {
    auto notspace = [](unsigned char c){ return c != ' ' && c != '\t' && c != '\n' && c != '\r'; };
    while (!s.empty() && !notspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && !notspace((unsigned char)s.back())) s.pop_back();
    return s;
}

static std::optional<std::string> read_text_file(const std::filesystem::path& p) {
    std::ifstream f(p);
    if (!f) return std::nullopt;
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::optional<uint64_t> read_hex_u64_file(const std::filesystem::path& p) {
    auto txt = read_text_file(p);
    if (!txt) return std::nullopt;
    std::string s = trim(*txt);
    // e.g. "0x10de"
    uint64_t v = 0;
    try {
        v = std::stoull(s, nullptr, 16);
    } catch (...) {
        return std::nullopt;
    }
    return v;
}

static std::optional<uint64_t> read_dec_u64_file(const std::filesystem::path& p) {
    auto txt = read_text_file(p);
    if (!txt) return std::nullopt;
    std::string s = trim(*txt);
    uint64_t v = 0;
    try {
        v = std::stoull(s, nullptr, 10);
    } catch (...) {
        return std::nullopt;
    }
    return v;
}

static uint64_t get_total_ram_bytes_sysinfo() {
    struct sysinfo info{};
    if (sysinfo(&info) != 0) return 0;
    return static_cast<uint64_t>(info.totalram) * info.mem_unit;
}

struct CpuCounts {
    int logical_threads = 0;
    int physical_cores = 0; // 0 if unknown
};

// Robust-ish:
// - logical_threads: count "processor\t:" lines (fallback to std::thread)
// - physical_cores: count unique (physical id, core id) pairs if present
static CpuCounts get_cpu_counts_from_proc() {
    CpuCounts out;

    std::ifstream f("/proc/cpuinfo");
    if (!f) {
        out.logical_threads = (int)std::thread::hardware_concurrency();
        return out;
    }

    std::string line;
    int processors = 0;

    // For physical cores:
    // Many x86 expose "physical id" and "core id". Some don't.
    bool saw_phys = false;
    bool saw_core = false;
    int cur_phys_id = -1;
    int cur_core_id = -1;
    std::set<std::pair<int,int>> core_pairs;

    auto flush_record = [&](){
        if (cur_phys_id >= 0 && cur_core_id >= 0) {
            core_pairs.emplace(cur_phys_id, cur_core_id);
        }
        cur_phys_id = -1;
        cur_core_id = -1;
    };

    while (std::getline(f, line)) {
        if (line.empty()) {
            flush_record();
            continue;
        }
        auto pos = line.find(':');
        if (pos == std::string::npos) continue;
        std::string key = trim(line.substr(0, pos));
        std::string val = trim(line.substr(pos + 1));

        if (key == "processor") {
            processors++;
        } else if (key == "physical id") {
            saw_phys = true;
            try { cur_phys_id = std::stoi(val); } catch (...) {}
        } else if (key == "core id") {
            saw_core = true;
            try { cur_core_id = std::stoi(val); } catch (...) {}
        }
    }
    flush_record();

    out.logical_threads = processors;
    if (out.logical_threads <= 0) {
        out.logical_threads = (int)std::thread::hardware_concurrency();
    }

    if (saw_phys && saw_core && !core_pairs.empty()) {
        out.physical_cores = (int)core_pairs.size();
    } else {
        out.physical_cores = 0; // unknown; let classifier rely on threads
    }

    return out;
}

// ------------ Optional NVML (NVIDIA VRAM) via dlopen ------------
//
// We only need a couple of types and functions.
// This avoids link-time dependency on libnvidia-ml.
//
// NVML API basics we use:
// - nvmlInit_v2
// - nvmlDeviceGetCount_v2
// - nvmlDeviceGetHandleByIndex_v2
// - nvmlDeviceGetMemoryInfo
// - nvmlShutdown
//
// If any missing, we treat NVML as unavailable.

using nvmlReturn_t = int;
static constexpr nvmlReturn_t NVML_SUCCESS = 0;
using nvmlDevice_t = struct nvmlDevice_st*;

struct nvmlMemory_t {
    uint64_t total;
    uint64_t free;
    uint64_t used;
};

struct NvmlApi {
    void* handle = nullptr;

    nvmlReturn_t (*nvmlInit_v2)() = nullptr;
    nvmlReturn_t (*nvmlShutdown)() = nullptr;
    nvmlReturn_t (*nvmlDeviceGetCount_v2)(unsigned int*) = nullptr;
    nvmlReturn_t (*nvmlDeviceGetHandleByIndex_v2)(unsigned int, nvmlDevice_t*) = nullptr;
    nvmlReturn_t (*nvmlDeviceGetMemoryInfo)(nvmlDevice_t, nvmlMemory_t*) = nullptr;

    bool ok() const {
        return handle &&
               nvmlInit_v2 && nvmlShutdown &&
               nvmlDeviceGetCount_v2 && nvmlDeviceGetHandleByIndex_v2 &&
               nvmlDeviceGetMemoryInfo;
    }
};

static NvmlApi try_load_nvml() {
    NvmlApi api{};

    // Common soname on Linux NVIDIA drivers
    api.handle = dlopen("libnvidia-ml.so.1", RTLD_LAZY);
    if (!api.handle) return api;

    auto load = [&](auto& fn, const char* name) {
        fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(dlsym(api.handle, name));
    };

    load(api.nvmlInit_v2, "nvmlInit_v2");
    load(api.nvmlShutdown, "nvmlShutdown");
    load(api.nvmlDeviceGetCount_v2, "nvmlDeviceGetCount_v2");
    load(api.nvmlDeviceGetHandleByIndex_v2, "nvmlDeviceGetHandleByIndex_v2");
    load(api.nvmlDeviceGetMemoryInfo, "nvmlDeviceGetMemoryInfo");

    if (!api.ok()) {
        dlclose(api.handle);
        api.handle = nullptr;
    }
    return api;
}

static void unload_nvml(NvmlApi& api) {
    if (api.handle) dlclose(api.handle);
    api = NvmlApi{};
}

static uint64_t query_nvidia_vram_bytes_nvml_best_effort() {
    NvmlApi api = try_load_nvml();
    if (!api.ok()) return 0;

    uint64_t best = 0;

    if (api.nvmlInit_v2() != NVML_SUCCESS) {
        unload_nvml(api);
        return 0;
    }

    unsigned int count = 0;
    if (api.nvmlDeviceGetCount_v2(&count) == NVML_SUCCESS) {
        for (unsigned int i = 0; i < count; i++) {
            nvmlDevice_t dev = nullptr;
            if (api.nvmlDeviceGetHandleByIndex_v2(i, &dev) != NVML_SUCCESS || !dev) continue;
            nvmlMemory_t mem{};
            if (api.nvmlDeviceGetMemoryInfo(dev, &mem) != NVML_SUCCESS) continue;
            best = std::max(best, mem.total);
        }
    }

    api.nvmlShutdown();
    unload_nvml(api);
    return best;
}

// ------------ GPU enumeration via /sys/class/drm ------------

static bool path_is_card(const std::filesystem::directory_entry& de) {
    // Match "card0", "card1", ... (not "card0-DP-1" connectors)
    if (!de.is_directory()) return false;
    auto name = de.path().filename().string();
    if (name.rfind("card", 0) != 0) return false;
    if (name.size() <= 4) return false;
    for (size_t i = 4; i < name.size(); i++) {
        if (name[i] < '0' || name[i] > '9') return false;
    }
    return true;
}

struct GpuCandidate {
    uint64_t vendor = 0;   // PCI vendor
    uint64_t device = 0;   // PCI device id
    bool is_discrete_hint = false;
    bool is_intel_arc_hint = false;
    uint64_t vram_bytes = 0; // best-effort
};

static bool intel_arc_device_heuristic(uint64_t intel_device_id) {
    // Heuristic: DG2/Alchemist (Arc) devices commonly fall in 0x56xx / 0x57xx ranges.
    // This is not perfect, but good enough for a first-caste bucket.
    uint64_t hi = (intel_device_id & 0xFF00ull) >> 8;
    return (hi == 0x56ull) || (hi == 0x57ull);
}

static std::optional<uint64_t> try_read_amd_vram_total(const std::filesystem::path& drm_card_device_path) {
    // Many amdgpu expose mem_info_vram_total in bytes.
    auto p = drm_card_device_path / "mem_info_vram_total";
    auto v = read_dec_u64_file(p);
    if (v && *v > 0) return v;
    return std::nullopt;
}

static std::vector<GpuCandidate> enumerate_gpus_sysfs() {
    std::vector<GpuCandidate> out;

    const std::filesystem::path drm("/sys/class/drm");
    if (!std::filesystem::exists(drm)) return out;

    for (auto& de : std::filesystem::directory_iterator(drm)) {
        if (!path_is_card(de)) continue;

        auto devpath = de.path() / "device";
        auto vendor = read_hex_u64_file(devpath / "vendor").value_or(0);
        auto device = read_hex_u64_file(devpath / "device").value_or(0);
        if (!vendor) continue;

        GpuCandidate g{};
        g.vendor = vendor;
        g.device = device;

        // Vendor-based hints
        // NVIDIA: 0x10de
        // AMD:    0x1002
        // Intel:  0x8086
        if (vendor == 0x10de) {
            g.is_discrete_hint = true;
            // VRAM via NVML handled globally later; keep 0 here for now.
        } else if (vendor == 0x1002) {
            // Could be discrete or APU; if VRAM sysfs exists, treat as discrete-ish.
            if (auto amd_vram = try_read_amd_vram_total(devpath)) {
                g.vram_bytes = *amd_vram;
                g.is_discrete_hint = true;
            } else {
                g.is_discrete_hint = false;
            }
        } else if (vendor == 0x8086) {
            g.is_discrete_hint = false; // Intel is usually iGPU, but Arc dGPU exists
            g.is_intel_arc_hint = intel_arc_device_heuristic(device);
            // If Arc is discrete, you’ll usually still want VRAM via a better method;
            // without deps, we just use the hint + RAM in your classifier.
        }

        out.push_back(g);
    }

    return out;
}

static bool has_vendor(const std::vector<GpuCandidate>& gpus, uint64_t vendor) {
    return std::any_of(gpus.begin(), gpus.end(), [&](const GpuCandidate& g){ return g.vendor == vendor; });
}

static GpuCandidate pick_best_gpu(std::vector<GpuCandidate> gpus) {
    // Prefer discrete > integrated, then by VRAM if known, else by vendor preference.
    // Vendor preference when unknown VRAM: NVIDIA > AMD > Intel.
    auto score = [](const GpuCandidate& g) -> uint64_t {
        uint64_t s = 0;
        if (g.is_discrete_hint) s += 1'000'000'000ull;
        s += std::min<uint64_t>(g.vram_bytes, 999'000'000ull); // keep bounded
        if (g.vendor == 0x10de) s += 10'000ull;
        if (g.vendor == 0x1002) s += 5'000ull;
        if (g.vendor == 0x8086) s += 1'000ull;
        if (g.is_intel_arc_hint) s += 2'000ull;
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
    hw.ram_bytes = get_total_ram_bytes_sysinfo();

    // CPU
    {
        CpuCounts c = get_cpu_counts_from_proc();
        hw.logical_threads = c.logical_threads;
        hw.physical_cores = c.physical_cores;
        if (hw.logical_threads <= 0) hw.logical_threads = (int)std::thread::hardware_concurrency();
    }

    // GPU(s)
    auto gpus = enumerate_gpus_sysfs();

    // NVIDIA VRAM via NVML (best effort) — if ANY NVIDIA present, we’ll try it.
    uint64_t nvidia_vram_best = 0;
    if (has_vendor(gpus, 0x10de)) {
        nvidia_vram_best = query_nvidia_vram_bytes_nvml_best_effort();
        // Attach this VRAM number to all NVIDIA candidates so picker can choose properly.
        if (nvidia_vram_best > 0) {
            for (auto& g : gpus) {
                if (g.vendor == 0x10de) {
                    g.vram_bytes = std::max(g.vram_bytes, nvidia_vram_best);
                    g.is_discrete_hint = true;
                }
            }
        }
    }

    if (gpus.empty()) {
        hw.gpu_kind = GpuKind::None;
        hw.has_discrete_gpu = false;
        hw.vram_bytes = 0;
        hw.is_intel_arc = false;
        return hw;
    }

    GpuCandidate best = pick_best_gpu(std::move(gpus));

    // Fill HwFacts from best candidate
    hw.is_intel_arc = (best.vendor == 0x8086) && best.is_intel_arc_hint;

    if (best.is_discrete_hint) {
        hw.gpu_kind = GpuKind::Discrete;
        hw.has_discrete_gpu = true;
        hw.vram_bytes = best.vram_bytes; // may still be 0 if unknown (e.g., Intel Arc dGPU without a VRAM source)
    } else {
        hw.gpu_kind = GpuKind::Integrated;
        hw.has_discrete_gpu = false;
        hw.vram_bytes = 0; // shared memory; don’t pretend
    }

    return hw;
}

// If you want a quick manual test, compile with -DHWFACTS_TEST_MAIN
#ifdef HWFACTS_TEST_MAIN
#include <iostream>
static const char* gpu_kind_name(GpuKind k) {
    switch (k) {
        case GpuKind::None: return "None";
        case GpuKind::Integrated: return "Integrated";
        case GpuKind::Unified: return "Unified";
        case GpuKind::Discrete: return "Discrete";
    }
    return "Unknown";
}
int main() {
    HwFacts hw = fill_hw_facts_platform();
    std::cout << "RAM: " << (hw.ram_bytes / (1024ull*1024ull*1024ull)) << " GiB\n";
    std::cout << "CPU: physical_cores=" << hw.physical_cores
              << " logical_threads=" << hw.logical_threads << "\n";
    std::cout << "GPU: kind=" << gpu_kind_name(hw.gpu_kind)
              << " has_discrete=" << (hw.has_discrete_gpu ? "true" : "false")
              << " vram=" << (hw.vram_bytes / (1024ull*1024ull*1024ull)) << " GiB"
              << " intel_arc_hint=" << (hw.is_intel_arc ? "true" : "false")
              << "\n";
    return 0;
}
#endif

#endif
