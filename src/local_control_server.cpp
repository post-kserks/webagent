#include "local_control_server.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
#ifdef _WIN32
using socket_t = SOCKET;
constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
using socket_t = int;
constexpr socket_t kInvalidSocket = -1;
#endif

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

void close_socket(socket_t fd) {
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
}

bool send_all(socket_t fd, const std::string& data) {
    size_t offset = 0;
    while (offset < data.size()) {
        const int sent = static_cast<int>(
            send(fd, data.data() + offset, static_cast<int>(data.size() - offset), 0)
        );
        if (sent <= 0) {
            return false;
        }
        offset += static_cast<size_t>(sent);
    }
    return true;
}

std::string make_http_response(
    int code,
    const std::string& status,
    const std::string& body,
    const std::string& content_type = "application/json"
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

std::string lowercase_copy(const std::string& input) {
    std::string out = input;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
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
} // namespace

LocalControlServer::LocalControlServer(
    int port,
    SetVideoHandler set_video_handler,
    GetVideoHandler get_video_handler,
    LogHandler log_handler
) : port_(port),
    set_video_handler_(std::move(set_video_handler)),
    get_video_handler_(std::move(get_video_handler)),
    log_handler_(std::move(log_handler)),
    running_(false),
    server_fd_(kInvalidSocket),
    server_thread_() {}

LocalControlServer::~LocalControlServer() {
    stop();
}

bool LocalControlServer::start() {
    if (running_) {
        return true;
    }

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        log("LocalControlServer: WSAStartup failed");
        return false;
    }
#endif

    server_fd_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (server_fd_ == kInvalidSocket) {
        log("LocalControlServer: failed to create socket");
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    int yes = 1;
    setsockopt(
        static_cast<socket_t>(server_fd_),
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<const char*>(&yes),
        static_cast<socklen_t>(sizeof(yes))
    );

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(static_cast<socket_t>(server_fd_), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        log("LocalControlServer: bind failed on 127.0.0.1:" + std::to_string(port_));
        close_socket(static_cast<socket_t>(server_fd_));
        server_fd_ = kInvalidSocket;
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    if (listen(static_cast<socket_t>(server_fd_), 8) != 0) {
        log("LocalControlServer: listen failed");
        close_socket(static_cast<socket_t>(server_fd_));
        server_fd_ = kInvalidSocket;
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    running_ = true;
    server_thread_ = std::thread([this]() { server_loop(); });
    log("LocalControlServer: started on http://127.0.0.1:" + std::to_string(port_));
    return true;
}

void LocalControlServer::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    if (server_fd_ != kInvalidSocket) {
#ifdef _WIN32
        shutdown(static_cast<socket_t>(server_fd_), SD_BOTH);
#else
        shutdown(static_cast<socket_t>(server_fd_), SHUT_RDWR);
#endif
        close_socket(static_cast<socket_t>(server_fd_));
        server_fd_ = kInvalidSocket;
    }
#ifdef _WIN32
    WSACleanup();
#endif

    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void LocalControlServer::server_loop() {
    while (running_) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        const socket_t client_fd = accept(
            static_cast<socket_t>(server_fd_),
            reinterpret_cast<sockaddr*>(&client_addr),
            &client_len
        );
        if (client_fd == kInvalidSocket) {
            if (!running_) {
                break;
            }
            continue;
        }

        std::string request;
        std::array<char, 4096> buffer{};
        int content_length = 0;
        bool header_parsed = false;
        size_t header_end_pos = std::string::npos;

        while (true) {
            const int received = static_cast<int>(recv(client_fd, buffer.data(), static_cast<int>(buffer.size()), 0));
            if (received <= 0) {
                break;
            }
            request.append(buffer.data(), static_cast<size_t>(received));

            if (!header_parsed) {
                header_end_pos = request.find("\r\n\r\n");
                if (header_end_pos != std::string::npos) {
                    header_parsed = true;
                    const std::string headers = request.substr(0, header_end_pos + 4);
                    content_length = parse_content_length(headers);
                }
            }

            if (header_parsed) {
                const size_t body_size = request.size() - (header_end_pos + 4);
                if (body_size >= static_cast<size_t>(content_length)) {
                    break;
                }
            }
        }

        std::string response = make_http_response(
            400, "Bad Request", "{\"ok\":false,\"error\":\"bad request\"}"
        );

        const size_t line_end = request.find("\r\n");
        if (line_end != std::string::npos) {
            const std::string request_line = request.substr(0, line_end);
            std::string method;
            std::string path;
            if (parse_request_line(request_line, method, path)) {
                const std::string clean_path = strip_query_string(path);

                if (method == "OPTIONS") {
                    response = make_http_response(204, "No Content", "", "text/plain");
                } else if (clean_path == "/api/selected-video" && method == "GET") {
                    const std::string selected = get_video_handler_ ? get_video_handler_() : "";
                    response = make_http_response(
                        200,
                        "OK",
                        "{\"ok\":true,\"video\":\"" + escape_json(selected) + "\"}"
                    );
                } else if (clean_path == "/api/selected-video" && method == "POST") {
                    std::string body;
                    const size_t body_pos = request.find("\r\n\r\n");
                    if (body_pos != std::string::npos) {
                        body = request.substr(body_pos + 4);
                    }
                    const std::string video = json_extract_video_field(body);
                    if (video.empty()) {
                        response = make_http_response(
                            400,
                            "Bad Request",
                            "{\"ok\":false,\"error\":\"video is required\"}"
                        );
                    } else {
                        const bool accepted = set_video_handler_ ? set_video_handler_(video) : false;
                        if (accepted) {
                            response = make_http_response(200, "OK", "{\"ok\":true}");
                        } else {
                            response = make_http_response(
                                400,
                                "Bad Request",
                                "{\"ok\":false,\"error\":\"invalid video value\"}"
                            );
                        }
                    }
                } else {
                    response = make_http_response(
                        404,
                        "Not Found",
                        "{\"ok\":false,\"error\":\"not found\"}"
                    );
                }
            }
        }

        send_all(client_fd, response);
        close_socket(client_fd);
    }
}

void LocalControlServer::log(const std::string& message) const {
    if (log_handler_) {
        log_handler_(message);
    }
}
