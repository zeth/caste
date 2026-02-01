#include <cstdint>
#include <string>
#include <algorithm>

enum class Caste {
    Mini,
    User,
    Developer,
    Workstation,
    Rig
};

enum class GpuKind {
    None,
    Integrated,   // Intel UHD/Iris Xe, AMD iGPU, etc. (shared memory)
    Unified,      // Apple Silicon style unified memory (shared, but fast)
    Discrete      // NVIDIA/AMD dGPU with dedicated VRAM
};

struct HwFacts {
    // Memory
    uint64_t ram_bytes = 0;

    // CPU (use whatever you can get; physical if known, else set to 0)
    int physical_cores = 0;
    int logical_threads = 0;

    // GPU summary (your detection layer fills this)
    GpuKind gpu_kind = GpuKind::None;
    uint64_t vram_bytes = 0;          // only meaningful if gpu_kind == Discrete
    bool has_discrete_gpu = false;    // convenience (often same as gpu_kind==Discrete)
    bool is_apple_silicon = false;    // macOS arm64
    bool is_intel_arc = false;        // Arc dGPU OR Arc-class iGPU (your detection decides)
};

struct CasteResult {
    Caste caste = Caste::Mini;
    std::string reason; // for logs/UI
};

static inline uint64_t GiB(uint64_t x) { return x * 1024ull * 1024ull * 1024ull; }

static Caste min_caste(Caste a, Caste b) {
    return (static_cast<int>(a) < static_cast<int>(b)) ? a : b;
}
static Caste max_caste(Caste a, Caste b) {
    return (static_cast<int>(a) > static_cast<int>(b)) ? a : b;
}

static Caste caste_from_vram(uint64_t vram_bytes) {
    if (vram_bytes >= GiB(24)) return Caste::Rig;
    if (vram_bytes >= GiB(16)) return Caste::Workstation;
    if (vram_bytes >= GiB(6))  return Caste::Developer;
    if (vram_bytes >= GiB(2))  return Caste::User;
    return Caste::Mini; // dGPU with <2GB is basically Mini for modern local LLMs
}

// Clamp “how high you can go” by RAM, to avoid embarrassment.
static Caste ram_cap(uint64_t ram_bytes) {
    if (ram_bytes < GiB(8))  return Caste::Mini;
    if (ram_bytes < GiB(16)) return Caste::User;
    if (ram_bytes < GiB(24)) return Caste::User;        // 16–23GB: still usually “User”
    if (ram_bytes < GiB(32)) return Caste::Developer;   // 24–31GB
    if (ram_bytes < GiB(64)) return Caste::Workstation; // 32–63GB
    return Caste::Rig;                                  // 64GB+
}

// Optional clamp by CPU. Keep this gentle; RAM/GPU dominate.
static Caste cpu_cap(int physical_cores, int logical_threads) {
    // If you only have logical threads, pass physical_cores=0 and we’ll use threads.
    const int cores = (physical_cores > 0) ? physical_cores : 0;
    const int threads = logical_threads;

    // Very low end
    if ((cores > 0 && cores < 4) || (cores == 0 && threads > 0 && threads < 8)) {
        return Caste::Mini;
    }

    // “User floor” (roughly 4c/8t)
    if ((cores > 0 && cores < 6) || (cores == 0 && threads > 0 && threads < 12)) {
        return Caste::User;
    }

    // 6c/12t can be Developer or above, don’t cap further.
    return Caste::Rig;
}

CasteResult classify_caste(const HwFacts& hw) {
    TierResult out;

    // 0) Absolute floor
    if (hw.ram_bytes < GiB(8)) {
        out.caste = Caste::Mini;
        out.reason = "RAM < 8GB";
        return out;
    }

    // 1) Base caste by GPU/memory model
    Caste base = Caste::User;

    if (hw.gpu_kind == GpuKind::Discrete || hw.has_discrete_gpu) {
        base = caste_from_vram(hw.vram_bytes);
        out.reason = "discrete GPU VRAM caste";
    } else if (hw.is_apple_silicon || hw.gpu_kind == GpuKind::Unified) {
        // Apple Silicon: treat RAM as the main budget signal.
        if (hw.ram_bytes >= GiB(64))      base = Caste::Rig;
        else if (hw.ram_bytes >= GiB(32)) base = Caste::Workstation;
        else if (hw.ram_bytes >= GiB(24)) base = Caste::Developer;
        else                              base = Caste::User;
        out.reason = "unified memory (Apple Silicon) caste by RAM";
    } else {
        // Integrated GPU (Intel/AMD iGPU): default to User if >=8GB RAM.
        base = Caste::User;
        out.reason = "integrated GPU caste";
    }

    // 2) Intel Arc special-case
    // - If Arc is DISCRETE, VRAM already handled above.
    // - If Arc is integrated/unknown, allow a cautious bump only with enough RAM.
    if (!hw.has_discrete_gpu && hw.gpu_kind != GpuKind::Discrete && hw.is_intel_arc) {
        if (hw.ram_bytes >= GiB(16)) {
            base = max_caste(base, Caste::Developer);
            out.reason += "; Arc-class iGPU with >=16GB RAM => Developer floor";
        } else {
            out.reason += "; Arc-class iGPU but <16GB RAM => no bump";
        }
    }

    // 3) Clamp by RAM (prevents “VRAM says Rig” when system RAM is too small)
    Caste cap_ram = ram_cap(hw.ram_bytes);
    Caste capped = min_caste(base, cap_ram);

    // 4) Gentle CPU sanity clamp (optional but cheap)
    Caste cap_cpu = cpu_cap(hw.physical_cores, hw.logical_threads);
    capped = min_caste(capped, cap_cpu);

    // 5) Ensure we don’t return Mini if RAM >= 8GB unless everything is truly weak
    // (You can remove this if you want harsher behavior)
    if (hw.ram_bytes >= GiB(8)) {
        capped = max_caste(capped, Caste::User);
    }

    out.caste = capped;

    // Improve reason string with caps applied
    if (cap_ram != Caste::Rig) out.reason += "; RAM cap applied";
    if (cap_cpu != Caste::Rig) out.reason += "; CPU cap applied";

    return out;
}

// Optional helper for display
static const char* caste_name(Caste t) {
    switch (t) {
        case Caste::Mini: return "Mini";
        case Caste::User: return "User";
        case Caste::Developer: return "Developer";
        case Caste::Workstation: return "Workstation";
        case Caste::Rig: return "Rig";
    }
    return "Unknown";
}
