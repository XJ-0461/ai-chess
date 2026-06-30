#include "GauntTelemetry.hpp"

#include <random>
#include <sstream>
#include <iomanip>
#include <format>

#include <spdlog/spdlog.h>

namespace chess::telemetry {

std::string GenerateSessionId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8) << dis(gen) << "-"
        << std::setw(4) << (dis(gen) & 0xFFFF) << "-"
        << std::setw(4) << ((dis(gen) & 0x0FFF) | 0x4000) << "-"
        << std::setw(4) << ((dis(gen) & 0x3FFF) | 0x8000) << "-"
        << std::setw(8) << dis(gen) << std::setw(4) << (dis(gen) & 0xFFFF);
    return oss.str();
}

std::shared_ptr<GauntContextHandles> InitializeGauntTelemetry(
    const GauntTelemetryConfig& config,
    const std::string& session_id
) {
    auto handles = std::make_shared<GauntContextHandles>();

    // Build the sink manager with both potential sink types baked into its type,
    // then register only the sinks the configuration actually enables.
    handles->sink_manager = gaunt::core::SharedSinkManagerBuilder<ggcc::TypeErasedEventConverter>{}
        .WithFormatter<gaunt::core::XMLDebugFormatter>()
        .WithSink<std::shared_ptr<gaunt::core::XMLSink>>()
        .WithSink<std::shared_ptr<ChessColiseumConsoleSink>>()
        .BuildShared();

    handles->sink_manager->RegisterFormatter(gaunt::core::XMLDebugFormatter{});

    if (config.xml_output_file.has_value()) {
        // Create directories if they don't exist
        if (config.xml_output_file->has_parent_path()) {
            std::filesystem::create_directories(config.xml_output_file->parent_path());
        }

        gaunt::core::XMLSinkOptions xml_sink_options{
            .output_file_path = *config.xml_output_file,
            .create_file_if_not_exists = true
        };
        handles->sink_manager->RegisterSink(std::make_shared<gaunt::core::XMLSink>(xml_sink_options));
    }

    if (config.enable_console_logging) {
        auto console_sink = gaunt::core::MakeCustomSharedSink<gaunt::core::XMLDebugFormatResult>(
            [](std::monostate, const std::any&, const gaunt::core::XMLDebugFormatResult& formatted, const std::any&) {
                spdlog::info("[GAUNT] {}", formatted.as_string);
            }
        );
        handles->sink_manager->RegisterSink(console_sink);
    }

    // Initialize Root context
    handles->root_context = gaunt::core::GauntContext<ggcc::context::Root>{
        ggcc::context::Root{},
        gaunt::core::ContextPath("Root")
    };
    handles->root_context.AttachSinkManager(handles->sink_manager);
    handles->root_context.Accept(ggcc::event::RootBegin{});

    // Initialize Session context
    handles->session_context = handles->root_context.CreateChildContext<ggcc::context::Session>(
        ggcc::context::Session::Key{session_id},
        ggcc::event::SessionBegin{}
    );

    spdlog::info("[GAUNT] Telemetry initialized with session ID: {}", session_id);

    return handles;
}

void ShutdownGauntTelemetry(GauntContextHandles& handles) {
    spdlog::info("[GAUNT] Shutting down telemetry");

    if (handles.session_context.has_value()) {
        handles.session_context->Accept(ggcc::event::SessionEnd{});
        handles.session_context.reset();
    }

    handles.root_context.Accept(ggcc::event::RootEnd{});

    // Flush sinks
    if (handles.sink_manager) {
        handles.sink_manager->Flush();
    }
}

} // namespace chess::telemetry