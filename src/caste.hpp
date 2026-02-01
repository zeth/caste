#pragma once

#include <cstdint>
#include <string>

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

CasteResult classify_caste(const HwFacts& hw);
const char* caste_name(Caste t);

// Simple public API: call this and get a single word bucket name.
HwFacts detect_hw_facts();
CasteResult detect_caste();
std::string detect_caste_word();
