#pragma once
#include "tcp.h"
#include "wire.h"
#include <array>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace fixture {
using namespace genesis::net;
#ifdef _WIN32
using Socket = SOCKET;
inline constexpr auto invalid = INVALID_SOCKET;
inline void close_socket(Socket s) {
    closesocket(s);
}
inline void nonblocking(Socket s) {
    u_long mode = 1;
    if (ioctlsocket(s, FIONBIO, &mode))
        throw std::runtime_error("Fixture nonblocking");
}
#else
using Socket = int;
inline constexpr auto invalid = -1;
inline void close_socket(Socket s) {
    ::close(s);
}
inline void nonblocking(Socket s) {
    if (fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK))
        throw std::runtime_error("Fixture nonblocking");
}
#endif
struct OwnedSocket {
    Socket value = invalid;
    ~OwnedSocket() {
        if (value != invalid)
            close_socket(value);
    }
};
class Endpoint {
  public:
    Endpoint() {
        listener_.value = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener_.value == invalid)
            throw std::runtime_error("Fixture socket creation failed");
        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(listener_.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) ||
            listen(listener_.value, 1))
            throw std::runtime_error("Fixture listener failed");
#ifdef _WIN32
        int length = sizeof(address);
#else
        socklen_t length = sizeof(address);
#endif
        if (getsockname(listener_.value, reinterpret_cast<sockaddr*>(&address), &length))
            throw std::runtime_error("Fixture port lookup");
        port_ = ntohs(address.sin_port);
        nonblocking(listener_.value);
    }
    std::uint16_t port() const {
        return port_;
    }
    Wire wire;
    void send(const Packet& packet) {
        auto frame = wire.encode(packet);
        queued_.insert(queued_.end(), frame.begin(), frame.end());
    }
    void pump() {
        if (closed_)
            return;
        if (accepted_.value == invalid) {
            accepted_.value = accept(listener_.value, nullptr, nullptr);
            if (accepted_.value == invalid)
                return;
            nonblocking(accepted_.value);
        }
        std::array<char, 41> received;
        auto count = recv(accepted_.value, received.data(), static_cast<int>(received.size()), 0);
        if (count > 0)
            wire.feed(received.data(), static_cast<std::size_t>(count));
        else if (count == 0) {
            closed_ = true;
            return;
        }
        if (sent_ < queued_.size()) {
            auto remaining = std::min<std::size_t>(queued_.size() - sent_, 37);
#ifdef _WIN32
            constexpr int flags = 0;
#else
            constexpr int flags = MSG_NOSIGNAL;
#endif
            count = ::send(accepted_.value, reinterpret_cast<const char*>(queued_.data() + sent_),
                           static_cast<int>(remaining), flags);
            if (count > 0)
                sent_ += static_cast<std::size_t>(count);
        }
    }

  private:
    Tcp network_lifetime_;
    OwnedSocket listener_, accepted_;
    Bytes queued_;
    std::size_t sent_ = 0;
    std::uint16_t port_ = 0;
    bool closed_ = false;
};
} // namespace fixture
