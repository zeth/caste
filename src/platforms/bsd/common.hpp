#pragma once

#include "caste.hpp"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace bsd_common {

struct GpuCandidate {
    bool is_discrete_hint = false;
    bool is_virtual_hint = false;
    bool is_intel_arc_hint = false;
};

std::string trim(std::string s);
std::string to_lower(std::string s);
bool contains_any(const std::string& haystack, std::initializer_list<const char*> needles);
void apply_name_hints(GpuCandidate& gpu, const std::string& name_lower);
void apply_vendor_device_hints(GpuCandidate& gpu,
                               const std::string& vendor_lower,
                               const std::string& device_lower,
                               bool treat_virtual_vendor_as_virtual);
GpuCandidate pick_best_gpu(const std::vector<GpuCandidate>& gpus);
void apply_gpu_candidate_to_hw(HwFacts& hw, const GpuCandidate& best);

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
bool sysctlbyname_u64(const char* name, uint64_t& out);
bool sysctlbyname_i64(const char* name, int64_t& out);
bool sysctlbyname_int(const char* name, int& out);
#endif

} // namespace bsd_common
