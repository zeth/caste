#include "caste.hpp"

#if defined(__APPLE__) && defined(__MACH__)

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
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

static bool sysctl_bool(const char* name, bool& out) {
    int v = 0;
    if (!sysctl_int(name, v)) return false;
    out = (v != 0);
    return true;
}

static bool cfdata_to_u64(CFDataRef data, uint64_t& out) {
    if (!data) return false;
    CFIndex len = CFDataGetLength(data);
    if (len == 4) {
        uint32_t v = 0;
        std::memcpy(&v, CFDataGetBytePtr(data), 4);
        out = v;
        return true;
    }
    if (len == 8) {
        uint64_t v = 0;
        std::memcpy(&v, CFDataGetBytePtr(data), 8);
        out = v;
        return true;
    }
    return false;
}

static bool cfnumber_to_u64(CFNumberRef num, uint64_t& out) {
    if (!num) return false;
    uint64_t v = 0;
    if (CFNumberGetValue(num, kCFNumberSInt64Type, &v)) {
        out = v;
        return true;
    }
    return false;
}

struct GpuCandidate {
    uint32_t vendor_id = 0;
    uint32_t device_id = 0;
    bool is_discrete_hint = false;
    uint64_t vram_bytes = 0;
};

static bool is_gpu_class(uint32_t class_code) {
    return (class_code & 0xFF0000u) == 0x030000u;
}

static GpuCandidate pick_best_gpu(const std::vector<GpuCandidate>& gpus) {
    auto score = [](const GpuCandidate& g) -> uint64_t {
        uint64_t s = 0;
        if (g.is_discrete_hint) s += 1'000'000'000ull;
        s += std::min<uint64_t>(g.vram_bytes, 999'000'000ull);
        if (g.vendor_id == 0x10de) s += 10'000ull; // NVIDIA
        if (g.vendor_id == 0x1002) s += 5'000ull;  // AMD
        if (g.vendor_id == 0x8086) s += 1'000ull;  // Intel
        return s;
    };
    if (gpus.empty()) return {};
    return *std::max_element(gpus.begin(), gpus.end(),
                             [&](const GpuCandidate& a, const GpuCandidate& b){
                                 return score(a) < score(b);
                             });
}

static std::vector<GpuCandidate> enumerate_gpus_iokit() {
    std::vector<GpuCandidate> out;

    CFMutableDictionaryRef match = IOServiceMatching("IOPCIDevice");
    if (!match) return out;

    io_iterator_t iter = 0;
    if (IOServiceGetMatchingServices(kIOMasterPortDefault, match, &iter) != KERN_SUCCESS) {
        return out;
    }

    io_registry_entry_t entry = 0;
    while ((entry = IOIteratorNext(iter))) {
        CFDataRef class_code_data = (CFDataRef)IORegistryEntryCreateCFProperty(
            entry, CFSTR("class-code"), kCFAllocatorDefault, 0);

        uint64_t class_code_u64 = 0;
        bool is_gpu = cfdata_to_u64(class_code_data, class_code_u64) &&
                      is_gpu_class(static_cast<uint32_t>(class_code_u64));
        if (class_code_data) CFRelease(class_code_data);

        if (!is_gpu) {
            IOObjectRelease(entry);
            continue;
        }

        GpuCandidate g{};

        CFDataRef vendor_data = (CFDataRef)IORegistryEntryCreateCFProperty(
            entry, CFSTR("vendor-id"), kCFAllocatorDefault, 0);
        uint64_t vendor_u64 = 0;
        if (cfdata_to_u64(vendor_data, vendor_u64)) {
            g.vendor_id = static_cast<uint32_t>(vendor_u64);
        }
        if (vendor_data) CFRelease(vendor_data);

        CFDataRef device_data = (CFDataRef)IORegistryEntryCreateCFProperty(
            entry, CFSTR("device-id"), kCFAllocatorDefault, 0);
        uint64_t device_u64 = 0;
        if (cfdata_to_u64(device_data, device_u64)) {
            g.device_id = static_cast<uint32_t>(device_u64);
        }
        if (device_data) CFRelease(device_data);

        CFTypeRef vram_any = IORegistryEntryCreateCFProperty(
            entry, CFSTR("VRAM,totalsize"), kCFAllocatorDefault, 0);
        uint64_t vram_u64 = 0;
        if (vram_any) {
            if (CFGetTypeID(vram_any) == CFDataGetTypeID()) {
                cfdata_to_u64((CFDataRef)vram_any, vram_u64);
            } else if (CFGetTypeID(vram_any) == CFNumberGetTypeID()) {
                cfnumber_to_u64((CFNumberRef)vram_any, vram_u64);
            }
            CFRelease(vram_any);
        }
        g.vram_bytes = vram_u64;

        if (g.vendor_id == 0x10de || g.vendor_id == 0x1002) {
            g.is_discrete_hint = true;
        } else if (g.vendor_id == 0x8086) {
            g.is_discrete_hint = false;
        }

        out.push_back(g);
        IOObjectRelease(entry);
    }

    IOObjectRelease(iter);
    return out;
}

} // namespace

HwFacts fill_hw_facts_platform() {
    HwFacts hw{};

    // RAM
    sysctl_u64("hw.memsize", hw.ram_bytes);

    // CPU cores/threads
    sysctl_int("hw.logicalcpu", hw.logical_threads);
    sysctl_int("hw.physicalcpu", hw.physical_cores);
    if (hw.logical_threads <= 0) sysctl_int("hw.logicalcpu_max", hw.logical_threads);
    if (hw.physical_cores <= 0) sysctl_int("hw.physicalcpu_max", hw.physical_cores);

    // Apple Silicon detection
    bool arm64 = false;
    if (sysctl_bool("hw.optional.arm64", arm64) && arm64) {
        hw.is_apple_silicon = true;
        hw.gpu_kind = GpuKind::Unified;
        hw.has_discrete_gpu = false;
        return hw;
    }

    // Intel macs: best-effort GPU detection via IOKit
    auto gpus = enumerate_gpus_iokit();
    if (gpus.empty()) {
        hw.gpu_kind = GpuKind::None;
        return hw;
    }

    GpuCandidate best = pick_best_gpu(gpus);
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
