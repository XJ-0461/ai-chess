#pragma once

#include <imgui.h>
#include <functional>
#include <string>

template <typename T>
concept IsRenderableT = // T.Render() exists

template <IsRenderableT LeftSidebar, IsRenderableT MainContent, IsRenderableT RightSidebar>
struct Layout {

    // template <IsRenderableT Content>
    // struct Panel {
    //     float WidthRatio = 0.0f;
    //     Content panel_content;
    // };

    std::string Id = "MainThreeColumnLayout";

    LeftSidebar left_sidebar;
    MainContent main_content;
    RightSidebar right_sidebar;

    template <IsRenderableT LeftSidebarType, IsRenderableT MainContentType, IsRenderableT RightSidebarType>
    Layout(LeftSidebarType lhs, MainContentType main_content, RightSidebarType rhs)
        : left_sidebar(lhs), main_content(main_content), right_sidebar(rhs) {}

    // Pass 'const' because rendering shouldn't mutate the layout configuration
    void Render() const {
        // Table flags: No borders, stretch to fit available width, don't extend past the bottom
        ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendY;

        if (ImGui::BeginTable(Id.c_str(), 3, tableFlags, ImGui::GetContentRegionAvail())) {

            // Register columns with their assigned percentage weights
            ImGui::TableSetupColumn("LeftCol",  ImGuiTableColumnFlags_WidthStretch, LeftPanel.WidthRatio);
            ImGui::TableSetupColumn("MainCol",  ImGuiTableColumnFlags_WidthStretch, MainPanel.WidthRatio);
            ImGui::TableSetupColumn("RightCol", ImGuiTableColumnFlags_WidthStretch, RightPanel.WidthRatio);

            // --- Left Panel ---
            ImGui::TableNextColumn();
            if (ImGui::BeginChild("LeftChild", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None)) {
                if (LeftPanel.RenderContent)
                    LeftPanel.RenderContent();
                ImGui::EndChild();
            }

            // --- Main Panel ---
            ImGui::TableNextColumn();
            if (ImGui::BeginChild("MainChild", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None)) {
                if (MainPanel.RenderContent)
                    MainPanel.RenderContent();
                ImGui::EndChild();
            }

            // --- Right Panel ---
            ImGui::TableNextColumn();
            if (ImGui::BeginChild("RightChild", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None)) {
                if (RightPanel.RenderContent)
                    RightPanel.RenderContent();
                ImGui::EndChild();
            }

            ImGui::EndTable();
        }
    }
};
