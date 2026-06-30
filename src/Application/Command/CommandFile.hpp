#pragma once

#include <optional>
#include <string>

#include "Application/Command/Command.hpp"

namespace chess::application::command {

// Loads and parses a startup command file of the form
// { "commands": [ { "type": ..., "detail": { ... } }, ... ] }.
// On failure returns nullopt and writes a human-readable reason to `error`.
std::optional<CommandList> LoadCommandsFromFile(const std::string& path, std::string& error);

} // namespace chess::application::command
