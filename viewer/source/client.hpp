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

#ifndef CLIENT_H
#define CLIENT_H

#include "io/vrpn.hpp"

#include <thread>
#include <mutex>
#include <list>

struct ClientState;

extern ClientState StateInstance;
static inline ClientState &GetState() { return StateInstance; }


struct Config
{
	std::vector<std::string> vrpn_trackers;
};

struct ClientState
{
	Config config = {};

	std::jthread *protocolThread = NULL;

	struct
	{
		std::mutex mutex;

		// VRPN IO
		opaque_ptr<vrpn_Connection> vrpn_local;
		std::list<vrpn_Tracker_Wrapper> vrpn_trackers;
	} io;
};

/* Functions */

bool ClientInit(ClientState &state);
void ClientExit(ClientState &state);

void SetupIO(ClientState &state);
void ResetIO(ClientState &state);

#endif // CLIENT_H