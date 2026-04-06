#include "caste.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr uint64_t GiB(uint64_t x) {
    return x * 1024ull * 1024ull * 1024ull;
}

constexpr uint64_t MiB(uint64_t x) {
    return x * 1024ull * 1024ull;
}

HwFacts base_hw() {
    HwFacts hw{};
    hw.ram_bytes = GiB(64);
    hw.physical_cores = 8;
    hw.logical_threads = 16;
    hw.gpu_kind = GpuKind::Discrete;
    hw.has_discrete_gpu = true;
    return hw;
}

} // namespace

TEST_CASE("Discrete GPU VRAM tiers map to expected castes") {
    HwFacts hw = base_hw();

    hw.vram_bytes = GiB(2);
    REQUIRE(classify_caste(hw).caste == Caste::User);

    hw.vram_bytes = GiB(6);
    REQUIRE(classify_caste(hw).caste == Caste::Developer);

    hw.vram_bytes = GiB(16);
    REQUIRE(classify_caste(hw).caste == Caste::Workstation);

    hw.vram_bytes = GiB(24);
    REQUIRE(classify_caste(hw).caste == Caste::Rig);
}

TEST_CASE("RAM caps prevent overrating discrete GPUs") {
    HwFacts hw = base_hw();
    hw.ram_bytes = GiB(16);
    hw.vram_bytes = GiB(24);

    REQUIRE(classify_caste(hw).caste == Caste::User);
}

TEST_CASE("Apple Silicon unified memory uses RAM tiers") {
    HwFacts hw{};
    hw.ram_bytes = GiB(32);
    hw.physical_cores = 8;
    hw.logical_threads = 16;
    hw.gpu_kind = GpuKind::Unified;
    hw.is_apple_silicon = true;

    REQUIRE(classify_caste(hw).caste == Caste::Workstation);
}

TEST_CASE("CPU caps are gentle and do not drop below User with enough RAM") {
    HwFacts hw = base_hw();
    hw.vram_bytes = GiB(24);
    hw.physical_cores = 2;
    hw.logical_threads = 4;

    REQUIRE(classify_caste(hw).caste == Caste::User);
}

TEST_CASE("Caste names are stable") {
    REQUIRE(std::string(caste_name(Caste::Mini)) == "Mini");
    REQUIRE(std::string(caste_name(Caste::User)) == "User");
    REQUIRE(std::string(caste_name(Caste::Developer)) == "Developer");
    REQUIRE(std::string(caste_name(Caste::Workstation)) == "Workstation");
    REQUIRE(std::string(caste_name(Caste::Rig)) == "Rig");
}

TEST_CASE("Castes support direct ordered comparison") {
    REQUIRE(Caste::Mini < Caste::User);
    REQUIRE(Caste::User < Caste::Developer);
    REQUIRE(Caste::Developer < Caste::Workstation);
    REQUIRE(Caste::Workstation < Caste::Rig);

    REQUIRE(Caste::Rig > Caste::Workstation);
    REQUIRE(Caste::Workstation >= Caste::Developer);
    REQUIRE(Caste::Developer >= Caste::Developer);
    REQUIRE(Caste::User <= Caste::Developer);
    REQUIRE(Caste::Mini <= Caste::Mini);
}

TEST_CASE("Named caste comparison helpers match operator semantics") {
    REQUIRE(caste_at_least(Caste::Developer, Caste::Developer));
    REQUIRE(caste_at_least(Caste::Workstation, Caste::Developer));
    REQUIRE(caste_at_least(Caste::Rig, Caste::Developer));
    REQUIRE_FALSE(caste_at_least(Caste::User, Caste::Developer));

    REQUIRE(caste_at_most(Caste::Mini, Caste::User));
    REQUIRE(caste_at_most(Caste::User, Caste::User));
    REQUIRE_FALSE(caste_at_most(Caste::Developer, Caste::User));
}

TEST_CASE("Caste ranges support reusable policy buckets") {
    constexpr CasteRange user_or_below{Caste::Mini, Caste::User};
    constexpr CasteRange dev_or_above{Caste::Developer, Caste::Rig};
    constexpr CasteRange dev_to_workstation{
        Caste::Developer,
        Caste::Workstation
    };

    REQUIRE(user_or_below.contains(Caste::Mini));
    REQUIRE(user_or_below.contains(Caste::User));
    REQUIRE_FALSE(user_or_below.contains(Caste::Developer));

    REQUIRE(dev_or_above.contains(Caste::Developer));
    REQUIRE(dev_or_above.contains(Caste::Workstation));
    REQUIRE(dev_or_above.contains(Caste::Rig));
    REQUIRE_FALSE(dev_or_above.contains(Caste::User));

    REQUIRE(dev_to_workstation.contains(Caste::Developer));
    REQUIRE(dev_to_workstation.contains(Caste::Workstation));
    REQUIRE_FALSE(dev_to_workstation.contains(Caste::Rig));
}

