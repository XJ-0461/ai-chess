#pragma once

#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <variant>

#include <nlohmann/json.hpp>
#include <so_5/all.hpp>

#include "Game/GameContext.hpp"
#include "Game/GameLifecyclePhase.hpp"
#include "Game/Configuration/GameConfiguration.hpp"
#include "Game/Execution/MatchResult.hpp"
#include "Game/Execution/Message/Lifecycle.hpp"
#include "Application/Command/Command.hpp"
#include "Application/Command/CommandResponse.hpp"
#include "Application/Provider/ModelProviderConfiguration.hpp"

namespace chess::application::command {

// Thread-safe operations the executor performs on the application. The
// application binds these to its own (locked) methods.
struct CommandTargetCallbacks {
    std::function<void(const provider::ConfiguredProvider&)> configure_provider{};
    std::function<std::shared_ptr<chess::game::GameContext>(const chess::game::GameConfiguration&, const std::string&)> create_game{};
    std::function<std::shared_ptr<chess::game::GameContext>(const std::string&)> find_game{};
    // Opens a spectator view; must be dispatched to the UI thread because it
    // creates GL resources. The callback receives the response with window metadata.
    std::function<void(const OpenSpectatorViewCommand&,
                       std::function<void(OpenSpectatorViewResponse)>)> open_spectator_view{};
    // Toggles move history bar on a spectator view by window ID. Returns true if found.
    std::function<bool(const std::string& window_id, bool enable)> set_move_history_bar{};
};

// Message carrying one command for sequential execution. `reply`, when set, is
// invoked with the command's result (e.g. a query response) so it can be routed
// back to the original caller; it is null for fire-and-forget commands (such as
// those loaded from the startup command file).
struct ExecuteCommand {
    Command command{};
    std::function<void(const nlohmann::json&)> reply{};
};

// Executes external commands one at a time, blocking on its own dedicated
// thread until each completes before handling the next. Because it runs on its
// own one_thread dispatcher, blocking here never stalls the GUI or the game
// actors. Commands are delivered as messages and processed FIFO.
class ExternalCommandExecutor final : public so_5::agent_t {
public:
    ExternalCommandExecutor(context_t ctx, CommandTargetCallbacks callbacks)
        : so_5::agent_t{std::move(ctx)},
          callbacks_{std::move(callbacks)} {}

    void so_define_agent() override {
        so_subscribe_self().event(&ExternalCommandExecutor::OnExecuteCommand);
    }

private:
    static constexpr auto kCommandTimeout = std::chrono::seconds(30);
    static constexpr auto kPollInterval = std::chrono::milliseconds(20);

    void OnExecuteCommand(mhood_t<ExecuteCommand> message) {
        // Lifecycle commands are skipped once a prior command has aborted the
        // run; queries are always answered (they have no ordering dependency and
        // a caller is waiting on the reply).
        if (aborted_ && !std::holds_alternative<QueryMatchResultCommand>(message->command)) {
            return;
        }
        current_reply_ = message->reply;
        std::visit([this](const auto& command) { Execute(command); }, message->command);
        current_reply_ = nullptr;
    }

    // Sends an ack to a waiting runtime caller (no-op for fire-and-forget
    // startup-file commands, where current_reply_ is null). The `code` mirrors
    // HTTP-style status so the caller can sequence deterministically.
    void AckReply(const char* type, int code, const std::string& game_id) {
        if (!current_reply_) {
            return;
        }
        nlohmann::json response;
        response["type"] = type;
        response["code"] = code;
        if (!game_id.empty()) {
            response["game_id"] = game_id;
        }
        current_reply_(response);
    }

    void Execute(const ConfigureProviderCommand& command) {
        if (callbacks_.configure_provider) {
            callbacks_.configure_provider(command.provider);
        }
        AckReply("configure_provider_response", callbacks_.configure_provider ? 200 : 501, {});
    }

    void Execute(const ConfigureGameCommand& command) {
        if (!callbacks_.create_game) {
            AckReply("configure_game_response", 501, command.game_id);
            return;
        }
        const auto context = callbacks_.create_game(command.configuration, command.game_id);
        WaitForPhase(context, chess::game::GameLifecyclePhase::Ready, "configure_game", command.game_id);
        // aborted_ is set by WaitForPhase on timeout / missing observable game.
        AckReply("configure_game_response", aborted_ ? 500 : 200, command.game_id);
    }

    void Execute(const StartGameCommand& command) {
        if (!callbacks_.find_game) {
            AckReply("start_game_response", 501, command.game_id);
            return;
        }
        const auto context = callbacks_.find_game(command.game_id);
        if (!context) {
            std::cerr << "[ExternalCommandExecutor] start_game: unknown game id '" << command.game_id << "'\n";
            AckReply("start_game_response", 404, command.game_id);
            return;
        }
        so_5::send<chess::game::execution::StartGame>(context->command_mbox);
        WaitForPhase(context, chess::game::GameLifecyclePhase::InProgress, "start_game", command.game_id);
        AckReply("start_game_response", aborted_ ? 500 : 200, command.game_id);
    }

    void Execute(const QueryMatchResultCommand& command) {
        std::shared_ptr<chess::game::GameContext> context;
        if (callbacks_.find_game) {
            context = callbacks_.find_game(command.game_id);
        }
        const nlohmann::json response = BuildMatchResultResponse(command.game_id, context);
        if (current_reply_) {
            current_reply_(response);
        }
    }

