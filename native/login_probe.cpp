#include "login.h"
#include "tcp.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

using namespace genesis::net;
int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "Usage: genesis_login_probe <numeric-ipv4> <port>\n"
                             "Exchanges A101 only; does not submit account credentials.\n");
        return 2;
    }
    try {
        char* end = nullptr;
        long port = std::strtol(argv[2], &end, 10);
        if (!*argv[2] || *end || port < 1 || port > 65535)
            throw std::invalid_argument("Invalid port");
        Tcp socket;
        Login login;
        socket.connect_ipv4(argv[1], static_cast<std::uint16_t>(port));
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < deadline) {
            auto received = socket.poll(10);
            if (!received.empty())
                login.receive(received.data(), received.size());
            auto outgoing = login.take_outgoing();
            if (!outgoing.empty())
                socket.queue(outgoing);
            if (login.state() == Login::State::ready && !socket.queued_bytes()) {
                std::printf(
                    "A101 exchange completed; native %zu-bit transport; credentials not sent\n",
                    sizeof(void*) * 8);
                return 0;
            }
            if (socket.state() == Tcp::State::peer_closed)
                throw std::runtime_error("Peer closed during A101 exchange");
        }
        throw std::runtime_error("A101 exchange timed out");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
