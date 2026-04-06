#pragma once

#include <string>

#include <json.hpp>

namespace agent_video_utils {
std::string extract_video_name_token(const std::string& raw);
int parse_video_number_from_string(const std::string& raw);
std::string resolve_video_name(const nlohmann::json& options);
bool is_empty_task_option(const nlohmann::json& option);
bool normalize_video_name_for_control(const std::string& raw, std::string& out_name);
}  // namespace agent_video_utils