namespace {
bool is_valid_caste(Caste c) {
    switch (c) {
        case Caste::Mini:
        case Caste::User:
        case Caste::Developer:
        case Caste::Workstation:
        case Caste::Rig:
            return true;
    }
    return false;
}
} // namespace

TEST_CASE("Classify boundaries around RAM floor and caps") {
    struct Case {
        const char* name;
        uint64_t ram;
        int physical_cores;
        int logical_threads;
        uint64_t vram;
        Caste expected;
    };

    const std::vector<Case> cases = {
        {"below floor => Mini", GiB(8) - MiB(513), 8, 16, GiB(24), Caste::Mini},
        {"at floor => User floor", GiB(8) - MiB(512), 8, 16, GiB(24), Caste::User},
        {"high VRAM but 16GB RAM cap => User", GiB(16), 8, 16, GiB(24), Caste::User},
        {"high VRAM but 24GB RAM cap => Developer", GiB(24), 8, 16, GiB(24), Caste::Developer},
        {"high VRAM but 32GB RAM cap => Workstation", GiB(32), 8, 16, GiB(24), Caste::Workstation},
        {"high VRAM and 64GB RAM => Rig", GiB(64), 8, 16, GiB(24), Caste::Rig},
    };

    for (const auto& tc : cases) {
        HwFacts hw = base_hw();
        hw.ram_bytes = tc.ram;
        hw.physical_cores = tc.physical_cores;
        hw.logical_threads = tc.logical_threads;
        hw.vram_bytes = tc.vram;
        INFO(tc.name);
        REQUIRE(classify_caste(hw).caste == tc.expected);
    }
}

TEST_CASE("Intel Arc integrated bump behavior is gated by RAM") {
    HwFacts hw{};
    hw.gpu_kind = GpuKind::Integrated;
    hw.has_discrete_gpu = false;
    hw.is_intel_arc = true;
    hw.physical_cores = 8;
    hw.logical_threads = 16;

    hw.ram_bytes = GiB(15);
    REQUIRE(classify_caste(hw).caste == Caste::User);

    hw.ram_bytes = GiB(16);
    REQUIRE(classify_caste(hw).caste == Caste::User);

    hw.ram_bytes = GiB(24);
    REQUIRE(classify_caste(hw).caste == Caste::Developer);
}

TEST_CASE("detect_caste and detect_caste_word are consistent") {
    CasteResult result = detect_caste();
    REQUIRE(detect_caste_word() == std::string(caste_name(result.caste)));
    REQUIRE(is_valid_caste(result.caste));
}

#if defined(__FreeBSD__)

TEST_CASE("FreeBSD hw facts are populated") {
    HwFacts hw = detect_hw_facts();
    REQUIRE(hw.ram_bytes > 0);
    REQUIRE(hw.logical_threads > 0);

    CasteResult result = classify_caste(hw);
    REQUIRE(is_valid_caste(result.caste));
}
#endif

#if defined(__APPLE__) && defined(__MACH__)
TEST_CASE("macOS hw facts are populated") {
    HwFacts hw = detect_hw_facts();
    REQUIRE(hw.ram_bytes > 0);
    REQUIRE(hw.logical_threads > 0);

    CasteResult result = classify_caste(hw);
    REQUIRE(is_valid_caste(result.caste));
    REQUIRE(static_cast<int>(hw.gpu_kind) >= static_cast<int>(GpuKind::None));
    REQUIRE(static_cast<int>(hw.gpu_kind) <= static_cast<int>(GpuKind::Discrete));
    if (hw.is_apple_silicon) {
        REQUIRE(hw.gpu_kind == GpuKind::Unified);
    }
}
#endif

#if defined(_WIN32)
TEST_CASE("Windows hw facts are populated") {
    HwFacts hw = detect_hw_facts();
    REQUIRE(hw.ram_bytes > 0);
    REQUIRE(hw.logical_threads > 0);

    CasteResult result = classify_caste(hw);
    REQUIRE(is_valid_caste(result.caste));
    REQUIRE(hw.physical_cores >= 0);
    REQUIRE(static_cast<int>(hw.gpu_kind) >= static_cast<int>(GpuKind::None));
    REQUIRE(static_cast<int>(hw.gpu_kind) <= static_cast<int>(GpuKind::Discrete));
    if (hw.gpu_kind != GpuKind::Discrete) {
        REQUIRE(hw.vram_bytes == 0);
    }
}
#endif
