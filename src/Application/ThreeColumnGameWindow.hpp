#pragma once

#include <concepts>
#include <memory>
#include <string>
#include <utility>

#include <imgui.h>

#include "Application/Command/Command.hpp"

template <typename T>
concept IsRenderable = requires(T a) {
    { a.Render() } -> std::same_as<void>;
};

template <typename T>
using observer_ptr = T*;

// Convert ImGui flag name strings to ImGuiWindowFlags bitmask.
inline ImGuiWindowFlags ParseImGuiWindowFlags(const std::vector<std::string>& flag_names) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    for (const auto& name : flag_names) {
        if (name == "ImGuiWindowFlags_NoTitleBar") flags |= ImGuiWindowFlags_NoTitleBar;
        else if (name == "ImGuiWindowFlags_NoResize") flags |= ImGuiWindowFlags_NoResize;
        else if (name == "ImGuiWindowFlags_NoMove") flags |= ImGuiWindowFlags_NoMove;
        else if (name == "ImGuiWindowFlags_NoScrollbar") flags |= ImGuiWindowFlags_NoScrollbar;
        else if (name == "ImGuiWindowFlags_NoScrollWithMouse") flags |= ImGuiWindowFlags_NoScrollWithMouse;
        else if (name == "ImGuiWindowFlags_NoCollapse") flags |= ImGuiWindowFlags_NoCollapse;
        else if (name == "ImGuiWindowFlags_NoBackground") flags |= ImGuiWindowFlags_NoBackground;
        else if (name == "ImGuiWindowFlags_NoSavedSettings") flags |= ImGuiWindowFlags_NoSavedSettings;
        else if (name == "ImGuiWindowFlags_NoBringToFrontOnFocus") flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
        else if (name == "ImGuiWindowFlags_NoDocking") flags |= ImGuiWindowFlags_NoDocking;
        else if (name == "ImGuiWindowFlags_NoDecoration") flags |= ImGuiWindowFlags_NoDecoration;
    }
    return flags;
}

// A standalone, titled, movable, closable window laying out three renderable
// components across stretched columns. Each instance is its own ImGui window
// (unique title), so it can be dragged out into its own OS viewport - allowing
// individual window recording.
template <IsRenderable LeftSidebar, IsRenderable MainContent, IsRenderable RightSidebar>
struct ThreeColumnGameWindow {
    std::string window_title{};
    observer_ptr<LeftSidebar> left_sidebar{nullptr};
    observer_ptr<MainContent> main_content{nullptr};
    observer_ptr<RightSidebar> right_sidebar{nullptr};

    // Initial window configuration (size, position, type). Applied on first render
    // and then cleared. If null, default docking behavior is used.
    std::shared_ptr<chess::application::command::WindowConfiguration> initial_window_config{nullptr};
    bool initial_config_applied{false};
    ImGuiWindowFlags extra_window_flags{ImGuiWindowFlags_None};

    ThreeColumnGameWindow() = default;

    ThreeColumnGameWindow(
        std::string title,
        observer_ptr<LeftSidebar> lhs,
        observer_ptr<MainContent> main,
        observer_ptr<RightSidebar> rhs
    ) : window_title(std::move(title)),
        left_sidebar(lhs),
        main_content(main),
        right_sidebar(rhs) {}

    void SetInitialWindowConfig(std::shared_ptr<chess::application::command::WindowConfiguration> config) {
        if (config) {
            extra_window_flags = ParseImGuiWindowFlags(config->imgui_flags);
        }
        initial_window_config = std::move(config);
        initial_config_applied = false;
    }

    // Renders the window. When `open` is non-null it is wired to the window's
    // close button and set to false once the user closes the window.
    void Render(bool* open) {
        if (open != nullptr && !*open) {
            return;
        }

        // Apply initial window configuration if provided (overrides saved state).
        if (initial_window_config && !initial_config_applied) {
            const auto& config = *initial_window_config;

            // Set size if specified (non-zero dimensions).
            if (config.size.width > 0 && config.size.height > 0) {
                ImGui::SetNextWindowSize(
                    ImVec2(static_cast<float>(config.size.width),
                           static_cast<float>(config.size.height)),
                    ImGuiCond_Always);
            }

            // Set position if type indicates positioned window.
            if (config.type == "positioned" || config.type == "floating") {
                ImGui::SetNextWindowPos(
                    ImVec2(static_cast<float>(config.position.x),
                           static_cast<float>(config.position.y)),
                    ImGuiCond_Always);
            }

            // For floating windows, explicitly undock by setting dock ID to 0.
            if (config.type == "floating") {
                ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
            } else if (config.type == "docked") {
                // Dock into the main viewport.
                ImGui::SetNextWindowDockID(ImGui::GetMainViewport()->ID, ImGuiCond_Always);
            }

            initial_config_applied = true;
        } else if (!initial_window_config) {
            // No explicit configuration: dock into the main viewport by default
            // on first use (respects saved INI state thereafter).
            ImGui::SetNextWindowDockID(ImGui::GetMainViewport()->ID, ImGuiCond_FirstUseEver);
        }

        if (ImGui::Begin(window_title.c_str(), open, extra_window_flags)) {
            constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_SizingStretchSame |
                                                    ImGuiTableFlags_NoBordersInBody;

            if (ImGui::BeginTable("##three_columns", 3, table_flags, ImGui::GetContentRegionAvail())) {
                ImGui::TableSetupColumn("LeftCol",  ImGuiTableColumnFlags_WidthStretch, 0.25f);
                ImGui::TableSetupColumn("MainCol",  ImGuiTableColumnFlags_WidthStretch, 0.50f);
                ImGui::TableSetupColumn("RightCol", ImGuiTableColumnFlags_WidthStretch, 0.25f);

                ImGui::TableNextColumn();
                if (left_sidebar != nullptr) {
                    left_sidebar->Render();
                }

                ImGui::TableNextColumn();
                if (main_content != nullptr) {
                    main_content->Render();
                }

                ImGui::TableNextColumn();
                if (right_sidebar != nullptr) {
                    right_sidebar->Render();
                }

                ImGui::EndTable();
            }
        }
        ImGui::End();
    }
};
