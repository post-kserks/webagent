#pragma once

#include <string>

namespace local_control_http_utils {
std::string make_http_response(
    int code,
    const std::string& status,
    const std::string& body,
    const std::string& content_type = "application/json"
);
std::string escape_json(const std::string& input);
std::string json_extract_video_field(const std::string& body);
bool parse_request_line(
    const std::string& request_line,
    std::string& method,
    std::string& path
);
int parse_content_length(const std::string& headers);
std::string strip_query_string(const std::string& path);
}  // namespace local_control_http_utils
