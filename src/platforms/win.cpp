#include "caste.hpp"

#if defined(_WIN32)

#include <algorithm>
#include <cstdint>
#include <vector>

#include <windows.h>
#include <dxgi.h>

namespace {

struct CpuCounts {
    int logical_threads = 0;
    int physical_cores = 0;
};

static CpuCounts get_cpu_counts() {
    CpuCounts out{};

    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    out.logical_threads = static_cast<int>(si.dwNumberOfProcessors);

    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len);
    if (len == 0) return out;

    std::vector<unsigned char> buffer(len);
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore,
                                          reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()),
                                          &len)) {
        return out;
    }

    size_t offset = 0;
    int cores = 0;
    while (offset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) <= len) {
        auto info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data() + offset);
        if (info->Relationship == RelationProcessorCore) cores++;
        offset += info->Size;
    }
    out.physical_cores = cores;

    return out;
}

struct GpuCandidate {
    uint32_t vendor_id = 0;
    uint32_t device_id = 0;
    uint64_t vram_bytes = 0;
    bool is_discrete_hint = false;
    bool is_intel_arc_hint = false;
};

static bool intel_arc_device_heuristic(uint32_t device_id) {
    uint32_t hi = (device_id & 0xFF00u) >> 8;
    return (hi == 0x56u) || (hi == 0x57u);
}

static GpuCandidate pick_best_gpu(const std::vector<GpuCandidate>& gpus) {
    auto score = [](const GpuCandidate& g) -> uint64_t {
        uint64_t s = 0;
        if (g.is_discrete_hint) s += 1'000'000'000ull;
        s += std::min<uint64_t>(g.vram_bytes, 999'000'000ull);
        if (g.vendor_id == 0x10de) s += 10'000ull; // NVIDIA
        if (g.vendor_id == 0x1002) s += 5'000ull;  // AMD
        if (g.vendor_id == 0x8086) s += 1'000ull;  // Intel
        if (g.is_intel_arc_hint) s += 2'000ull;
        return s;
    };
    if (gpus.empty()) return {};
    return *std::max_element(gpus.begin(), gpus.end(),
                             [&](const GpuCandidate& a, const GpuCandidate& b){
                                 return score(a) < score(b);
                             });
}

static std::vector<GpuCandidate> enumerate_gpus_dxgi() {
    std::vector<GpuCandidate> out;

    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) {
        return out;
    }

    for (UINT i = 0; ; ++i) {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) break;

        DXGI_ADAPTER_DESC1 desc{};
        if (SUCCEEDED(adapter->GetDesc1(&desc))) {
            if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
                GpuCandidate g{};
                g.vendor_id = desc.VendorId;
                g.device_id = desc.DeviceId;
                g.vram_bytes = static_cast<uint64_t>(desc.DedicatedVideoMemory);
                g.is_intel_arc_hint = (g.vendor_id == 0x8086) && intel_arc_device_heuristic(g.device_id);

                if (g.vendor_id == 0x10de || g.vendor_id == 0x1002) {
                    g.is_discrete_hint = true;
                } else if (g.vendor_id == 0x8086) {
                    g.is_discrete_hint = false;
                } else {
                    g.is_discrete_hint = (g.vram_bytes > 0);
                }

                out.push_back(g);
            }
        }

        adapter->Release();
    }

    factory->Release();
    return out;
}

} // namespace

HwFacts fill_hw_facts_platform() {
    HwFacts hw{};

    // RAM
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        hw.ram_bytes = static_cast<uint64_t>(ms.ullTotalPhys);
    }

    // CPU
    CpuCounts c = get_cpu_counts();
    hw.logical_threads = c.logical_threads;
    hw.physical_cores = c.physical_cores;

    // GPU
    auto gpus = enumerate_gpus_dxgi();
    if (gpus.empty()) {
        hw.gpu_kind = GpuKind::None;
        return hw;
    }

    GpuCandidate best = pick_best_gpu(gpus);
    hw.is_intel_arc = (best.vendor_id == 0x8086) && best.is_intel_arc_hint;

    if (best.is_discrete_hint) {
        hw.gpu_kind = GpuKind::Discrete;
        hw.has_discrete_gpu = true;
        hw.vram_bytes = best.vram_bytes;
    } else {
        hw.gpu_kind = GpuKind::Integrated;
        hw.has_discrete_gpu = false;
        hw.vram_bytes = 0;
    }

    return hw;
}

#endif
