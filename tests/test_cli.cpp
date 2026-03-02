#include <cstdlib>
#include <cstdio>
#include <string>

#include <catch2/catch_test_macros.hpp>

#ifdef CASTE_CLI_PATH
namespace {

#if defined(_WIN32)
#define CASTE_POPEN _popen
#define CASTE_PCLOSE _pclose
#else
#define CASTE_POPEN popen
#define CASTE_PCLOSE pclose
#endif

int run_cli_and_capture(const std::string& args, std::string& output) {
    std::string cmd = "\"";
    cmd += CASTE_CLI_PATH;
    cmd += "\" ";
    cmd += args;
    cmd += " 2>&1";

    FILE* pipe = CASTE_POPEN(cmd.c_str(), "r");
    if (!pipe) return -1;

    output.clear();
    char buf[512];
    while (std::fgets(buf, sizeof(buf), pipe) != nullptr) {
        output += buf;
    }
    return CASTE_PCLOSE(pipe);
}

bool output_has_caste_word(const std::string& out) {
    return out.find("Mini") != std::string::npos ||
           out.find("User") != std::string::npos ||
           out.find("Developer") != std::string::npos ||
           out.find("Workstation") != std::string::npos ||
           out.find("Rig") != std::string::npos;
}

} // namespace

TEST_CASE("CLI help and version output contract") {
    std::string out;
    REQUIRE(run_cli_and_capture("--help", out) == 0);
    REQUIRE(out.find("Usage: caste") != std::string::npos);

    REQUIRE(run_cli_and_capture("--version", out) == 0);
    REQUIRE(out.find("caste ") != std::string::npos);
}

TEST_CASE("CLI hwfacts and reason output contract") {
    std::string out;
    REQUIRE(run_cli_and_capture("--hwfacts", out) == 0);
    REQUIRE(out.find("ram_bytes=") != std::string::npos);
    REQUIRE(out.find("logical_threads=") != std::string::npos);
    REQUIRE(out.find("gpu_kind=") != std::string::npos);

    REQUIRE(run_cli_and_capture("--reason", out) == 0);
    REQUIRE(output_has_caste_word(out));
}
#endif
