#include "agent_video_utils.h"
#include "local_control_http_utils.h"
#include "video_loader.h"

#include <json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
struct TestContext {
    int passed = 0;
    int failed = 0;

    void expect(bool condition, const std::string& message) {
        if (condition) {
            ++passed;
        } else {
            ++failed;
            std::cerr << "[FAIL] " << message << std::endl;
        }
    }
};

template <typename T>
std::string to_string_any(const T& value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

template <>
std::string to_string_any<std::string>(const std::string& value) {
    return "\"" + value + "\"";
}

template <typename T, typename U>
void expect_eq(
    TestContext& ctx,
    const T& actual,
    const U& expected,
    const std::string& message
) {
    if (actual == expected) {
        ctx.expect(true, message);
        return;
    }
    ctx.expect(
        false,
        message + " (actual=" + to_string_any(actual) +
            ", expected=" + to_string_any(expected) + ")"
    );
}

struct CurrentPathGuard {
    explicit CurrentPathGuard(const fs::path& target) : previous_(fs::current_path()) {
        fs::current_path(target);
    }

    ~CurrentPathGuard() {
        std::error_code ec;
        fs::current_path(previous_, ec);
    }

private:
    fs::path previous_;
};

fs::path make_temp_dir() {
    const fs::path base = fs::temp_directory_path();
    const auto seed = static_cast<long long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()
    );
    for (int attempt = 0; attempt < 100; ++attempt) {
        const fs::path candidate = base / (
            "webagent-tests-" + std::to_string(seed) + "-" + std::to_string(attempt)
        );
        std::error_code ec;
        if (fs::create_directory(candidate, ec)) {
            return candidate;
        }
    }
    throw std::runtime_error("Failed to create temporary test directory");
}

