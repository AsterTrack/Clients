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

#ifndef UI_H
#define UI_H

/**
 * Shared between all ui compile units, not a header for external use
 */

#include "client.hpp"

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#endif
#include "imgui/imgui_internal.h"
#include "imgui/imgui_custom.hpp"
#include "imgui/misc/cpp/imgui_stdlib.h"

#include "gl/visualisation.hpp"

#include "util/eigenutil.hpp"
#include "util/util.hpp" // TimePoint_t
#include "util/blocked_vector.hpp"
#include "util/log.hpp"

// Forward-declared opaque structs
struct GLFWwindow; // GLFW/glfw3.h
// Defined later
class InterfaceState;

extern InterfaceState *InterfaceInstance;
static inline InterfaceState &GetUI() { return *InterfaceInstance; }

struct InterfaceWindow
{
	std::string title;
	ImGuiID id;
	bool open;
	void (InterfaceState::*updateWindow)(InterfaceWindow&);

	InterfaceWindow() {}
	InterfaceWindow(const char* Title, void (InterfaceState::*UpdateWindow)(InterfaceWindow&), bool Open = false)
		: title(Title), id(ImHashStr(Title)), updateWindow(UpdateWindow), open(Open) {}
};

enum InterfaceWindows
{
	WIN_3D_VIEW,
	WIN_LOGGING,
	WIN_PROTOCOL,
	WIN_STYLE_EDITOR,
	WIN_IMGUI_DEMO,
	INTERFACE_WINDOWS_MAX
};

struct View3D
{
	// General projection
	float fov = 65.0f;
	float pitch = 0, heading = 0;
	Eigen::Isometry3f viewTransform;

	// Orbit view
	bool orbit;
	float distance;
	Eigen::Vector3f target;

	// Interaction
	bool isDragging = false;
	bool sidePanelOpen = true;
	Eigen::Vector2f mousePos;

	inline Eigen::Projective3f getProj(float aspect) const
	{
		Eigen::Projective3f proj;
		const float zNear = 0.01f/2, zFar = 100; // 1cm-100m
		const float a = -(zFar+zNear)/(zFar-zNear), b = (2*zNear*zFar)/(zFar-zNear);
		float sY = 1.0f / fInvFromFoV(fov);
		float sX = sY*aspect;
		proj.matrix() <<
			sX, 0, 0, 0,
			0, sY, 0, 0,
			0, 0, a, b,
			0, 0, 1, 0;
		return proj;
	}
};

/**
 * Interface for the AsterTrack application
 */
class InterfaceState
{
public:
	bool init = false;

	// Window and Platform state
	GLFWwindow *glfwWindow = nullptr;
	bool setCloseInterface = false;

	// Render state
	TimePoint_t renderTime;
	float deltaTime;
	int requireUpdates = 3;
	bool requireRender;

	// Window state
	ImGuiID dockspaceID;
	std::array<InterfaceWindow, INTERFACE_WINDOWS_MAX> windows;

	// 3D View state
	View3D view3D = {};
	int selectedTarget = -1;

	// Log state
	BlockedVector<std::size_t> logsFiltered;
	std::size_t logsFilterPos = 0;
	bool logsStickToNew = true;

	InterfaceState() { InterfaceInstance = this; }
	~InterfaceState() { if (InterfaceInstance == this) InterfaceInstance = NULL; }

	bool Init();
	void Exit();

	void RequestUpdates(int count = 1);
	void RequestRender();
	void UpdateUI();
	void RenderUI(bool fullUpdate = true);
	void ResetWindowLayout();

	void UpdateMainMenuBar();

	void Update3DViewUI(InterfaceWindow &window);
	void UpdateProtocols(InterfaceWindow &window);
	void UpdateLogging(InterfaceWindow &window);

	void UpdateStyleUI(InterfaceWindow &window);
	void UpdateImGuiDemoUI(InterfaceWindow &window);
};

#endif // UI_H