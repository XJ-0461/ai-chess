#pragma once

#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <zmq.hpp>
#include <nlohmann/json.hpp>

namespace chess::game::player {

// A thin, synchronous ZMQ PAIR connection to a single chess-agent process (the
// agent binds; we connect). It is NOT thread-safe and is intended to be driven
// by exactly one thread at a time: the handshake runs on the setup thread, and
// each subsequent request is handled by its own dedicated, short-lived thread
// (requests are strictly sequential, so the socket is never used concurrently).
class AgentConnection {
public:
    explicit AgentConnection(std::string endpoint)
        : endpoint_(std::move(endpoint)) {}

    ~AgentConnection() { Close(); }

    AgentConnection(const AgentConnection&) = delete;
    AgentConnection& operator=(const AgentConnection&) = delete;

    [[nodiscard]] bool Open() {
        try {
            socket_ = zmq::socket_t(context_, zmq::socket_type::pair);
            // Bound every blocking socket call so a dead/unresponsive agent can
            // never wedge a request thread (which would, in turn, block the actor
            // environment's join on shutdown and keep the process alive).
            // linger 0: closing the socket drops unsent frames instead of waiting.
            socket_.set(zmq::sockopt::linger, 0);
            socket_.set(zmq::sockopt::sndtimeo, static_cast<int>(kSendTimeout.count()));
            socket_.connect(endpoint_);
            open_ = true;
            return true;
        } catch (const zmq::error_t& e) {
            std::cerr << "[AgentConnection] connect failed (" << endpoint_ << "): " << e.what() << "\n";
            return false;
        }
    }

    void Close() {
        if (open_) {
            socket_.close();
            open_ = false;
        }
    }

    [[nodiscard]] bool IsOpen() const { return open_; }

    bool Send(const nlohmann::json& message) {
        if (!open_) {
            return false;
        }
        try {
            const std::string payload = message.dump();
            socket_.send(zmq::buffer(payload), zmq::send_flags::none);
            return true;
        } catch (const zmq::error_t& e) {
            std::cerr << "[AgentConnection] send failed: " << e.what() << "\n";
            return false;
        }
    }

    // Blocks up to `timeout` for one JSON message. Returns nullopt on timeout or
    // error (e.g. a malformed payload).
    [[nodiscard]] std::optional<nlohmann::json> Receive(std::chrono::milliseconds timeout) {
        if (!open_) {
            return std::nullopt;
        }
        try {
            std::vector<zmq::pollitem_t> items = {
                { static_cast<void*>(socket_), 0, ZMQ_POLLIN, 0 }
            };
            const int ready = zmq::poll(items, timeout);
            if (ready <= 0 || (items[0].revents & ZMQ_POLLIN) == 0) {
                return std::nullopt;
            }
            zmq::message_t frame;
            const auto received = socket_.recv(frame, zmq::recv_flags::none);
            if (!received) {
                return std::nullopt;
            }
            const std::string payload(static_cast<const char*>(frame.data()), frame.size());
            return nlohmann::json::parse(payload);
        } catch (const std::exception& e) {
            std::cerr << "[AgentConnection] receive error: " << e.what() << "\n";
            return std::nullopt;
        }
    }

private:
    // Upper bound on a blocking send to a healthy local agent (sends are normally
    // instant); a finite value guarantees a request thread can never hang here.
    static constexpr auto kSendTimeout = std::chrono::milliseconds(2000);

    std::string endpoint_;
    zmq::context_t context_{1};
    zmq::socket_t socket_{};
    bool open_{false};
};

} // namespace chess::game::player
