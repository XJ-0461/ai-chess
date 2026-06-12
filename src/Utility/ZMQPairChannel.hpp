#pragma once

#include <zmq.hpp>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <iostream>

#include <unofficial/concurrentqueue/concurrentqueue.h>

struct ZMQPairChannelOptions {
    std::string endpoint{};
};

class ZMQPairChannel {
public:

    explicit ZMQPairChannel(ZMQPairChannelOptions options)
        : options_(std::move(options)) {}

    // Destructor explicitly calls Stop() to guarantee clean teardown before
    // the underlying resources and queues are freed.
    ~ZMQPairChannel() {
        Stop();
    }

    // Delete copy/move constructors to preserve socket/thread uniqueness rules
    ZMQPairChannel(const ZMQPairChannel&) = delete;
    ZMQPairChannel& operator=(const ZMQPairChannel&) = delete;
    ZMQPairChannel(ZMQPairChannel&&) = delete;
    ZMQPairChannel& operator=(ZMQPairChannel&&) = delete;

    void SetReceiveQueue(std::shared_ptr<moodycamel::ConcurrentQueue<std::string>> receive_queue) {
        receive_queue_ = std::move(receive_queue);
    }

    // Lock-free lockless queueing. Returns false if the queueing fails.
    bool SendMessage(const std::string& message) {
        if (!is_running_.load(std::memory_order_relaxed)) {
            return false;
        }
        return send_queue_.enqueue(message);
    }

    void Start() {
        if (is_running_.exchange(true)) {
            return; // Already running
        }
        // std::jthread passes its stop_token automatically as the first parameter
        io_thread_ = std::jthread([this](std::stop_token st) {
            WorkerLoop(st);
        });
    }

    void Stop() {
        if (!is_running_.exchange(false)) {
            return; // Already stopped
        }
        // Request stop via the modern C++20 stop_token and join the thread
        if (io_thread_.joinable()) {
            io_thread_.request_stop();
            io_thread_.join();
        }
    }

private:

    void WorkerLoop(std::stop_token stop_token) {
        // ZeroMQ instances are fully contained within this worker thread for absolute safety
        zmq::context_t ctx(1);
        zmq::socket_t pair_sock(ctx, zmq::socket_type::pair);

        try {
            pair_sock.connect(options_.endpoint);
        } catch (const zmq::error_t& e) {
            std::cerr << "ZMQ Connection failed: " << e.what() << "\n";
            is_running_.store(false);
            return;
        }

        std::vector<zmq::pollitem_t> items = {
            { static_cast<void*>(pair_sock), 0, ZMQ_POLLIN, 0 }
        };

        while (!stop_token.stop_requested()) {
            // 1. POLL FOR INBOUND MESSAGES
            // Short 50ms block lets outbound messages and stop tokens get handled regularly
            std::int32_t rc = zmq::poll(items, std::chrono::milliseconds{50});

            if (rc > 0 && (items[0].revents & ZMQ_POLLIN)) {
                zmq::message_t inbound_msg;
                auto res = pair_sock.recv(inbound_msg, zmq::recv_flags::none);

                if (res && receive_queue_) {
                    // Extract payload directly to a string and move it lock-free
                    std::string payload(static_cast<char*>(inbound_msg.data()), inbound_msg.size());
                    receive_queue_->enqueue(std::move(payload));
                }
            }

            // 2. FLUSH OUTBOUND MESSAGES FROM LOCK-FREE QUEUE
            std::string outbound_str;
            while (send_queue_.try_dequeue(outbound_str)) {
                zmq::message_t msg(outbound_str.data(), outbound_str.size());
                pair_sock.send(msg, zmq::send_flags::none);
            }
        }
    }

    ZMQPairChannelOptions options_;
    std::jthread io_thread_{};

    std::atomic_bool is_running_{false};
    moodycamel::ConcurrentQueue<std::string> send_queue_{};
    std::shared_ptr<moodycamel::ConcurrentQueue<std::string>> receive_queue_{nullptr};
};
