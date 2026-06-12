#pragma once

#include <imgui.h>
#include <concepts>
#include <string>

template <typename T>
concept IsRenderable = requires(T a) {
    { a.Render() } -> std::same_as<void>;
};

template <typename T>
using observer_ptr = T*;

template <IsRenderable LeftSidebar, IsRenderable MainContent, IsRenderable RightSidebar>
struct Layout {
    std::string Id = "MainThreeColumnLayout";

    observer_ptr<LeftSidebar> left_sidebar;
    observer_ptr<MainContent> main_content;
    observer_ptr<RightSidebar> right_sidebar;

    Layout(observer_ptr<LeftSidebar> lhs, observer_ptr<MainContent> main, observer_ptr<RightSidebar> rhs)
        : left_sidebar(lhs), main_content(main), right_sidebar(rhs) {}

    void Render() const {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | 
                                      ImGuiWindowFlags_NoMove | 
                                      ImGuiWindowFlags_NoResize | 
                                      ImGuiWindowFlags_NoSavedSettings | 
                                      ImGuiWindowFlags_NoBringToFrontOnFocus |
                                      ImGuiWindowFlags_NoBackground;

        if (ImGui::Begin("##LayoutWindow", nullptr, windowFlags)) {
            ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchSame | 
                                         ImGuiTableFlags_NoHostExtendY |
                                         ImGuiTableFlags_NoBordersInBody;

            if (ImGui::BeginTable(Id.c_str(), 3, tableFlags, ImGui::GetContentRegionAvail())) {
                ImGui::TableSetupColumn("LeftCol",  ImGuiTableColumnFlags_WidthStretch, 0.25f);
                ImGui::TableSetupColumn("MainCol",  ImGuiTableColumnFlags_WidthStretch, 0.50f);
                ImGui::TableSetupColumn("RightCol", ImGuiTableColumnFlags_WidthStretch, 0.25f);

                ImGui::TableNextColumn();
                if (left_sidebar) {
                    left_sidebar->Render();
                }

                ImGui::TableNextColumn();
                if (main_content) {
                    main_content->Render();
                }

                ImGui::TableNextColumn();
                if (right_sidebar) {
                    right_sidebar->Render();
                }

                ImGui::EndTable();
            }
        }
        ImGui::End();
    }
};
