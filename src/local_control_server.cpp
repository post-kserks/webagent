#include "local_control_server.h"
#include "local_control_http_utils.h"

#include <array>
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
constexpr std::intptr_t kStoredInvalidSocket = -1;

static_assert(
    sizeof(std::intptr_t) >= sizeof(socket_t),
    "std::intptr_t must be large enough to store native socket type"
);

std::intptr_t to_stored_socket(socket_t socket) {
    return static_cast<std::intptr_t>(socket);
}

socket_t to_native_socket(std::intptr_t socket) {
    return static_cast<socket_t>(socket);
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
    server_fd_(kStoredInvalidSocket),
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

    const socket_t server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == kInvalidSocket) {
        log("LocalControlServer: failed to create socket");
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }
    server_fd_ = to_stored_socket(server_socket);

    int yes = 1;
    setsockopt(
        to_native_socket(server_fd_),
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<const char*>(&yes),
        static_cast<socklen_t>(sizeof(yes))
    );

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(to_native_socket(server_fd_), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        log("LocalControlServer: bind failed on 127.0.0.1:" + std::to_string(port_));
        close_socket(to_native_socket(server_fd_));
        server_fd_ = kStoredInvalidSocket;
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    if (listen(to_native_socket(server_fd_), 8) != 0) {
        log("LocalControlServer: listen failed");
        close_socket(to_native_socket(server_fd_));
        server_fd_ = kStoredInvalidSocket;
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
    if (server_fd_ != kStoredInvalidSocket) {
#ifdef _WIN32
        shutdown(to_native_socket(server_fd_), SD_BOTH);
#else
        shutdown(to_native_socket(server_fd_), SHUT_RDWR);
#endif
        close_socket(to_native_socket(server_fd_));
        server_fd_ = kStoredInvalidSocket;
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
            to_native_socket(server_fd_),
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
                    content_length = local_control_http_utils::parse_content_length(headers);
                }
            }

            if (header_parsed) {
                const size_t body_size = request.size() - (header_end_pos + 4);
                if (body_size >= static_cast<size_t>(content_length)) {
                    break;
                }
            }
        }

        std::string response = local_control_http_utils::make_http_response(
            400, "Bad Request", "{\"ok\":false,\"error\":\"bad request\"}"
        );

        const size_t line_end = request.find("\r\n");
        if (line_end != std::string::npos) {
            const std::string request_line = request.substr(0, line_end);
            std::string method;
            std::string path;
            if (local_control_http_utils::parse_request_line(request_line, method, path)) {
                const std::string clean_path = local_control_http_utils::strip_query_string(path);

                if (method == "OPTIONS") {
                    response = local_control_http_utils::make_http_response(
                        204, "No Content", "", "text/plain"
                    );
                } else if (clean_path == "/api/selected-video" && method == "GET") {
                    const std::string selected = get_video_handler_ ? get_video_handler_() : "";
                    response = local_control_http_utils::make_http_response(
                        200,
                        "OK",
                        "{\"ok\":true,\"video\":\"" +
                            local_control_http_utils::escape_json(selected) + "\"}"
                    );
                } else if (clean_path == "/api/selected-video" && method == "POST") {
                    std::string body;
                    const size_t body_pos = request.find("\r\n\r\n");
                    if (body_pos != std::string::npos) {
                        body = request.substr(body_pos + 4);
                    }
                    const std::string video = local_control_http_utils::json_extract_video_field(body);
                    if (video.empty()) {
                        response = local_control_http_utils::make_http_response(
                            400,
                            "Bad Request",
                            "{\"ok\":false,\"error\":\"video is required\"}"
                        );
                    } else {
                        const bool accepted = set_video_handler_ ? set_video_handler_(video) : false;
                        if (accepted) {
                            response = local_control_http_utils::make_http_response(
                                200, "OK", "{\"ok\":true}"
                            );
                        } else {
                            response = local_control_http_utils::make_http_response(
                                400,
                                "Bad Request",
                                "{\"ok\":false,\"error\":\"invalid video value\"}"
                            );
                        }
                    }
                } else {
                    response = local_control_http_utils::make_http_response(
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
