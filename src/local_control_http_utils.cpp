#include "local_control_http_utils.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <string>

namespace {
std::string trim_copy(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::string lowercase_copy(const std::string& input) {
    std::string out = input;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return out;
}
}  // namespace

namespace local_control_http_utils {
std::string make_http_response(
    int code,
    const std::string& status,
    const std::string& body,
    const std::string& content_type
) {
    std::ostringstream out;
    out << "HTTP/1.1 " << code << " " << status << "\r\n";
    out << "Content-Type: " << content_type << "\r\n";
    out << "Access-Control-Allow-Origin: *\r\n";
    out << "Access-Control-Allow-Headers: Content-Type\r\n";
    out << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    out << "Connection: close\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "\r\n";
    out << body;
    return out.str();
}

std::string escape_json(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += ch; break;
        }
    }
    return out;
}

std::string json_extract_video_field(const std::string& body) {
    const std::string key = "\"video\"";
    size_t key_pos = body.find(key);
    if (key_pos == std::string::npos) {
        return "";
    }
    size_t colon = body.find(':', key_pos + key.size());
    if (colon == std::string::npos) {
        return "";
    }
    size_t first_quote = body.find('"', colon + 1);
    if (first_quote == std::string::npos) {
        return "";
    }
    std::string value;
    bool escaping = false;
    for (size_t i = first_quote + 1; i < body.size(); ++i) {
        const char ch = body[i];
        if (escaping) {
            switch (ch) {
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default: value.push_back(ch); break;
            }
            escaping = false;
            continue;
        }
        if (ch == '\\') {
            escaping = true;
            continue;
        }
        if (ch == '"') {
            return trim_copy(value);
        }
        value.push_back(ch);
    }
    return "";
}

bool parse_request_line(
    const std::string& request_line,
    std::string& method,
    std::string& path
) {
    std::istringstream in(request_line);
    std::string version;
    if (!(in >> method >> path >> version)) {
        return false;
    }
    if (path.empty()) {
        return false;
    }
    return true;
}

int parse_content_length(const std::string& headers) {
    std::istringstream in(headers);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::string lower = lowercase_copy(line);
        if (lower.rfind("content-length:", 0) == 0) {
            const std::string raw = trim_copy(line.substr(std::strlen("Content-Length:")));
            try {
                const int value = std::stoi(raw);
                return value < 0 ? 0 : value;
            } catch (...) {
                return 0;
            }
        }
    }
    return 0;
}

std::string strip_query_string(const std::string& path) {
    const size_t pos = path.find('?');
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(0, pos);
}
}  // namespace local_control_http_utils
