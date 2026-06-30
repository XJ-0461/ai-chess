#pragma once

#include <memory>
#include <string>
#include <optional>
#include <filesystem>

#include <tuple>

#include <gaunt/core/data/GauntContext.hpp>
#include <gaunt/core/data/ContextPath.hpp>
#include <gaunt/core/producer/ISink.hpp>
#include <gaunt/core/producer/XMLSink.hpp>
#include <gaunt/core/producer/CustomSink.hpp>
#include <gaunt/core/format/xml-debug/XMLDebugFormatter.hpp>
#include <gaunt/generated/chess_coliseum.gaunt.hpp>

namespace chess::telemetry {

namespace ggcc = gaunt::generated::chess_coliseum;

// The console sink renders the same XMLDebug formatted result that the XML file
// sink consumes, routing it through spdlog instead of to disk.
using ChessColiseumConsoleSink = gaunt::core::CustomSink<gaunt::core::XMLDebugFormatResult>;

// Concrete sink manager type used throughout the application. Both the XML and
// console sink types are baked into the manager type so it stays stable
// regardless of which sinks are actually registered at runtime.
using ChessColiseumSinkManager = gaunt::core::SharedSinkManagerImpl2<
    ggcc::TypeErasedEventConverter,
    std::tuple<gaunt::core::XMLDebugFormatter>,
    std::tuple<std::shared_ptr<gaunt::core::XMLSink>, std::shared_ptr<ChessColiseumConsoleSink>>
>;

// Context handles maintained at the Application level
struct GauntContextHandles {
    gaunt::core::GauntContext<ggcc::context::Root> root_context;
    std::optional<gaunt::core::GauntContext<ggcc::context::Session>> session_context;
    std::shared_ptr<ChessColiseumSinkManager> sink_manager;
};

// Configuration for Gaunt telemetry
struct GauntTelemetryConfig {
    std::optional<std::filesystem::path> xml_output_file;
    bool enable_console_logging = true;
};

// Factory function to initialize the telemetry system
std::shared_ptr<GauntContextHandles> InitializeGauntTelemetry(
    const GauntTelemetryConfig& config,
    const std::string& session_id
);

// Clean shutdown of the telemetry system
void ShutdownGauntTelemetry(GauntContextHandles& handles);

// Generate a UUID-style session identifier
std::string GenerateSessionId();

} // namespace chess::telemetry