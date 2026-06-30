#pragma once

#include <zmq.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <unofficial/concurrentqueue/concurrentqueue.h>

namespace chess::application::command {

// The ROUTER routing envelope that precedes a request payload (the identity
// frame, plus an empty delimiter for REQ-style clients). Echoed verbatim on
// reply so the router delivers the response to the originating client.
using ClientIdentity = std::vector<std::string>;

struct ZMQCommandServerOptions {
    std::string endpoint{}; // e.g. "tcp://127.0.0.1:5599"
};

// A ZMQ ROUTER server running on its own thread for the app's lifetime. Inbound
// JSON requests are handed to a request handler (which forwards them into the
// command queue and returns immediately); replies are posted back later via
// Reply() and sent — correctly routed by identity — from the server thread.
// This mirrors the threading / lock-free-queue model of ZMQPairChannel. ROUTER
// (rather than REP) is used so replies can come back out of lockstep and after
// an arbitrary delay, which is what makes the request/response asynchronous.
class ZMQCommandServer {
public:
    // Invoked on the server thread for each request. Must not block: forward the
    // request into the command queue and return; the eventual result is sent via
    // Reply() from whatever thread produces it.
    using RequestHandler = std::function<void(ClientIdentity, std::string)>;

    explicit ZMQCommandServer(ZMQCommandServerOptions options)
        : options_(std::move(options)) {}

    ~ZMQCommandServer() {
        Stop();
    }

    ZMQCommandServer(const ZMQCommandServer&) = delete;
    ZMQCommandServer& operator=(const ZMQCommandServer&) = delete;
    ZMQCommandServer(ZMQCommandServer&&) = delete;
    ZMQCommandServer& operator=(ZMQCommandServer&&) = delete;

    void SetRequestHandler(RequestHandler handler) {
        on_request_ = std::move(handler);
    }

    // Thread-safe: enqueue a reply to be routed back to `identity` and sent on
    // the server thread.
    void Reply(ClientIdentity identity, std::string payload) {
        response_queue_.enqueue(OutboundResponse{ std::move(identity), std::move(payload) });
    }

    void Start() {
        if (is_running_.exchange(true)) {
            return; // already running
        }
        io_thread_ = std::jthread([this](std::stop_token st) { WorkerLoop(st); });
    }

    void Stop() {
        if (!is_running_.exchange(false)) {
            return; // already stopped
        }
        if (io_thread_.joinable()) {
            io_thread_.request_stop();
            io_thread_.join();
        }
    }

private:
    struct OutboundResponse {
        ClientIdentity identity{};
        std::string payload{};
    };

    void WorkerLoop(std::stop_token stop_token) {
        // The ZeroMQ context and socket live entirely on this thread.
        zmq::context_t ctx(1);
        zmq::socket_t router(ctx, zmq::socket_type::router);
        try {
            router.bind(options_.endpoint);
        } catch (const zmq::error_t& e) {
            std::cerr << "[ZMQCommandServer] bind failed on '" << options_.endpoint
                      << "': " << e.what() << "\n";
            is_running_.store(false);
            return;
        }

        std::vector<zmq::pollitem_t> items = {
            { static_cast<void*>(router), 0, ZMQ_POLLIN, 0 }
        };

        while (!stop_token.stop_requested()) {
            const std::int32_t rc = zmq::poll(items, std::chrono::milliseconds{50});

            if (rc > 0 && (items[0].revents & ZMQ_POLLIN)) {
                // Read every frame of the message: for a ROUTER socket the
                // leading frame(s) are the routing envelope and the final frame
                // is the request payload.
                std::vector<std::string> frames;
                zmq::message_t frame;
                bool ok = true;
                while (true) {
                    const auto res = router.recv(frame, zmq::recv_flags::none);
                    if (!res) { ok = false; break; }
                    frames.emplace_back(static_cast<char*>(frame.data()), frame.size());
                    if (!frame.more()) break;
                }
                if (ok && frames.size() >= 2 && on_request_) {
                    std::string payload = std::move(frames.back());
                    frames.pop_back();
                    on_request_(std::move(frames), std::move(payload));
                }
            }

            // Flush queued replies back to their originating clients.
            OutboundResponse response;
            while (response_queue_.try_dequeue(response)) {
                for (const auto& envelope_frame : response.identity) {
                    router.send(zmq::message_t(envelope_frame.data(), envelope_frame.size()),
                                zmq::send_flags::sndmore);
                }
                router.send(zmq::message_t(response.payload.data(), response.payload.size()),
                            zmq::send_flags::none);
            }
        }
    }

    ZMQCommandServerOptions options_;
    RequestHandler on_request_{};
    std::jthread io_thread_{};
    std::atomic_bool is_running_{false};
    moodycamel::ConcurrentQueue<OutboundResponse> response_queue_{};
};

} // namespace chess::application::command