    void Execute(const OpenSpectatorViewCommand& command) {
        if (!callbacks_.open_spectator_view) {
            std::cerr << "[ExternalCommandExecutor] open_spectator_view: no callback registered\n";
            return;
        }

        // The actual window creation happens on the UI thread. We use a
        // promise/future to block this executor thread until the UI thread
        // completes the operation and returns the response.
        std::promise<OpenSpectatorViewResponse> promise;
        std::future<OpenSpectatorViewResponse> future = promise.get_future();

        callbacks_.open_spectator_view(command, [&promise](OpenSpectatorViewResponse response) {
            promise.set_value(std::move(response));
        });

        // Wait for the UI thread to process the request (bounded by command timeout).
        const auto status = future.wait_for(kCommandTimeout);
        if (status == std::future_status::timeout) {
            std::cerr << "[ExternalCommandExecutor] open_spectator_view timed out for game '"
                      << command.game_id << "'\n";
            return;
        }

        const OpenSpectatorViewResponse response = future.get();
        if (current_reply_) {
            nlohmann::json json_response;
            to_json(json_response, response);
            current_reply_(json_response);
        }
    }

    void Execute(const SetMoveHistoryBarCommand& command) {
        SimpleCodeResponse response;
        if (callbacks_.set_move_history_bar) {
            const bool found = callbacks_.set_move_history_bar(command.window_id, command.enable_move_history_bar);
            response.code = found ? 200 : 404;
        } else {
            std::cerr << "[ExternalCommandExecutor] set_move_history_bar: no callback registered\n";
            response.code = 501;
        }
        if (current_reply_) {
            nlohmann::json json_response;
            to_json(json_response, response);
            current_reply_(json_response);
        }
    }

    // Builds the JSON match-result response. In the intended "watch then query"
    // usage the game is already concluded, so the bounded wait returns at once.
    // A query never aborts the run (unlike the lifecycle WaitForPhase).
    [[nodiscard]] nlohmann::json BuildMatchResultResponse(
        const std::string& game_id,
        const std::shared_ptr<chess::game::GameContext>& context
    ) {
        nlohmann::json response;
        response["type"] = "query_match_result_response";
        response["game_id"] = game_id;

        if (!context) {
            response["error"] = "unknown_game";
            response["outcome"] = "unknown";
            response["winner"] = nullptr;
            response["cause"] = "";
            return response;
        }

        if (WaitForConcluded(context) && context->result) {
            const chess::game::execution::MatchResult& result = *context->result;
            response["outcome"] = std::string(chess::game::execution::OutcomeToString(result.outcome));
            response["winner"] = chess::game::execution::WinnerToJson(result.outcome);
            response["cause"] = result.cause;
        } else {
            response["outcome"] = "in_progress";
            response["winner"] = nullptr;
            response["cause"] = "";
        }

        if (context->board && context->board_mutex) {
            const std::lock_guard<std::mutex> lock(*context->board_mutex);
            response["final_fen"] = context->board->ToFEN();
        }
        return response;
    }

    // Bounded poll for conclusion; returns false on timeout WITHOUT aborting the
    // run, so a query issued mid-game can't kill subsequent commands.
    [[nodiscard]] bool WaitForConcluded(const std::shared_ptr<chess::game::GameContext>& context) {
        if (!context || !context->phase) {
            return false;
        }
        const auto deadline = std::chrono::steady_clock::now() + kCommandTimeout;
        while (context->phase->load() < chess::game::GameLifecyclePhase::Concluded) {
            if (std::chrono::steady_clock::now() > deadline) {
                return false;
            }
            std::this_thread::sleep_for(kPollInterval);
        }
        return true;
    }

    // Blocks this (dedicated) thread until the game reaches at least the target
    // phase, or aborts the run on timeout.
    void WaitForPhase(
        const std::shared_ptr<chess::game::GameContext>& context,
        const chess::game::GameLifecyclePhase target_phase,
        const char* command_name,
        const std::string& game_id
    ) {
        if (!context || !context->phase) {
            std::cerr << "[ExternalCommandExecutor] " << command_name
                      << ": no observable game '" << game_id << "'; aborting.\n";
            aborted_ = true;
            return;
        }
        const auto deadline = std::chrono::steady_clock::now() + kCommandTimeout;
        while (context->phase->load() < target_phase) {
            if (std::chrono::steady_clock::now() > deadline) {
                std::cerr << "[ExternalCommandExecutor] " << command_name
                          << " timed out waiting for game '" << game_id << "'; aborting remaining commands.\n";
                aborted_ = true;
                return;
            }
            std::this_thread::sleep_for(kPollInterval);
        }
    }

    CommandTargetCallbacks callbacks_;
    bool aborted_{false};
    // Reply sink for the command currently being executed (null for
    // fire-and-forget commands). Set per-message in OnExecuteCommand.
    std::function<void(const nlohmann::json&)> current_reply_{};
};

// Introduces the executor on its own dedicated thread; returns its mbox so the
// caller can post ExecuteCommand messages (processed FIFO).
inline so_5::mbox_t IntroduceExternalCommandExecutor(
    so_5::environment_t& env,
    CommandTargetCallbacks callbacks
) {
    so_5::mbox_t mbox;
    env.introduce_coop([&](so_5::coop_t& coop) {
        auto dispatcher = so_5::disp::one_thread::make_dispatcher(env);
        auto* const executor = coop.make_agent_with_binder<ExternalCommandExecutor>(
            dispatcher.binder(), std::move(callbacks)
        );
        mbox = executor->so_direct_mbox();
    });
    return mbox;
}

} // namespace chess::application::command
