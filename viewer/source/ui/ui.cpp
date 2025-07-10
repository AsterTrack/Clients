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
#include "ui/shared.hpp" // Signals

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#include "app.hpp"

#include "gl/visualisation.hpp" // initVisualisation/cleanVisualisation
#include "imgui/imgui_onDemand.hpp"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <filesystem>

struct wl_display;
struct wl_resource;
#include "GL/glew.h"

#define ALLOW_CUSTOM_HEADER 0


/*
 * Interface for the AsterTrack application
 */

InterfaceState *InterfaceInstance;

const long targetIntervalUS = 1000000/60; // glfwSwapBuffer will further limit it to display refresh rate


/* Function prototypes */

// GLFW Platform Window
static GLFWwindow* setupPlatformWindow(bool &useHeader);
static void closePlatformWindow(GLFWwindow *windowHandle);
static void RefreshGLFWWindow(GLFWwindow *window);
// ImGui Code
static bool setupImGuiTheme();
static void loadFont();


/**
 * Main loop of UI
 */


static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
	LOG(LGUI, LError, "Error: %s\n", description);
}

extern "C" {

bool _InterfaceThread()
{
	InterfaceState ui;
	if (!InterfaceInstance)
		return false;

	// Open platform window
	glfwSetErrorCallback(glfw_error_callback);
	bool customHeader = false;
	ui.glfwWindow = setupPlatformWindow(customHeader);
	glfwMakeContextCurrent(ui.glfwWindow);
	glfwSetWindowRefreshCallback(ui.glfwWindow, RefreshGLFWWindow);

	// Initialise UI resources (e.g. ImGui, Themes, Icons and OpenGL)
	if (!ui.Init())
	{
		glfwMakeContextCurrent(nullptr);
		GetApp().SignalInterfaceClosed();
		GetApp().SignalQuitApp();
		return false;
	}

	// Render loop, starting off with 3 full UI update iterations
	ui.requireUpdates = 3;
	while (!glfwWindowShouldClose(ui.glfwWindow) && !ui.setCloseInterface)
	{
		// Record render time
		auto now = sclock::now();
		ui.deltaTime = dt(ui.renderTime, now)/1000.0f;
		ui.renderTime = now;

		// Update/render UI as requested
		if (ui.requireUpdates)
		{ // ImGui UI needs updates
			ui.requireUpdates--;
			ui.requireRender = false;
			ui.UpdateUI();
			ui.RenderUI(true);
		}
		else if (ui.requireRender)
		{ // Render parts of the screen with OnDemand rendered items
			ui.requireRender = false;
			ui.RenderUI(false);
		}

		// Wait a minimum amount to limit maximum refresh rate
		long curIntervalUS = dtUS(ui.renderTime, sclock::now());
		if (curIntervalUS < targetIntervalUS)
			std::this_thread::sleep_for(std::chrono::microseconds(targetIntervalUS-curIntervalUS));

		// Wait for input events or update/render requests while updating general state
		glfwPollEvents();
		if (!ui.requireRender && ui.requireUpdates == 0
			&& ImGui::GetCurrentContext()->InputEventsQueue.empty())
		{ // This timeout greatly influences idle power consumption
			glfwWaitEventsTimeout(50.0f/1000.0f);
		}
		ui.requireUpdates = std::max(ui.requireUpdates, 1);
		if (!ImGui::GetCurrentContext()->InputEventsQueue.empty())
			ui.requireUpdates = std::max(ui.requireUpdates, 3);
	}

	glfwSetWindowRefreshCallback(ui.glfwWindow, nullptr);

	// Clean up UI resources
	ui.Exit();

	// If native decorations were used to close window, we haven't notified App yet to quit
	if (glfwWindowShouldClose(ui.glfwWindow))
		GetApp().SignalQuitApp();

	// Close platform window
	glfwMakeContextCurrent(nullptr);
	closePlatformWindow(ui.glfwWindow);
	GetApp().SignalInterfaceClosed();

	return true;
}

}

static void RefreshGLFWWindow(GLFWwindow *window)
{
	if (!InterfaceInstance) return;
	InterfaceState &ui = GetUI();
	if (!ui.init) return;

	// Request more renders after resizing is done, since we early out sometimes and might leave an invalid buffer 
	ui.RequestUpdates(3);

	// Check render time
	auto now = sclock::now();
	if (dtUS(ui.renderTime, now) < targetIntervalUS) return;
	ui.deltaTime = dt(ui.renderTime, now)/1000.0f;
	ui.renderTime = now;

	// Update/render UI
	ui.UpdateUI();
	ui.RenderUI(true);
}


