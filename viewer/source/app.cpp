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

#include "app.hpp"
#include "client.hpp"

#include "ui/shared.hpp" // Signals

#include "imgui/imgui.h" // IM_COL32

#include "util/util.hpp" // dtUS

#include <cstdio>

AppState AppInstance;
ClientState StateInstance = {};
static std::atomic<bool> quitApp = { false };
static std::atomic<bool> closedUI = { false };
static TimePoint_t lastUIUpdate;


/* Interface */

extern "C" {
	bool _InterfaceThread();
	void _SignalShouldClose();
	void _SignalLogUpdate();
}
InterfaceThread_t InterfaceThread = &_InterfaceThread;
SignalShouldClose_t SignalInterfaceShouldClose = &_SignalShouldClose;
SignalLogUpdate_t SignalLogUpdate = &_SignalLogUpdate;


/* Logging implementation */

LogLevel LogMaxLevelTable[LMaxCategory];
LogLevel LogFilterTable[LMaxCategory];
thread_local LogCategory CurrentLogCategory = LDefault;
thread_local LogLevel CurrentLogLevel = LDebug;
thread_local int CurrentLogContext = -1;

const char* LogCategoryIdentifiers[LMaxCategory];
const char* LogCategoryDescriptions[LMaxCategory];
const char* LogLevelIdentifiers[LMaxLevel];
uint32_t LogLevelHexColors[LMaxLevel];

static void initialise_logging_strings();

/* Main Server Loop */

int main (void)
{
	{ // Setup logging
		initialise_logging_strings();

		// Init runtime max log levels
		// NOTE: Compile-time LOG_MAX_LEVEL takes priority!
		for (int i = 0; i < LMaxCategory; i++)
			LogMaxLevelTable[i] = LOG_MAX_LEVEL_DEFAULT;
	}

	{ // Init AsterTrack server
		if (!ClientInit(StateInstance))
			return -1;
		atexit([](){
			ClientExit(StateInstance);
		});
	}

	LOGC(LInfo, "=======================\n");

	// TODO: Setup tray icon, etc.

	while (!quitApp.load())
	{
		// TODO: Wait for request to open interface

		// Start UI thread that will be running for the lifetime of the UI
		lastUIUpdate = sclock::now();
		std::thread uiThread(InterfaceThread);
		while (!closedUI.load() && dtUS(sclock::now(), lastUIUpdate) < 20000)
		{ // 2s
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		if (!closedUI.load())
		{ // -> UI Hanging, ensure it knows to close, and wait for it
			SignalInterfaceShouldClose();
		}
		uiThread.join();
		closedUI = false;
	}

	ClientExit(StateInstance);

	// If UI signaled to close the WHOLE app, just exit
	exit(0);
}

void AppState::SignalQuitApp()
{
	// Signal main thread to quit
	quitApp = true;
	SignalInterfaceShouldClose();
}

void AppState::SignalInterfaceClosed()
{
	// Signal main thread that UI was closed
	closedUI = true;
}


/* Logging implementation */

static void initialise_logging_strings()
{
	const char* defaultCategoryName = "!CAT";
	const char* defaultCategoryDesc = "!Missing Category Description";
	for (int i = 0; i < LMaxCategory; i++)
	{
		LogCategoryIdentifiers[i] = defaultCategoryName;
		LogCategoryDescriptions[i] = defaultCategoryDesc;
	}

	LogCategoryIdentifiers[LDefault] 				= "Std ";
	LogCategoryIdentifiers[LGUI] 					= "GUI ";
	LogCategoryIdentifiers[LIO] 					= "IO  ";

	LogCategoryDescriptions[LDefault] 				= "Default";
	LogCategoryDescriptions[LGUI] 					= "GUI";
	LogCategoryDescriptions[LIO] 					= "IO";

	LogLevelIdentifiers[LTrace]  = "TRACE";
	LogLevelIdentifiers[LDebug]  = "DEBUG";
	LogLevelIdentifiers[LDarn]   = "DWARN";
	LogLevelIdentifiers[LInfo]   = "INFO ";
	LogLevelIdentifiers[LWarn]   = "WARN ";
	LogLevelIdentifiers[LError]  = "ERROR";
	LogLevelIdentifiers[LOutput] = "OUT  ";

	LogLevelHexColors[LTrace]  = IM_COL32(0xAA, 0xAA, 0xAA, 0xAA);
	LogLevelHexColors[LDebug]  = IM_COL32(0xCC, 0xCC, 0xCC, 0xFF);
	LogLevelHexColors[LDarn]   = IM_COL32(0xBB, 0x66, 0x33, 0xFF);
	LogLevelHexColors[LInfo]   = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
	LogLevelHexColors[LWarn]   = IM_COL32(0xDD, 0x88, 0x44, 0xFF);
	LogLevelHexColors[LError]  = IM_COL32(0xEE, 0x44, 0x44, 0xFF);
	LogLevelHexColors[LOutput] = IM_COL32(0xBB, 0xBB, 0x44, 0xFF);
}

// TODO: switch to stb_sprintf, supposedly faster
#define FORMAT_TO_STRING(FORMAT, STRING) \
	va_list argp; \
	va_start(argp, FORMAT); \
	int size = std::vsnprintf(nullptr, 0, FORMAT, argp); \
	va_end(argp); \
	if (size <= 0) return -1; \
	STRING.resize(size); \
	va_start(argp, FORMAT); \
	std::vsnprintf(STRING.data(), size+1, FORMAT, argp); \
	va_end(argp); \

int PrintLog(LogCategory category, LogLevel level, int context, const char *format, ...)
{
	AppState::LogEntry entry = {};
	entry.category = category;
	entry.level = level;
	entry.context = context;
	FORMAT_TO_STRING(format, entry.log);

	GetApp().logEntries.push_back(std::move(entry));

	if (LogFilterTable[category] <= level)
		SignalLogUpdate();

	return size;
}