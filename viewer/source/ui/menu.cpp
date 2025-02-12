/**
AsterTrack Optical Tracking System
Copyright (C)  2025 Seneral <contact@seneral.dev> and contributors

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "ui.hpp"

#include "app.hpp"


void InterfaceState::UpdateMainMenuBar()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10,6));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10,10));

	if (!ImGui::BeginMainMenuBar())
	{
		ImGui::PopStyleVar(3);
		return;
	}

	ClientState &state = GetState();

	if (ImGui::BeginMenu("Client"))
	{
		if (ImGui::MenuItem("Quit"))
		{
			GetApp().SignalQuitApp();
		}
		ImGui::SetItemTooltip("Quit the Client");
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("View"))
	{
		if (ImGui::MenuItem("Reset Layout"))
			ResetWindowLayout();
		ImGui::Separator();

		auto addWindowMenuItem = [](InterfaceWindow &window)
		{
			if (!ImGui::MenuItem(window.title.c_str(), nullptr, &window.open))
				return;
			if (!window.open)
			{
				ImGuiWindow* imgui_window = ImGui::FindWindowByID(window.id);
				if (imgui_window)
					ImGui::DockContextProcessUndockWindow(ImGui::GetCurrentContext(), imgui_window, 0);
				ImGui::SetTabItemClosed(window.title.c_str());
			}
		};
		addWindowMenuItem(windows[WIN_3D_VIEW]);
		ImGui::Separator();
		addWindowMenuItem(windows[WIN_PROTOCOL]);
		ImGui::Separator();
		addWindowMenuItem(windows[WIN_LOGGING]);
		ImGui::Separator();
		if (ImGui::BeginMenu("Dear ImGUI"))
		{
			addWindowMenuItem(windows[WIN_STYLE_EDITOR]);
			addWindowMenuItem(windows[WIN_IMGUI_DEMO]);
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();

	ImGui::PopStyleVar(3);
}