/**
 * UI Logic
 */

void InterfaceState::UpdateUI()
{
	// Start new UI frame
	ImGui_ImplGlfw_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	// Always reset layout in first frame
	static bool firstIteration = true;
	if (firstIteration)
	{
		firstIteration = false;
		ResetWindowLayout();
	}

	UpdateMainMenuBar();

	// Dockspace for other windows to dock to, covering whole viewport
	ImGui::DockSpaceOverViewport(dockspaceID, ImGui::GetMainViewport());

	for (auto &window : windows)
	{
		if (window.open)
			(this->*window.updateWindow)(window);
	}

	// Render UI out to a DrawData list, for later GL rendering
	ImGui::Render();
}

void InterfaceState::RenderUI(bool fullUpdate)
{
	// Render 2D UI with callbacks at appropriate places for 3D GL
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	glfwMakeContextCurrent(glfwWindow);
	glfwSwapBuffers(glfwWindow);

	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

void InterfaceState::ResetWindowLayout()
{
	// Create root dockspace covering the whole main viewport (will be replaced if it already exists)
	ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodePos(dockspaceID, ImGui::GetMainViewport()->WorkPos);
	ImGui::DockBuilderSetNodeSize(dockspaceID, ImGui::GetMainViewport()->WorkSize);

	// Define layout panels
	ImGuiID mainPanelID, bottomPanelID, sidePanelID, auxPanelID, edgePanelID;
	ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Right, 0.4f, &sidePanelID, &mainPanelID);
	ImGui::DockBuilderSplitNode(sidePanelID, ImGuiDir_Right, 0.3f, &edgePanelID, &sidePanelID);
	ImGui::DockBuilderSplitNode(sidePanelID, ImGuiDir_Down, 0.3f, &auxPanelID, &sidePanelID);	
	ImGui::DockBuilderSplitNode(mainPanelID, ImGuiDir_Down, 0.5f, &bottomPanelID, &mainPanelID);

	// Associate all static windows with a panel by default
	ImGui::DockBuilderDockWindow(windows[WIN_3D_VIEW].title.c_str(), mainPanelID);
	ImGui::DockBuilderDockWindow(windows[WIN_LOGGING].title.c_str(), bottomPanelID);
	ImGui::DockBuilderDockWindow(windows[WIN_PROTOCOL].title.c_str(), auxPanelID);
	ImGui::DockBuilderDockWindow(windows[WIN_STYLE_EDITOR].title.c_str(), sidePanelID);
	ImGui::DockBuilderDockWindow(windows[WIN_IMGUI_DEMO].title.c_str(), mainPanelID);

	ImGui::DockBuilderFinish(dockspaceID);
}


/* Default ImGUI windows */

void InterfaceState::UpdateStyleUI(InterfaceWindow &window)
{
	if (ImGui::Begin(window.title.c_str(), &window.open))
		ImGui::ShowStyleEditor();
	ImGui::End();
}

void InterfaceState::UpdateImGuiDemoUI(InterfaceWindow &window)
{
	ImGui::ShowDemoWindow(&window.open);
}


/**
 * UI Setup
 */

