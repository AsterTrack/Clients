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
#include "client.hpp"

#include "gl/visualisation.hpp"

#include "imgui/imgui_onDemand.hpp"
#include "imgui/imgui_custom.hpp"

struct wl_display;
struct wl_resource;
#include "GL/glew.h"

static void visualiseState3D(View3D &view3D, Eigen::Vector2i viewSize, float dT);

void InterfaceState::Update3DViewUI(InterfaceWindow &window)
{
	if (!ImGui::Begin(window.title.c_str(), &window.open, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		return;
	}
	ClientState &state = GetState();

	auto viewWin = ImGui::GetCurrentWindowRead();
	AddOnDemandRender(viewWin->InnerRect, [](const ImDrawList* dl, const ImDrawCmd* dc)
	{
		OnDemandItem &state = *static_cast<OnDemandItem*>(dc->UserCallbackData);
		ImVec2 size = SetOnDemandRenderArea(state, dc->ClipRect);
		glClearColor(0.2f, 0.0, 0.2f, 0.0);
		glClearDepth(0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Enable Depth for 3D scene
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_GEQUAL);
		glEnable(GL_DEPTH_TEST);

		// Update and render visualisations
		visualiseState3D(GetUI().view3D, Eigen::Vector2i(size.x, size.y), GetUI().deltaTime);
	}, nullptr);

	std::string viewLabel = "";
	ImVec2 toolbarPos = ImGui::GetCursorPos();
	ImVec2 viewOrigin = ImGui::GetCursorPos() - ImGui::GetStyle().WindowPadding;

	bool viewBGHovered, viewHeld;
	bool viewPressed = InteractionSurface("3DView", viewWin->InnerRect, viewBGHovered, viewHeld);
	bool viewFocused = ImGui::IsItemFocused();
	bool viewHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem);

	/**
	 * 3D View Key Input
	 */

	ImGuiIO &io = ImGui::GetIO();
	if (viewFocused)
	{ // Process input
		float dT = io.DeltaTime > 0.05f? 0.016f : io.DeltaTime;
		float dM = dT *10.0f;
		auto &transform = view3D.viewTransform;

		// Movement
		if (view3D.orbit)
		{
			if (ImGui::IsKeyDown(ImGuiKey_S))
			{
				RequestUpdates();
				view3D.distance += 1.0f*dT;
			}
			if (ImGui::IsKeyDown(ImGuiKey_W))
			{
				RequestUpdates();
				view3D.distance -= 1.0f*dT;
			}
		}
		else
		{
			if (ImGui::IsKeyDown(ImGuiKey_A))
			{
				RequestUpdates();
				transform.translation() += transform.rotation() * Eigen::Vector3f(-5.0f*dT, 0, 0);
			}
			if (ImGui::IsKeyDown(ImGuiKey_D))
			{
				RequestUpdates();
				transform.translation() += transform.rotation() * Eigen::Vector3f(+5.0f*dT, 0, 0);
			}
			if (ImGui::IsKeyDown(ImGuiKey_S))
			{
				RequestUpdates();
				transform.translation() += transform.rotation() * Eigen::Vector3f(0, 0, -5.0f*dT);
			}
			if (ImGui::IsKeyDown(ImGuiKey_W))
			{
				RequestUpdates();
				transform.translation() += transform.rotation() * Eigen::Vector3f(0, 0, +5.0f*dT);
			}
			if (ImGui::IsKeyDown(ImGuiKey_PageDown) || ImGui::IsKeyDown(ImGuiKey_Q))
			{
				RequestUpdates();
				transform.translation() += /*transform.rotation() **/ Eigen::Vector3f(0, 0, -3.0f*dT);
			}
			if (ImGui::IsKeyDown(ImGuiKey_PageUp) || ImGui::IsKeyDown(ImGuiKey_E))
			{
				RequestUpdates();
				transform.translation() += /*transform.rotation() **/ Eigen::Vector3f(0, 0, +3.0f*dT);
			}
		}
	}
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
	 || ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
	{
		if (ImGui::IsKeyPressed(ImGuiKey_N))
		{
			RequestUpdates();
			view3D.sidePanelOpen = !view3D.sidePanelOpen;
		}
	}

	/**
	 * 3D View Mouse Interaction
	 */

	if (viewHovered && std::abs(io.MouseWheel) > 0.0001f)
	{
		if (view3D.orbit)
		{
			view3D.distance = std::max(0.0f, view3D.distance/(1.0f+io.MouseWheel*0.05f) - io.MouseWheel*0.01f);
		}
	}

	if (viewPressed)
	{
		view3D.isDragging = true;
#ifdef VIEW_CAPTURE_MOUSE_CURSOR // Wayland/GLFW has a bug where it doesn't update the mouse pos in between captures when not moved 
		glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	#ifdef VIEW_RAW_MOUSE_MOVEMENT
		if (glfwRawMouseMotionSupported()) glfwSetInputMode(glfwWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	#endif
#endif
	}
	if (view3D.isDragging && !viewHeld)
	{
		view3D.isDragging = false;
#ifdef VIEW_CAPTURE_MOUSE_CURSOR // Wayland/GLFW has a bug where it doesn't update the mouse pos in between captures when not moved 
		glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	#ifdef VIEW_RAW_MOUSE_MOVEMENT
		if (glfwRawMouseMotionSupported()) glfwSetInputMode(glfwWindow, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
	#endif
#endif
	}
	if (view3D.isDragging)
	{
#ifndef VIEW_CAPTURE_MOUSE_CURSOR // Wayland/GLFW has a bug where it doesn't update the mouse pos in between captures when not moved 
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
#endif
		float mouseSensitivity = 0.005;
		view3D.pitch += mouseSensitivity*io.MouseDelta.y;
		view3D.pitch = std::clamp(view3D.pitch, 0.0f, (float)PI);
		view3D.heading += mouseSensitivity*io.MouseDelta.x;
		view3D.heading = std::fmod(view3D.heading, (float)PI*2);
		view3D.viewTransform.linear() = getRotationXYZ(Eigen::Vector3f(view3D.pitch, 0, view3D.heading));

		if (view3D.orbit)
		{
			if (selectedTarget >= 0 && selectedTarget < state.io.vrpn_trackers.size())
				view3D.target = std::next(state.io.vrpn_trackers.begin(), selectedTarget)->pose.translation();
			if (!view3D.target.hasNaN())
				view3D.viewTransform.translation() = view3D.target + view3D.viewTransform.linear() * Eigen::Vector3f(0, 0, -view3D.distance); 
		}
	}


	/**
	 * 3D View Interaction
	 */

	if (viewHovered)
	{ // Convert to projected space useful for 3D interactions
		ImVec2 mouseRel = (ImGui::GetMousePos() - viewWin->InnerRect.Min) / viewWin->InnerRect.GetSize();
		view3D.mousePos = Eigen::Vector2f(mouseRel.x*2-1, -(mouseRel.y*2-1));
	}
	else
		view3D.mousePos.setConstant(NAN);


	/**
	 * Overlay UI Layout
	 */

	static float sidePanelWidth = 200;
	float areaEnd = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x; // To use instead of GetRightAlignedStartPos
	ImRect area3D(viewOrigin, viewOrigin + viewWin->InnerRect.GetSize()), areaSide;
	if (view3D.sidePanelOpen)
	{
		areaSide = area3D;
		areaSide.Min.x = area3D.Max.x - (sidePanelWidth + ImGui::GetStyle().WindowPadding.x);
		areaSide.Max.x -= ImGui::GetStyle().WindowPadding.x;
		areaSide.Min.y += ImGui::GetStyle().WindowPadding.y;
		areaSide.Max.y = 0;
		assert(areaSide.GetWidth() == sidePanelWidth);

		areaEnd -= sidePanelWidth + ImGui::GetStyle().WindowPadding.x;
		area3D.Max.x -= sidePanelWidth + ImGui::GetStyle().WindowPadding.x;
	}


	/**
	 * 3D View Toolbar
	 */

	ImGui::SetCursorPos(toolbarPos);

	BeginViewToolbar();

	ImVec2 iconSize(ImGui::GetFontSize()*6/5, ImGui::GetFontSize());
	if (CircularButton("O", ImGui::GetFrameHeight()))
	{
		view3D.orbit = !view3D.orbit;
	}
	ImGui::SetItemTooltip("Toggle Orbit View");
	ImGui::SameLine();

	if (!viewLabel.empty())
	{
		ImGui::TextUnformatted(viewLabel.c_str());
		ImGui::SameLine();
	}

	ImGui::SetCursorPosX(areaEnd - GetBarWidth(ImGui::GetFrameHeight(), 1));
	ImGui::BeginDisabled(true);
	ImGui::Button("?", ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("Move around with WASD/Arrow Keys\nMove Up/Down with E/Q\nLook around with Left Mouse Drag");

	EndViewToolbar();


	/**
	 * Side Panel
	 */

	bool showWindow = false;
	if (view3D.sidePanelOpen)
	{
		ImGui::SetCursorPos(areaSide.Min);
		ImVec4 sideBG = ImGui::GetStyleColorVec4(ImGuiCol_ChildBg);
		sideBG.w = 0.3f;
		ImGui::PushStyleColor(ImGuiCol_ChildBg, sideBG);
		ImGui::SetNextWindowSizeConstraints(ImVec2(150, 100), ImVec2(FLT_MAX,FLT_MAX));
		showWindow = ImGui::BeginChild("Visualisation", ImVec2(0,0),
			ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX |
			ImGuiChildFlags_Borders | ImGuiChildFlags_TitleBar |
			ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar, &view3D.sidePanelOpen);
		ImGui::PopStyleColor();
		if (!showWindow)
			ImGui::EndChild();
	}
	if (showWindow)
	{ // Side Panel
		sidePanelWidth = ImGui::GetWindowWidth();

		ImGui::EndChild();
	}

	ImGui::End();
}

static void visualiseState3D(View3D &view3D, Eigen::Vector2i viewSize, float dT)
{
	float visAspect = (float)viewSize.y()/viewSize.x();
	ClientState &state = GetState();

	if (view3D.orbit)
	{
		if (GetUI().selectedTarget >= 0 && GetUI().selectedTarget < state.io.vrpn_trackers.size())
			view3D.target = std::next(state.io.vrpn_trackers.begin(), GetUI().selectedTarget)->pose.translation();
		if (!view3D.target.hasNaN())
			view3D.viewTransform.translation() = view3D.target + view3D.viewTransform.linear() * Eigen::Vector3f(0, 0, -view3D.distance); 
	}
	visSetupView(view3D.getProj(visAspect), view3D.viewTransform.inverse(), viewSize);

	static float time = 15.0f;
	time += dT/6;

	visualiseSkybox(time);
	visualiseFloor();

	for (auto &tracker : state.io.vrpn_trackers)
	{
		visualisePose(tracker.pose, { 0.8f, 0.2f, 0.2f, 0.8f }, 0.5f, 3.0f);
	}
}