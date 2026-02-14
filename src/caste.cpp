#include "caste.hpp"

static inline uint64_t GiB(uint64_t x) { return x * 1024ull * 1024ull * 1024ull; }
static inline uint64_t MiB(uint64_t x) { return x * 1024ull * 1024ull; }
static inline uint64_t ram_user_floor_bytes() { return GiB(8) - MiB(512); } // tolerate reserved memory

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
    if (ram_bytes < ram_user_floor_bytes()) return Caste::Mini;
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
    CasteResult out;

    // 0) Absolute floor
    if (hw.ram_bytes < ram_user_floor_bytes()) {
        out.caste = Caste::Mini;
        out.reason = "RAM < ~7.5GB";
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
    } else if (hw.gpu_kind == GpuKind::Integrated) {
        // Integrated GPU (Intel/AMD iGPU): default to User if >=8GB RAM.
        base = Caste::User;
        out.reason = "integrated GPU caste";
    } else {
        // No GPU signal (or unknown): keep conservative User baseline.
        base = Caste::User;
        out.reason = "no discrete GPU detected";
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
    if (hw.ram_bytes >= ram_user_floor_bytes()) {
        capped = max_caste(capped, Caste::User);
    }

    out.caste = capped;

    // Improve reason string with caps applied
    if (cap_ram != Caste::Rig) out.reason += "; RAM cap applied";
    if (cap_cpu != Caste::Rig) out.reason += "; CPU cap applied";

    return out;
}

// Optional helper for display
const char* caste_name(Caste t) {
    switch (t) {
        case Caste::Mini: return "Mini";
        case Caste::User: return "User";
        case Caste::Developer: return "Developer";
        case Caste::Workstation: return "Workstation";
        case Caste::Rig: return "Rig";
    }
    return "Unknown";
}

#if defined(__linux__)
HwFacts fill_hw_facts_platform();
#elif defined(__APPLE__) && defined(__MACH__)
HwFacts fill_hw_facts_platform();
#elif defined(_WIN32)
HwFacts fill_hw_facts_platform();
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
HwFacts fill_hw_facts_platform();
#else
static HwFacts fill_hw_facts_platform() {
    return HwFacts{};
}
#endif

CasteResult detect_caste() {
    HwFacts hw = fill_hw_facts_platform();
    return classify_caste(hw);
}

std::string detect_caste_word() {
    return std::string(caste_name(detect_caste().caste));
}

HwFacts detect_hw_facts() {
    return fill_hw_facts_platform();
}
