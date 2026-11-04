#include "tcp.h"
#include <array>
#include <cstdio>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
constexpr auto invalid_socket = INVALID_SOCKET;
void close_socket(Socket s) {
    closesocket(s);
}
void nonblocking(Socket s) {
    u_long mode = 1;
    if (ioctlsocket(s, FIONBIO, &mode))
        throw std::runtime_error("Test nonblocking");
}
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket = int;
constexpr auto invalid_socket = -1;
void close_socket(Socket s) {
    ::close(s);
}
void nonblocking(Socket s) {
    if (fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK))
        throw std::runtime_error("Test nonblocking");
}
#endif
using namespace genesis::net;
struct OwnedSocket {
    Socket value = invalid_socket;
    ~OwnedSocket() {
        if (value != invalid_socket)
            close_socket(value);
    }
};
void expect(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
int main() {
    try {
        Tcp client; // establishes Winsock lifetime for the raw loopback fixture
        OwnedSocket listener;
        listener.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        expect(listener.value != invalid_socket, "Loopback socket creation");
        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        expect(bind(listener.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0 &&
                   listen(listener.value, 1) == 0,
               "Loopback listener");
#ifdef _WIN32
        int length = sizeof(address);
#else
        socklen_t length = sizeof(address);
#endif
        expect(getsockname(listener.value, reinterpret_cast<sockaddr*>(&address), &length) == 0,
               "Ephemeral port lookup");
        nonblocking(listener.value);
        client.connect_ipv4("127.0.0.1", ntohs(address.sin_port));
        Bytes data(900000);
        for (std::size_t i = 0; i < data.size(); ++i)
            data[i] = std::uint8_t(i * 31);
        client.queue(data); // send while the nonblocking connect is pending
        OwnedSocket accepted;
        Bytes received, reply;
        std::size_t reply_sent = 0;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        std::array<char, 4093> buffer;
        while (std::chrono::steady_clock::now() < deadline && reply.size() < data.size()) {
            auto bytes = client.poll(1);
            reply.insert(reply.end(), bytes.begin(), bytes.end());
            if (accepted.value == invalid_socket) {
                accepted.value = accept(listener.value, nullptr, nullptr);
                if (accepted.value != invalid_socket)
                    nonblocking(accepted.value);
            }
            if (accepted.value != invalid_socket) {
                auto n = recv(accepted.value, buffer.data(), static_cast<int>(buffer.size()), 0);
                if (n > 0)
                    received.insert(received.end(), buffer.begin(), buffer.begin() + n);
                if (reply_sent < received.size()) {
                    auto count = std::min<std::size_t>(received.size() - reply_sent, 2039);
                    auto sent = send(accepted.value,
                                     reinterpret_cast<const char*>(received.data() + reply_sent),
                                     static_cast<int>(count), 0);
                    if (sent > 0)
                        reply_sent += std::size_t(sent);
                }
            }
        }
        expect(received == data && reply == data && client.queued_bytes() == 0,
               "Partial TCP send/receive lost or reordered bytes");
        close_socket(accepted.value);
        accepted.value = invalid_socket;
        while (std::chrono::steady_clock::now() < deadline &&
               client.state() != Tcp::State::peer_closed)
            (void)client.poll(1);
        expect(client.state() == Tcp::State::peer_closed, "Graceful TCP close not delivered");
        client.close();
        expect(client.state() == Tcp::State::disconnected, "TCP explicit close");
        close_socket(listener.value);
        listener.value = invalid_socket;
        bool refused = false;
        try {
            client.connect_ipv4("127.0.0.1", ntohs(address.sin_port), std::chrono::seconds(1));
            while (client.state() == Tcp::State::connecting)
                (void)client.poll(1);
        } catch (const std::runtime_error&) {
            refused = true;
        }
        expect(refused && client.state() == Tcp::State::failed,
               "Refused nonblocking connect not reported");
        std::puts("Native TCP loopback checks passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
