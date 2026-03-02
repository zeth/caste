#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#ifdef CASTE_CLI_PATH
namespace {

std::string read_file_to_string(const std::filesystem::path& p) {
    std::ifstream f(p);
    std::string s((std::istreambuf_iterator<char>(f)),
                  std::istreambuf_iterator<char>());
    return s;
}

int run_cli_and_capture(const std::string& args, std::string& output) {
    std::filesystem::path out_path =
        std::filesystem::temp_directory_path() /
        std::filesystem::path("caste_cli_test_output.txt");
    std::string cmd = "\"";
    cmd += CASTE_CLI_PATH;
    cmd += "\" ";
    cmd += args;
    cmd += " > \"";
    cmd += out_path.string();
    cmd += "\" 2>&1";

    int rc = std::system(cmd.c_str());
    output = read_file_to_string(out_path);
    std::error_code ec;
    std::filesystem::remove(out_path, ec);
    return rc;
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
