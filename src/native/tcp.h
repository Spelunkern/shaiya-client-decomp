#pragma once
#include "packet.h"
#include <chrono>
#include <memory>

namespace genesis::net {
// Native socket-width storage lives in the platform implementation, never a
// recovered uint32_t field. Both backends expose the same nonblocking contract.
class Tcp {
  public:
    enum class State { disconnected, connecting, connected, peer_closed, failed };
    Tcp();
    ~Tcp();
    Tcp(const Tcp&) = delete;
    Tcp& operator=(const Tcp&) = delete;
    void connect_ipv4(const std::string& address, std::uint16_t port,
                      std::chrono::milliseconds timeout = std::chrono::seconds(10));
    void close();
    void queue(const Bytes& bytes);
    Bytes poll(int timeout_ms = 0);
    State state() const;
    std::size_t queued_bytes() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace genesis::net