bool InterfaceState::Init()
{
	// ImGui Init

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	dockspaceID = ImHashStr("MainDockSpace");

	{ // Configure ImGui
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;		// Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;			// Enable Docking
		io.ConfigWindowsMoveFromTitleBarOnly = true;
	}

	setupImGuiTheme();

	loadFont();

	if (!ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true))
		return false;
	if (!ImGui_ImplOpenGL3_Init())
		return false;

	// OpenGL configuration

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);
	glEnable(GL_SCISSOR_TEST);
	glEnable(GL_MULTISAMPLE);
	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
	glEnable(GL_POINT_SPRITE);

	// GL Visualisation Init
	initVisualisation();

	// Initialise all static UI windows
	windows[WIN_3D_VIEW] = InterfaceWindow("3D View", &InterfaceState::Update3DViewUI, true);
	windows[WIN_LOGGING] = InterfaceWindow("Logging", &InterfaceState::UpdateLogging, true);
	windows[WIN_PROTOCOL] = InterfaceWindow("Protocols", &InterfaceState::UpdateProtocols, true);

	// Shortcut to ImGui's built-in style editor
	windows[WIN_STYLE_EDITOR] = InterfaceWindow("Style Editor", &InterfaceState::UpdateStyleUI);
	// Useful tool to debug and research UI
	windows[WIN_IMGUI_DEMO] = InterfaceWindow("Dear ImGui Demo", &InterfaceState::UpdateImGuiDemoUI);

	// Initialise 3D View
	view3D.pitch = (float)PI/2.0f;
	view3D.heading = +(float)PI;
	view3D.distance = 1.0f;
	view3D.viewTransform.translation() = Eigen::Vector3f(0, -4, 1.8f);
	view3D.viewTransform.linear() = getRotationXYZ(Eigen::Vector3f(view3D.pitch, 0, view3D.heading));

	// Initialise log
	for (int i = 0; i < LMaxCategory; i++)
		LogFilterTable[i] = LDebug;

	init = true;

	return true;
}

