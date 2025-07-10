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
#include "ui/imgui/imgui_custom.hpp"

void InterfaceState::UpdateProtocols(InterfaceWindow &window)
{
	if (!ImGui::Begin(window.title.c_str(), &window.open))
	{
		ImGui::End();
		return;
	}
	ClientState &state = GetState();

	{ // VRPN UI

		auto io_lock = std::unique_lock(state.io.mutex);

		BeginSection("Local VRPN server");
		if (!state.io.vrpn_local->connected())
		{
			ImGui::TextUnformatted("Not connected to a local server.");
		}
		else if (!state.io.vrpn_local->doing_okay())
		{
			ImGui::TextUnformatted("Connection to local server broken!");
		}
		else
		{
			ImGui::TextUnformatted("Connected to local server!");
		}
		EndSection();

		BeginSection("Known Trackers (?)");
		ImGui::SetItemTooltip("Any trackers remote is exposing or local is attempting to connect.");

		auto localTrackers = vrpn_getKnownTrackers(state.io.vrpn_local.get());
		for (auto trackerPath : localTrackers)
		{
			ImGui::Text("  - %s", trackerPath.c_str());
			SameLineTrailing(ImGui::GetFrameHeight());
			if (ImGui::Button(asprintf_s("+##%s", trackerPath.c_str()).c_str(), ImVec2(ImGui::GetFrameHeight(), 0)))
			{
				bool found = false;
				for (auto &trk : state.io.vrpn_trackers)
				{
					if (trk.path.size() < trackerPath.size()) continue;
					if (trk.path.compare(0, trackerPath.size(), trackerPath) != 0) continue;
					if (trk.path.size() > trackerPath.size() && trk.path[trackerPath.size()] != '@') continue;
					found = true;
					break;
				}
				if (!found)
				{
					state.io.vrpn_trackers.emplace_back(trackerPath);
					state.io.vrpn_trackers.back().connect();
				}
			}
		}

		EndSection();

		BeginSection("Connected Trackers");
		int index = 0;
		for (auto trk = state.io.vrpn_trackers.begin(); trk != state.io.vrpn_trackers.end();)
		{
			ImGui::PushID(index);
			ImGui::PushID(trk->path.c_str());
			if (ImGui::Selectable("", selectedTarget == index, ImGuiSelectableFlags_SpanAvailWidth | ImGuiSelectableFlags_AllowOverlap,  ImVec2(0, ImGui::GetFrameHeight())))
				selectedTarget = index;
			ImGui::SameLine();
			ImGui::AlignTextToFramePadding();
			if (trk->editing)
			{
				trk->remote = nullptr;
				ImGui::SetNextItemWidth(LineWidthRemaining() - ImGui::GetFrameHeight()*2 - ImGui::GetStyle().ItemSpacing.x*2);
				ImGui::InputText("##path", &trk->path);
			}
			else
				ImGui::Text("%s", trk->path.c_str());

			if (trk->remote && !trk->editing)
			{
				const char *status;
				auto con = trk->remote->connectionPtr();
				bool delayed = trk->receivedPackets && dt(trk->lastPacket, sclock::now()) > 50;
				if (trk->receivedPackets && !delayed && con->connected())
					status = "Tracking";
				else if (trk->receivedPackets && !trk->isConnected())
					status = "Connection Lost";
				else if (trk->receivedPackets && !con->doing_okay())
					status = "Connection Broken"; // TODO: This might be redundant, isConnected should cover it all
				else if (trk->receivedPackets && delayed)
					status = "Tracking Lost";
				else if (trk->isConnected())
					status = "Connected";
				else
					status = "Searching";
				SameLineTrailing(ImGui::CalcTextSize(status).x + ImGui::GetFrameHeight()*2 + ImGui::GetStyle().ItemSpacing.x*2);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(status);
				ImGui::SameLine();
			}
			else
			{
				SameLineTrailing(ImGui::GetFrameHeight()*2 + ImGui::GetStyle().ItemSpacing.x);
			}
			if (ImGui::Button("E", ImVec2(ImGui::GetFrameHeight(), 0)))
			{
				trk->editing = !trk->editing;
				if (!trk->editing && !trk->remote)
					trk->connect();
			}
			ImGui::SameLine();
			bool remove = CrossButton("Del");
			ImGui::PopID();
			ImGui::PopID();
			if (remove)
			{
				trk = state.io.vrpn_trackers.erase(trk);
				if (selectedTarget == index) selectedTarget = -1;
				else if (selectedTarget > index) selectedTarget--;
				continue;
			}
			trk++;
			index++;
		}

		{
			static std::string path;
			ImGui::SetNextItemWidth(SizeWidthDiv3_2().x);
			ImGui::InputText("##TrkAdr", &path);
			ImGui::SameLine();
			if (ImGui::Button("Connect", SizeWidthDiv3()))
			{
				state.io.vrpn_trackers.emplace_back(std::move(path));
				state.io.vrpn_trackers.back().connect();
				path.clear();
			}
		}

		EndSection();
	}

	// TODO: Add other protocols UIs here

	ImGui::End();
}