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

#ifndef APP_H
#define APP_H

#include "util/log.hpp"
#include "util/blocked_vector.hpp"

class AppState;
extern AppState AppInstance;
static inline AppState &GetApp() { return AppInstance; }

/**
 * Main AsterTrack application managing lifetime of server and interface
 */
class AppState
{
public:
	// Logging
	struct LogEntry {
		std::string log;
		enum LogCategory category = LDefault;
		enum LogLevel level;
		int context = 0; // #Target, #Controller, #Camera, etc.
	};
	BlockedQueue<LogEntry, 1024*16> logEntries;

	void SignalQuitApp();
	void SignalInterfaceClosed();
};

#endif // APP_H