void test_agent_video_utils(TestContext& ctx) {
    using namespace agent_video_utils;

    expect_eq(
        ctx,
        extract_video_name_token(" https://host/video/screamer7.mp4?raw=1#t=2 "),
        std::string("screamer7.mp4"),
        "extract_video_name_token should normalize URL"
    );
    expect_eq(
        ctx,
        extract_video_name_token("movie.avi"),
        std::string(""),
        "extract_video_name_token should reject non-mp4"
    );
    expect_eq(
        ctx,
        extract_video_name_token("screamer..7.mp4"),
        std::string(""),
        "extract_video_name_token should reject suspicious token"
    );

    expect_eq(
        ctx,
        parse_video_number_from_string("video42.mp4"),
        42,
        "parse_video_number_from_string should parse inline number"
    );
    expect_eq(
        ctx,
        parse_video_number_from_string(" -5 "),
        -5,
        "parse_video_number_from_string should parse signed number"
    );
    expect_eq(
        ctx,
        parse_video_number_from_string("no digits"),
        1,
        "parse_video_number_from_string should fallback to 1"
    );

    expect_eq(
        ctx,
        resolve_video_name(json::parse(R"({"data":{"video":"https://cdn/screamer9.mp4?x=1"}})")),
        std::string("screamer9.mp4"),
        "resolve_video_name should prioritize explicit video name"
    );
    expect_eq(
        ctx,
        resolve_video_name(json::parse(R"({"meta":{"number":"video 3"}})")),
        std::string("screamer3.mp4"),
        "resolve_video_name should build video name from parsed number"
    );
    expect_eq(
        ctx,
        resolve_video_name(json::parse(R"({"meta":{"value":"no match"}})")),
        std::string("screamer1.mp4"),
        "resolve_video_name should fallback to screamer1.mp4"
    );

    ctx.expect(is_empty_task_option(json()), "is_empty_task_option should treat null as empty");
    ctx.expect(is_empty_task_option(json::parse(R"("")")), "is_empty_task_option should treat empty string as empty");
    ctx.expect(is_empty_task_option(json::parse(R"("none")")), "is_empty_task_option should treat 'none' as empty");
    ctx.expect(!is_empty_task_option(json::parse(R"({"video":"screamer2.mp4"})")), "is_empty_task_option should keep non-empty object");

    std::string normalized;
    ctx.expect(
        normalize_video_name_for_control(" 12 ", normalized),
        "normalize_video_name_for_control should accept numeric input"
    );
    expect_eq(
        ctx,
        normalized,
        std::string("screamer12.mp4"),
        "normalize_video_name_for_control should convert numeric input"
    );

    ctx.expect(
        normalize_video_name_for_control("https://host/path/screamer5.mp4", normalized),
        "normalize_video_name_for_control should accept URL input"
    );
    expect_eq(
        ctx,
        normalized,
        std::string("screamer5.mp4"),
        "normalize_video_name_for_control should extract file from URL"
    );

    ctx.expect(
        !normalize_video_name_for_control("invalid value", normalized),
        "normalize_video_name_for_control should reject invalid value"
    );
}

void test_clear_resources(TestContext& ctx) {
    const fs::path temp_dir = make_temp_dir();

    {
        CurrentPathGuard guard(temp_dir);
        std::ofstream("screamer.mp4").put('a');
        std::ofstream("screamer1.mp4").put('a');
        std::ofstream("screamer22.mp4").put('a');
        std::ofstream("screamerA.mp4").put('a');
        std::ofstream("notes.txt").put('a');

        const bool result = clear_resources();
        ctx.expect(result, "clear_resources should return true for a normal directory");
        ctx.expect(!fs::exists("screamer.mp4"), "clear_resources should remove screamer.mp4");
        ctx.expect(!fs::exists("screamer1.mp4"), "clear_resources should remove screamerN.mp4");
        ctx.expect(!fs::exists("screamer22.mp4"), "clear_resources should remove numbered screamer");
        ctx.expect(fs::exists("screamerA.mp4"), "clear_resources should keep non-numeric screamer file");
        ctx.expect(fs::exists("notes.txt"), "clear_resources should keep unrelated files");
    }

    std::error_code ec;
    fs::remove_all(temp_dir, ec);
}

void test_local_control_http_utils(TestContext& ctx) {
    using namespace local_control_http_utils;

    std::string method;
    std::string path;
    ctx.expect(
        parse_request_line("POST /api/selected-video HTTP/1.1", method, path),
        "parse_request_line should parse valid request line"
    );
    expect_eq(ctx, method, std::string("POST"), "parse_request_line should set method");
    expect_eq(ctx, path, std::string("/api/selected-video"), "parse_request_line should set path");
    ctx.expect(
        !parse_request_line("invalid_line", method, path),
        "parse_request_line should reject malformed line"
    );

    expect_eq(
        ctx,
        parse_content_length(
            "Host: localhost\r\nContent-Type: application/json\r\nContent-Length: 17\r\n\r\n"
        ),
        17,
        "parse_content_length should parse numeric header"
    );
    expect_eq(
        ctx,
        parse_content_length("Content-Length: abc\r\n"),
        0,
        "parse_content_length should return 0 for invalid number"
    );

    expect_eq(
        ctx,
        strip_query_string("/api/selected-video?refresh=1"),
        std::string("/api/selected-video"),
        "strip_query_string should drop query part"
    );
    expect_eq(
        ctx,
        strip_query_string("/api/selected-video"),
        std::string("/api/selected-video"),
        "strip_query_string should keep path without query"
    );

    expect_eq(
        ctx,
        json_extract_video_field(R"({"video":"  screamer11.mp4  "})"),
        std::string("screamer11.mp4"),
        "json_extract_video_field should trim extracted value"
    );
    expect_eq(
        ctx,
        json_extract_video_field(R"({"video":"scr\"eam\"er1.mp4"})"),
        std::string("scr\"eam\"er1.mp4"),
        "json_extract_video_field should unescape quotes"
    );
    expect_eq(
        ctx,
        json_extract_video_field(R"({"value":"screamer1.mp4"})"),
        std::string(""),
        "json_extract_video_field should return empty for missing key"
    );

    expect_eq(
        ctx,
        escape_json("line1\n\"line2\""),
        std::string("line1\\n\\\"line2\\\""),
        "escape_json should escape quotes and newline"
    );

    const std::string response = make_http_response(
        200,
        "OK",
        "{\"ok\":true}",
        "application/json"
    );
    ctx.expect(
        response.find("HTTP/1.1 200 OK\r\n") == 0,
        "make_http_response should build status line"
    );
    ctx.expect(
        response.find("Content-Length: 11\r\n") != std::string::npos,
        "make_http_response should contain content length"
    );
    ctx.expect(
        response.rfind("{\"ok\":true}") != std::string::npos,
        "make_http_response should append body"
    );
}
}  // namespace

int main() {
    TestContext ctx;

    try {
        test_agent_video_utils(ctx);
        test_clear_resources(ctx);
        test_local_control_http_utils(ctx);
    } catch (const std::exception& ex) {
        ctx.expect(false, std::string("Unexpected exception: ") + ex.what());
    }

    std::cout << "Passed: " << ctx.passed << ", Failed: " << ctx.failed << std::endl;
    return ctx.failed == 0 ? 0 : 1;
}
