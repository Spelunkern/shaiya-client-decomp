#include "tcp.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace genesis::net {
namespace {
#ifdef _WIN32
using Socket = SOCKET;
constexpr auto invalid_socket = INVALID_SOCKET;
int socket_error() {
    return WSAGetLastError();
}
bool pending(int error) {
    return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEALREADY;
}
bool interrupted(int error) {
    return error == WSAEINTR;
}
void close_socket(Socket socket) {
    closesocket(socket);
}
void nonblocking(Socket socket) {
    u_long enabled = 1;
    if (ioctlsocket(socket, FIONBIO, &enabled) != 0)
        throw std::runtime_error("Cannot make socket nonblocking");
}
#else
using Socket = int;
constexpr Socket invalid_socket = -1;
int socket_error() {
    return errno;
}
bool pending(int error) {
    return error == EAGAIN || error == EWOULDBLOCK || error == EINPROGRESS || error == EALREADY;
}
bool interrupted(int error) {
    return error == EINTR;
}
void close_socket(Socket socket) {
    ::close(socket);
}
void nonblocking(Socket socket) {
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags < 0 || fcntl(socket, F_SETFL, flags | O_NONBLOCK) != 0)
        throw std::runtime_error("Cannot make socket nonblocking");
    flags = fcntl(socket, F_GETFD, 0);
    if (flags < 0 || fcntl(socket, F_SETFD, flags | FD_CLOEXEC) != 0)
        throw std::runtime_error("Cannot set socket close-on-exec");
}
#endif
} // namespace
struct Tcp::Impl {
    Socket socket = invalid_socket;
    State state = State::disconnected;
    Bytes queued;
    std::size_t sent = 0;
    std::chrono::steady_clock::time_point deadline;
    Impl() {
#ifdef _WIN32
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
            throw std::runtime_error("Winsock initialization failed");
#endif
    }
    ~Impl() {
        close(State::disconnected);
#ifdef _WIN32
        WSACleanup();
#endif
    }
    void close(State next) {
        if (socket != invalid_socket)
            close_socket(socket);
        socket = invalid_socket;
        state = next;
        queued.clear();
        sent = 0;
    }
    [[noreturn]] void fail(const char* operation, int error) {
        close(State::failed);
        throw std::runtime_error(std::string(operation) + " failed (socket error " +
                                 std::to_string(error) + ")");
    }
};
Tcp::Tcp() : impl_(std::make_unique<Impl>()) {}
Tcp::~Tcp() = default;
Tcp::State Tcp::state() const {
    return impl_->state;
}
std::size_t Tcp::queued_bytes() const {
    return impl_->queued.size() - impl_->sent;
}
void Tcp::close() {
    impl_->close(State::disconnected);
}
void Tcp::connect_ipv4(const std::string& address, std::uint16_t port,
                       std::chrono::milliseconds timeout) {
    if (!port || timeout.count() <= 0)
        throw std::invalid_argument("Invalid connection port or timeout");
    sockaddr_in peer = {};
    peer.sin_family = AF_INET;
    peer.sin_port = htons(port);
    if (address.find('\0') != std::string::npos ||
        inet_pton(AF_INET, address.c_str(), &peer.sin_addr) != 1)
        throw std::invalid_argument("Invalid numeric IPv4 address");
    close();
    auto& connection = *impl_;
    connection.socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connection.socket == invalid_socket)
        connection.fail("socket", socket_error());
    try {
        nonblocking(connection.socket);
    } catch (...) {
        connection.close(State::failed);
        throw;
    }
    const int result =
        ::connect(connection.socket, reinterpret_cast<const sockaddr*>(&peer), sizeof(peer));
    if (result != 0 && !pending(socket_error()))
        connection.fail("connect", socket_error());
    connection.state = result == 0 ? State::connected : State::connecting;
    connection.deadline = std::chrono::steady_clock::now() + timeout;
}
void Tcp::queue(const Bytes& bytes) {
    auto& connection = *impl_;
    if (connection.state != State::connecting && connection.state != State::connected)
        throw std::runtime_error("Cannot queue bytes on a disconnected socket");
    constexpr std::size_t limit = 1024 * 1024;
    if (bytes.size() > limit || bytes.size() > limit - queued_bytes())
        throw std::runtime_error("Send queue limit exceeded");
    if (connection.sent) {
        connection.queued.erase(connection.queued.begin(),
                                connection.queued.begin() +
                                    static_cast<std::ptrdiff_t>(connection.sent));
        connection.sent = 0;
    }
    connection.queued.insert(connection.queued.end(), bytes.begin(), bytes.end());
}
Bytes Tcp::poll(int timeout_ms) {
    if (timeout_ms < 0 || timeout_ms > 100)
        throw std::invalid_argument("Socket poll timeout must be 0..100 ms");
    auto& connection = *impl_;
    Bytes received;
    if (connection.socket == invalid_socket)
        return received;
    if (connection.state == State::connecting &&
        std::chrono::steady_clock::now() >= connection.deadline)
        connection.fail("connect timeout", 0);
    bool readable = false, writable = false, error = false;
#ifdef _WIN32
    // select's exceptfds is required to observe failed nonblocking connects.
    fd_set reads, writes, errors;
    FD_ZERO(&reads);
    FD_ZERO(&writes);
    FD_ZERO(&errors);
    FD_SET(connection.socket, &reads);
    FD_SET(connection.socket, &errors);
    if (connection.state == State::connecting || queued_bytes())
        FD_SET(connection.socket, &writes);
    timeval timeout = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    int ready = select(0, &reads, &writes, &errors, &timeout);
    if (ready < 0) {
        int code = socket_error();
        if (interrupted(code))
            return received;
        connection.fail("select", code);
    }
    readable = FD_ISSET(connection.socket, &reads) != 0;
    writable = FD_ISSET(connection.socket, &writes) != 0;
    error = FD_ISSET(connection.socket, &errors) != 0;
#else
    pollfd descriptor = {connection.socket, POLLIN, 0};
    if (connection.state == State::connecting || queued_bytes())
        descriptor.events |= POLLOUT;
    int ready = ::poll(&descriptor, 1, timeout_ms);
    if (ready < 0) {
        int code = socket_error();
        if (interrupted(code))
            return received;
        connection.fail("poll", code);
    }
    readable = (descriptor.revents & (POLLIN | POLLHUP)) != 0;
    writable = (descriptor.revents & POLLOUT) != 0;
    error = (descriptor.revents & (POLLERR | POLLNVAL)) != 0;
#endif
    if (connection.state == State::connecting && (readable || writable || error)) {
        int status = 0;
#ifdef _WIN32
        int length = sizeof(status);
#else
        socklen_t length = sizeof(status);
#endif
        if (getsockopt(connection.socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&status),
                       &length) != 0)
            connection.fail("getsockopt", socket_error());
        if (status)
            connection.fail("connect", status);
        connection.state = State::connected;
    } else if (error && !readable)
        connection.fail("socket poll", socket_error());
    // Bound per-frame work so a busy peer cannot stall rendering indefinitely.
    if (readable) {
        std::array<char, 8192> buffer;
        while (received.size() < 65536) {
            const auto result =
                ::recv(connection.socket, buffer.data(), static_cast<int>(buffer.size()), 0);
            if (result > 0) {
                received.insert(received.end(), buffer.begin(), buffer.begin() + result);
                continue;
            }
            if (!result) {
                connection.close(State::peer_closed);
                return received;
            }
            int code = socket_error();
            if (interrupted(code))
                continue;
            if (pending(code))
                break;
            connection.fail("recv", code);
        }
    }
    if (writable && queued_bytes()) {
#ifdef _WIN32
        constexpr int flags = 0;
#else
        constexpr int flags = MSG_NOSIGNAL;
#endif
        auto count = std::min<std::size_t>(queued_bytes(), 65536);
        const auto result =
            ::send(connection.socket,
                   reinterpret_cast<const char*>(connection.queued.data() + connection.sent),
                   static_cast<int>(count), flags);
        if (result > 0)
            connection.sent += static_cast<std::size_t>(result);
        else if (result == 0)
            connection.fail("send closed", 0);
        else {
            int code = socket_error();
            if (!pending(code) && !interrupted(code))
                connection.fail("send", code);
        }
        if (!queued_bytes()) {
            connection.queued.clear();
            connection.sent = 0;
        }
    }
    return received;
}
} // namespace genesis::net
