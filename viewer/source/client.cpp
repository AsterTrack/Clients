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

//#define LOG_MAX_LEVEL LTrace

#include "client.hpp"

#include "ui/shared.hpp" // Signals

#include "io/vrpn.hpp" // Outputting data over VRPN

#include "util/log.hpp"
#include "util/eigenutil.hpp"
#include "util/util.hpp" // printBuffer, TimePoint_t

#include <chrono>

#include "nlohmann/json.hpp"
using json = nlohmann::json;

#include <fstream>

/**
 * Main program flow
 */


/* Functions */

static void ReceivingThread(std::stop_token stop_token, ClientState *state);
static void parseConfigFile(std::string path, Config *config);

bool ClientInit(ClientState &state)
{
	parseConfigFile("config/config.json", &state.config);

	state.protocolThread = new std::jthread(ReceivingThread, &state);

	SetupIO(state);

	return true;
}

void ClientExit(ClientState &state)
{
	// Join coprocessing thread
	delete state.protocolThread;
	state.protocolThread = NULL;

	ResetIO(state);
}

static void ReceivingThread(std::stop_token stop_token, ClientState *state)
{
	int it = 0;
	
	while (!stop_token.stop_requested())
	{
		it++;

		{
			auto io_lock = std::unique_lock(state->io.mutex);
			for (auto &tracker : state->io.vrpn_trackers)
			{
				if (tracker.remote)
					tracker.remote->mainloop();
			}
			state->io.vrpn_local->mainloop();
		}

		std::this_thread::sleep_for(std::chrono::microseconds(500));
	}
}

static void parseConfigFile(std::string path, Config *config)
{
	// Read JSON config file
	std::ifstream fs(path);
	if (!fs.is_open()) return;
	json cfg;
	fs >> cfg;

	if (cfg.contains("vrpn_trackers") && cfg["vrpn_trackers"].is_array())
	{
		auto &vrpn_trackers = cfg["vrpn_trackers"];
		for (auto &vrpn_tracker : vrpn_trackers)
			config->vrpn_trackers.push_back(vrpn_tracker.get<std::string>());
	}

	// TODO: Configure other integrations here as well
}


// ----------------------------------------------------------------------------
// Tracking Data IO
// ----------------------------------------------------------------------------

void SetupIO(ClientState &state)
{
	auto io_lock = std::unique_lock(state.io.mutex);

	// Try connecting to a local VRPN server, just to tell if one is there
	std::string connectionName = "localhost:" + std::to_string(vrpn_DEFAULT_LISTEN_PORT_NO);
	state.io.vrpn_local = opaque_ptr<vrpn_Connection>(vrpn_get_connection_by_name(connectionName.c_str()));

	// Load all configured VRPN trackers
	for (auto &trkPath : state.config.vrpn_trackers)
	{
		// Add to list first so pointers stay intact
		state.io.vrpn_trackers.emplace_back(trkPath);
		state.io.vrpn_trackers.back().connect();
	}

	// TODO: Setup other integrations here as well
}

void ResetIO(ClientState &state)
{
	auto io_lock = std::unique_lock(state.io.mutex);
	state.io.vrpn_trackers.clear();
	state.io.vrpn_local = nullptr;

	// TODO: Clean up other integrations here as well
}