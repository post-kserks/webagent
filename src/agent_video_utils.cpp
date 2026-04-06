#include "agent_video_utils.h"

#include <cctype>
#include <string>

namespace {
std::string trim_copy(const std::string& raw) {
    size_t start = 0;
    while (start < raw.size() && std::isspace(static_cast<unsigned char>(raw[start]))) {
        ++start;
    }
    size_t end = raw.size();
    while (end > start && std::isspace(static_cast<unsigned char>(raw[end - 1]))) {
        --end;
    }
    return raw.substr(start, end - start);
}

std::string to_lower_copy(const std::string& value) {
    std::string lowered = value;
    for (char& ch : lowered) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return lowered;
}

bool ends_with_case_insensitive(const std::string& value, const std::string& suffix) {
    if (value.size() < suffix.size()) {
        return false;
    }
    const size_t offset = value.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i) {
        const unsigned char lhs = static_cast<unsigned char>(value[offset + i]);
        const unsigned char rhs = static_cast<unsigned char>(suffix[i]);
        if (std::tolower(lhs) != std::tolower(rhs)) {
            return false;
        }
    }
    return true;
}

bool has_any_digit(const std::string& value) {
    for (unsigned char ch : value) {
        if (std::isdigit(ch)) {
            return true;
        }
    }
    return false;
}

std::string video_name_from_number(int number) {
    if (number < 1) {
        number = 1;
    }
    return "screamer" + std::to_string(number) + ".mp4";
}

bool try_extract_video_name_recursive(const nlohmann::json& node, std::string& out_name) {
    if (node.is_string()) {
        const std::string file_name = agent_video_utils::extract_video_name_token(node.get<std::string>());
        if (!file_name.empty()) {
            out_name = file_name;
            return true;
        }
        return false;
    }

    if (node.is_object()) {
        for (const auto& item : node.items()) {
            if (try_extract_video_name_recursive(item.value(), out_name)) {
                return true;
            }
        }
        return false;
    }

    if (node.is_array()) {
        for (const auto& item : node) {
            if (try_extract_video_name_recursive(item, out_name)) {
                return true;
            }
        }
        return false;
    }

    return false;
}

bool try_extract_video_number_recursive(const nlohmann::json& node, int& out_number) {
    if (node.is_number_integer()) {
        out_number = node.get<int>();
        return true;
    }

    if (node.is_string()) {
        const std::string raw = trim_copy(node.get<std::string>());
        if (raw.empty() || !has_any_digit(raw)) {
            return false;
        }
        out_number = agent_video_utils::parse_video_number_from_string(raw);
        return true;
    }

    if (node.is_object()) {
        for (const auto& item : node.items()) {
            if (try_extract_video_number_recursive(item.value(), out_number)) {
                return true;
            }
        }
        return false;
    }

    if (node.is_array()) {
        for (const auto& item : node) {
            if (try_extract_video_number_recursive(item, out_number)) {
                return true;
            }
        }
        return false;
    }

    return false;
}
}  // namespace

namespace agent_video_utils {
std::string extract_video_name_token(const std::string& raw) {
    std::string value = trim_copy(raw);
    if (value.empty()) {
        return "";
    }

    const size_t query_pos = value.find('?');
    if (query_pos != std::string::npos) {
        value = value.substr(0, query_pos);
    }
    const size_t fragment_pos = value.find('#');
    if (fragment_pos != std::string::npos) {
        value = value.substr(0, fragment_pos);
    }

    const size_t slash_pos = value.find_last_of("/\\");
    if (slash_pos != std::string::npos) {
        value = value.substr(slash_pos + 1);
    }
    value = trim_copy(value);
    if (value.empty()) {
        return "";
    }

    if (value.find("..") != std::string::npos) {
        return "";
    }
    if (value.find('/') != std::string::npos || value.find('\\') != std::string::npos) {
        return "";
    }
    if (!ends_with_case_insensitive(value, ".mp4")) {
        return "";
    }

    return value;
}

int parse_video_number_from_string(const std::string& raw) {
    std::string digits;
    bool has_sign = false;
    bool negative = false;

    for (size_t i = 0; i < raw.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(raw[i]);
        if (!has_sign && digits.empty() && (raw[i] == '-' || raw[i] == '+')) {
            has_sign = true;
            negative = (raw[i] == '-');
            continue;
        }
        if (std::isdigit(ch)) {
            digits.push_back(static_cast<char>(ch));
            continue;
        }
        if (!digits.empty()) {
            break;
        }
    }

    if (digits.empty()) {
        return 1;
    }

    try {
        return std::stoi((negative ? "-" : "") + digits);
    } catch (...) {
        return 1;
    }
}

std::string resolve_video_name(const nlohmann::json& options) {
    std::string file_name;
    if (try_extract_video_name_recursive(options, file_name)) {
        return file_name;
    }

    int number = 1;
    if (try_extract_video_number_recursive(options, number)) {
        return video_name_from_number(number);
    }

    return "screamer1.mp4";
}

bool is_empty_task_option(const nlohmann::json& option) {
    if (option.is_null()) {
        return true;
    }
    if (option.is_string()) {
        const std::string value = to_lower_copy(trim_copy(option.get<std::string>()));
        return value.empty() || value == "null" || value == "none";
    }
    if (option.is_object()) {
        return option.empty();
    }
    if (option.is_array()) {
        return option.empty();
    }
    return false;
}

bool normalize_video_name_for_control(const std::string& raw, std::string& out_name) {
    const std::string token = extract_video_name_token(raw);
    if (!token.empty()) {
        out_name = token;
        return true;
    }

    const std::string trimmed = trim_copy(raw);
    if (trimmed.empty() || !has_any_digit(trimmed)) {
        return false;
    }

    out_name = video_name_from_number(parse_video_number_from_string(trimmed));
    return true;
}
}  // namespace agent_video_utils