void InterfaceState::Exit()
{
	init = false;

	// Cleanup GL
	cleanVisualisation();

	// Cleanup OnDemand drawing system
	CleanupOnDemand();

	// Exit ImGui
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void InterfaceState::RequestUpdates(int count)
{
	requireUpdates = std::max(requireUpdates, count);
	glfwPostEmptyEvent(); // Wake up UI thread
}

void InterfaceState::RequestRender()
{
	requireRender = true;
	glfwPostEmptyEvent(); // Wake up UI thread
}


/**
 * Signals for UI
 */

extern "C" {

void _SignalShouldClose()
{
	if (!InterfaceInstance || !ImGui::GetCurrentContext())
		return; // UI not initialised
	GetUI().setCloseInterface = true;
	glfwPostEmptyEvent(); // Wake up UI thread
}

void _SignalLogUpdate()
{
	if (!InterfaceInstance || !ImGui::GetCurrentContext())
		return; // UI not initialised
	if (!GetUI().logsStickToNew)
		return; // Don't need to update with new logs
	ImGuiWindow* imguiWindow = ImGui::FindWindowByName(GetUI().windows[WIN_LOGGING].title.c_str());
	if (!(imguiWindow && imguiWindow->Active && imguiWindow->DockTabIsVisible))
		return; // Log is not visible
	//GetUI().RequestUpdates(1);
	// Not interactive, no need to wake UI thread to render now, especially if this gets spammed a lot
	GetUI().requireUpdates = std::max(GetUI().requireUpdates, 1);
}

}


/**
 * GLFW Platform Window
 */

static GLFWwindow* setupPlatformWindow(bool &useHeader)
{

	/* Select platform-specific feature set */

#ifdef __unix__

#if ALLOW_CUSTOM_HEADER
	// Custom header works for both X11 and wayland with a customised glfw
	useHeader = true;
#endif

	// Use wayland if available and desired, X11 otherwise
	const char *backendHint = std::getenv("GDK_BACKEND");
	if (backendHint && strcasecmp(backendHint, "x11") == 0)
	{ // We're not gtk, but this is a common method, so why not
		glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
		if (!glfwInit())
			return nullptr;
	}
	else
	{ // Try to initialise with wayland, fall back to X11
		glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
		if (useHeader)
			glfwInitHint(GLFW_WAYLAND_LIBDECOR, false);
		if (!glfwInit())
		{ // No wayland
			glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
			if (!glfwInit())
				return nullptr;
		}
	}

	// OpenGL 3.0 works just fine, 3.3 drops some simpler functions
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#elif _WIN32

#if ALLOW_CUSTOM_HEADER
	// Moving window worked with a glfw patch, but automatic docking/fullscreening didn't
	// Sizing options are available with a further call though
	// Still, disable until it works better
	useHeader = false;
#endif

	if (!glfwInit())
		return nullptr;

	// Colors didn't work on 3.0 context
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#else

	#error "Platform currently not supported!"

#endif

	/* Setup GLFW window */

	{
		GLFWmonitor *monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode *mode = glfwGetVideoMode(monitor);
		glfwWindowHint(GLFW_RED_BITS, mode->redBits);
		glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
		glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
		glfwWindowHint(GLFW_DOUBLEBUFFER, true);
		glfwWindowHint(GLFW_SAMPLES, 4);
		//glfwWindowHint(GLFW_MAXIMIZED, true); // Keeps specified size, so problem of "what is fullscreen - taskbar" is not solved
	}

#if ALLOW_CUSTOM_HEADER
	if (useHeader)
	{
		glfwWindowHint(GLFW_DECORATED, false);
		glfwWindowHint(GLFW_RESIZABLE, true);
	}
#else
	useHeader = false;
#endif

	// Open main window
	GLFWwindow *glfwWindow = glfwCreateWindow(1280, 720, "AsterTrack Client", nullptr, nullptr);
	if (!glfwWindow)
	{
		glfwTerminate();
		return nullptr;
	}
	glfwMaximizeWindow(glfwWindow);

#if ALLOW_CUSTOM_HEADERS && defined(_WIN32)
	if (useHeader)
	{ // Enable resizing handles, should disable in fullscreen
		HWND hwnd = glfwGetWin32Window(glfwWindow);
		long style = GetWindowLongA(hwnd, GWL_STYLE);
		SetWindowLongA(hwnd, GWL_STYLE, style | WS_SIZEBOX);
	}
#endif

	// GLEW Init (temporarily grabbing window coontext for this thread)
	glfwMakeContextCurrent(glfwWindow);
	GLenum err = glewContextInit();
	glfwMakeContextCurrent(nullptr);
	if (GLEW_OK != err)
	{
		LOG(LGUI, LError, "GLEW Init Error %d\n", err);
		fprintf(stderr, "GLEW Init Error: '%s'\n", glewGetErrorString(err));
		glfwTerminate();
		return nullptr;
	}

	return glfwWindow;
}

static void closePlatformWindow(GLFWwindow *windowHandle)
{
	if (windowHandle)
		glfwDestroyWindow((GLFWwindow*)windowHandle);
	glfwTerminate();
}


/**
 * ImGui code
 */

static bool setupImGuiTheme()
{
	// Setup Dear ImGui style
	ImGuiStyle& style = ImGui::GetStyle();

	style.WindowPadding = ImVec2(7, 7);				// Padding within a window.
	style.WindowRounding = 0.0f;					// Radius of window corners rounding. Set to 0.0f to have rectangular windows. Large values tend to lead to variety of artifacts and are not recommended.
	style.WindowBorderSize = 1.0f;					// Thickness of border around windows. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
	//style.WindowMinSize;					// Minimum window size. This is a global setting. If you want to constrain individual windows, use SetNextWindowSizeConstraints().
	//style.WindowTitleAlign;				// Alignment for title bar text. Defaults to (0.0f,0.5f) for left-aligned,vertically centered.
	style.WindowMenuButtonPosition = ImGuiDir_None;	// Side of the collapsing/docking button in the title bar (None/Left/Right). Defaults to ImGuiDir_Left.
	style.ChildRounding = 8.0f;						// Radius of child window corners rounding. Set to 0.0f to have rectangular windows.
	style.ChildBorderSize = 1.0f;					// Thickness of border around child windows. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
	style.PopupRounding = 0.0f;						// Radius of popup window corners rounding. (Note that tooltip windows use WindowRounding)
	style.PopupBorderSize = 1.0f;					// Thickness of border around popup/tooltip windows. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
	style.FramePadding = ImVec2(8,4);				// Padding within a framed rectangle (used by most widgets).
	style.FrameRounding = 2.0f;						// Radius of frame corners rounding. Set to 0.0f to have rectangular frame (used by most widgets).
	style.FrameBorderSize = 0.0f;					// Thickness of border around frames. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
	style.ItemSpacing = ImVec2(8,6);				// Horizontal and vertical spacing between widgets/lines.
	style.ItemInnerSpacing = ImVec2(6,6);			// Horizontal and vertical spacing between within elements of a composed widget (e.g. a slider and its label).
	style.CellPadding = ImVec2(4,4);				// Padding within a table cell. CellPadding.y may be altered between different rows.
	style.TouchExtraPadding = ImVec2(0, 0);			// Expand reactive bounding box for touch-based system where touch position is not accurate enough. Unfortunately we don't sort widgets so priority on overlap will always be given to the first widget. So don't grow this too much!
	style.IndentSpacing = 20.0f;					// Horizontal indentation when e.g. entering a tree node. Generally == (FontSize + FramePadding.x*2).
	style.ColumnsMinSpacing = 8;					// Minimum horizontal spacing between two columns. Preferably > (FramePadding.x + 1).
	style.ScrollbarSize = 20.0f;					// Width of the vertical scrollbar, Height of the horizontal scrollbar.
	style.ScrollbarRounding = 10.0f;				// Radius of grab corners for scrollbar.
	style.GrabMinSize = 8.0f;						// Minimum width/height of a grab box for slider/scrollbar.
	style.GrabRounding = 4.0f;						// Radius of grabs corners rounding. Set to 0.0f to have rectangular slider grabs.
	//style.LogSliderDeadzone;				// The size in pixels of the dead-zone around zero on logarithmic sliders that cross zero.
	style.TabRounding = 4.0f;						// Radius of upper corners of a tab. Set to 0.0f to have rectangular tabs.
	style.TabBorderSize = 0.0f;						// Thickness of border around tabs.
	style.TabCloseButtonMinWidthSelected = 0.0f;			// Minimum width for close button to appear on an unselected tab when hovered. Set to 0.0f to always show when hovering, set to FLT_MAX to never show close button unless selected.
	style.TabCloseButtonMinWidthUnselected = 0.0f;	
	style.TabBarBorderSize = 1.0f;					// Thickness of tab-bar separator, which takes on the tab active color to denote focus.
	//style.ColorButtonPosition;			// Side of the color button in the ColorEdit4 widget (left/right). Defaults to ImGuiDir_Right.
	//style.ButtonTextAlign;				// Alignment of button text when button is larger than text. Defaults to (0.5f, 0.5f) (centered).
	//style.SelectableTextAlign;			// Alignment of selectable text. Defaults to (0.0f, 0.0f) (top-left aligned). It's generally important to keep this left-aligned if you want to lay multiple items on a same line.
	style.SeparatorTextBorderSize = 2.0f;			// Thickkness of border in SeparatorText()
	//style.SeparatorTextAlign;				// Alignment of text within the separator. Defaults to (0.0f, 0.5f) (left aligned, center).
	//style.SeparatorTextPadding;			// Horizontal offset of text from each edge of the separator + spacing on other axis. Generally small values. .y is recommended to be == FramePadding.y.
	//style.DisplayWindowPadding;			// Window position are clamped to be visible within the display area or monitors by at least this amount. Only applies to regular windows.
	style.DisplaySafeAreaPadding = ImVec2(0,0);		// If you cannot see the edges of your screen (e.g. on a TV) increase the safe area padding. Apply to popups/tooltips as well regular windows. NB: Prefer configuring your TV sets correctly!
	style.DockingSeparatorSize = 3.0f;				// Thickness of resizing border between docked windows
	//style.MouseCursorScale;				// Scale software rendered mouse cursor (when io.MouseDrawCursor is enabled). We apply per-monitor DPI scaling over this scale. May be removed later.
	style.AntiAliasedLines = true;					// Enable anti-aliased lines/borders. Disable if you are really tight on CPU/GPU. Latched at the beginning of the frame (copied to ImDrawList).
	style.AntiAliasedLinesUseTex = true;			// Enable anti-aliased lines/borders using textures where possible. Require backend to render with bilinear filtering (NOT point/nearest filtering). Latched at the beginning of the frame (copied to ImDrawList).
	style.AntiAliasedFill = true;					// Enable anti-aliased edges around filled shapes (rounded rectangles, circles, etc.). Disable if you are really tight on CPU/GPU. Latched at the beginning of the frame (copied to ImDrawList).
	//style.CurveTessellationTol;			// Tessellation tolerance when using PathBezierCurveTo() without a specific number of segments. Decrease for highly tessellated curves (higher quality, more polygons), increase to reduce quality.
	//style.CircleTessellationMaxError;		// Maximum error (in pixels) allowed when using AddCircle()/AddCircleFilled() or drawing rounded corner rectangles with no explicit segment count specified. Decrease for higher quality but more geometry.


	// Interesting colors
	//(11,36,36,255)   (0.043f, 0.141f, 0.141f, 1.000f)

	// Default dark colors
	style.Colors[ImGuiCol_Text]						= ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	style.Colors[ImGuiCol_TextDisabled]				= ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	style.Colors[ImGuiCol_WindowBg]					= ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_ChildBg]					= ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_PopupBg]					= ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_Border]					= ImVec4(0.25f, 0.25f, 0.25f, 0.50f);
	style.Colors[ImGuiCol_BorderShadow]				= ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_FrameBg]					= ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	style.Colors[ImGuiCol_FrameBgHovered]			= ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
	style.Colors[ImGuiCol_FrameBgActive]			= ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
	style.Colors[ImGuiCol_TitleBg]					= ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
	style.Colors[ImGuiCol_TitleBgActive]			= ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
	style.Colors[ImGuiCol_TitleBgCollapsed]			= ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	style.Colors[ImGuiCol_MenuBarBg]				= ImVec4(0.027f, 0.027f, 0.027f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarBg]				= ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	style.Colors[ImGuiCol_ScrollbarGrab]			= ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered]		= ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabActive]		= ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	style.Colors[ImGuiCol_CheckMark]				= ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
	style.Colors[ImGuiCol_SliderGrab]				= ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
	style.Colors[ImGuiCol_SliderGrabActive]			= ImVec4(0.08f, 0.50f, 0.72f, 1.00f);
	style.Colors[ImGuiCol_Button]					= ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	style.Colors[ImGuiCol_ButtonHovered]			= ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
	style.Colors[ImGuiCol_ButtonActive]				= ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
	style.Colors[ImGuiCol_Header]					= ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
	style.Colors[ImGuiCol_HeaderHovered]			= ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	style.Colors[ImGuiCol_HeaderActive]				= ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	style.Colors[ImGuiCol_Separator]				= style.Colors[ImGuiCol_Border];
	style.Colors[ImGuiCol_SeparatorHovered]			= ImVec4(0.41f, 0.42f, 0.44f, 1.00f);
	style.Colors[ImGuiCol_SeparatorActive]			= ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	style.Colors[ImGuiCol_ResizeGrip]				= ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_ResizeGripHovered]		= ImVec4(0.29f, 0.30f, 0.31f, 0.67f);
	style.Colors[ImGuiCol_ResizeGripActive]			= ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	style.Colors[ImGuiCol_Tab]						= ImVec4(0.08f, 0.08f, 0.09f, 0.83f);
	style.Colors[ImGuiCol_TabHovered]				= ImVec4(0.33f, 0.34f, 0.36f, 0.83f);
	style.Colors[ImGuiCol_TabSelected]				= ImVec4(0.23f, 0.23f, 0.24f, 1.00f);
	style.Colors[ImGuiCol_TabDimmed]				= ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
	style.Colors[ImGuiCol_TabDimmedSelected]		= ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_DockingPreview]			= ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
	style.Colors[ImGuiCol_DockingEmptyBg]			= ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	style.Colors[ImGuiCol_PlotLines]				= ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	style.Colors[ImGuiCol_PlotLinesHovered]			= ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogram]			= ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogramHovered]		= ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_TextSelectedBg]			= ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	style.Colors[ImGuiCol_DragDropTarget]			= ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
	style.Colors[ImGuiCol_NavCursor]				= ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_NavWindowingHighlight]	= ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	style.Colors[ImGuiCol_NavWindowingDimBg]		= ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	style.Colors[ImGuiCol_ModalWindowDimBg]			= ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

	//ImGui::StyleColorsLight();

	return true;
}

static void loadFont()
{
	ImFontConfig config;
	config.OversampleH = 2;
	config.OversampleV = 2;
	if (std::filesystem::exists("config/Karla-Regular.ttf"))
	{
		ImGui::GetIO().Fonts->Clear();
		ImGui::GetIO().Fonts->AddFontFromFileTTF("config/Karla-Regular.ttf", 17, &config);
	}
	else if (std::filesystem::exists("../config/Karla-Regular.ttf"))
	{
		printf("'config/Karla-Regular.ttf' not found in working directory but in parent directory! Make sure to run AsterTrack in the program root directory!\n");
		LOG(LDefault, LError, "'config/Karla-Regular.ttf' not found in working directory but in parent directory! Make sure to run AsterTrack in the program root directory!");
	}
	else
	{
		printf("'config/Karla-Regular.ttf' not found in working directory! Make sure to run AsterTrack in the program root directory!\n");
		LOG(LDefault, LError, "'config/Karla-Regular.ttf' not found in working directory! Make sure to run AsterTrack in the program root directory!");
	}
}
