#include "caste.hpp"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr uint64_t GiB(uint64_t x) {
    return x * 1024ull * 1024ull * 1024ull;
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
}
#endif

#if defined(_WIN32)
TEST_CASE("Windows hw facts are populated") {
    HwFacts hw = detect_hw_facts();
    REQUIRE(hw.ram_bytes > 0);
    REQUIRE(hw.logical_threads > 0);

    CasteResult result = classify_caste(hw);
    REQUIRE(is_valid_caste(result.caste));
}
#endif
