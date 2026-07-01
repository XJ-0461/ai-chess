#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <variant>
#include <vector>

#include "Utility/AgentTrajectory.h"
#include "Game/GameMoveLog.hpp"
#include "Game/Error/AgentMoveError.hpp"

namespace chess::application {

enum class SidebarBubbleKind { Info, Error, Reasoning, Response, Quip, Move };

// A display-ready bubble, derived from a pure AgentTrajectory entry and (for
// moves) hydrated with game context from the GameMoveLog. The backing
// trajectory is never mutated by this transformation.
struct SidebarBubble {
    std::string id;
    std::size_t move_count{0};
    SidebarBubbleKind kind{SidebarBubbleKind::Info};
    std::string text;                                                      // display-ready
    bool is_error{false};                                                  // Info / Error
    MoveVerificationState move_state{MoveVerificationState::Unverified};    // Move
    std::vector<chess::game::error::MoveError> move_errors;                // Move: codified errors
};

// The Agent Sidebar's view-model: transforms a pure AgentTrajectory (+ the
// shared GameMoveLog) into display bubbles. Rebuilt from the backing whenever
// the sidebar renders; move bubbles correlate to the move log by id.
class AgentSidebarTrajectory {
public:
    void Rebuild(AgentTrajectory& trajectory, const std::shared_ptr<chess::game::GameMoveLog>& move_log) {
        std::vector<SidebarBubble> rebuilt;
        {
            const std::lock_guard<std::mutex> lock(trajectory.mtx);
            rebuilt.reserve(trajectory.events.size());
            for (const auto& entry : trajectory.events) {
                rebuilt.push_back(Transform(entry, move_log));
            }
        }
        bubbles_ = std::move(rebuilt);
    }

    [[nodiscard]] const std::vector<SidebarBubble>& Bubbles() const { return bubbles_; }

private:
    static SidebarBubble Transform(
        const chess::agent::TrajectoryEntry& entry,
        const std::shared_ptr<chess::game::GameMoveLog>& move_log
    ) {
        SidebarBubble bubble;
        bubble.id = entry.id;
        bubble.move_count = entry.move_count;
        std::visit([&](const auto& event) { Hydrate(bubble, event, move_log); }, entry.event);
        return bubble;
    }

    static void Hydrate(SidebarBubble& bubble, const chess::agent::InfoEvent& event,
                        const std::shared_ptr<chess::game::GameMoveLog>&) {
        if (event.isError) {
            bubble.kind = SidebarBubbleKind::Error;
            bubble.is_error = true;
            bubble.text = chess::game::error::CodifyAgentError(event.message).Format();
        } else {
            bubble.kind = SidebarBubbleKind::Info;
            bubble.text = event.message;
        }
    }

    static void Hydrate(SidebarBubble& bubble, const chess::agent::message::ReasoningSnapshot& event,
                        const std::shared_ptr<chess::game::GameMoveLog>&) {
        bubble.kind = SidebarBubbleKind::Reasoning;
        bubble.text = event.message;
    }

    static void Hydrate(SidebarBubble& bubble, const chess::agent::message::ResponseSnapshot& event,
                        const std::shared_ptr<chess::game::GameMoveLog>&) {
        bubble.kind = SidebarBubbleKind::Response;
        bubble.text = event.message;
    }

    static void Hydrate(SidebarBubble& bubble, const chess::agent::message::QuipResponse& event,
                        const std::shared_ptr<chess::game::GameMoveLog>&) {
        bubble.kind = SidebarBubbleKind::Quip;
        bubble.text = event.message;
    }

    static void Hydrate(SidebarBubble& bubble, const chess::agent::MoveEvent& event,
                        const std::shared_ptr<chess::game::GameMoveLog>& move_log) {
        bubble.kind = SidebarBubbleKind::Move;
        bubble.text = event.response.long_algebraic_move_string;
        // Hydrate verification + codified errors from the move log (game context).
        if (move_log) {
            if (const auto outcome = move_log->Find(bubble.id)) {
                bubble.move_state = outcome->accepted
                    ? MoveVerificationState::Verified
                    : MoveVerificationState::Error;
                bubble.move_errors = outcome->errors;
            }
        }
    }

    static void Hydrate(SidebarBubble& bubble, const chess::agent::message::ErrorResponse& event,
                        const std::shared_ptr<chess::game::GameMoveLog>&) {
        bubble.kind = SidebarBubbleKind::Error;
        bubble.is_error = true;
        bubble.text = chess::game::error::CodifyAgentError(event.message).Format();
    }

    std::vector<SidebarBubble> bubbles_;
};

} // namespace chess::application
