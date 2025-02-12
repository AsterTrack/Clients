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
			// TODO: Query known trackers from VRPN server. Not implemented yet.
			auto localTrackers = vrpn_getKnownTrackers(state.io.vrpn_local.get());
			for (auto tracker : localTrackers)
			{
				ImGui::Text("%s", tracker.c_str());
			}
		}
		EndSection();

		BeginSection("Connected Trackers");
		int index = 0;
		for (auto trk = state.io.vrpn_trackers.begin(); trk != state.io.vrpn_trackers.end(); trk++, index++)
		{
			ImGui::PushID(index);
			ImGui::PushID(trk->path.c_str());
			if (ImGui::Selectable("", selectedTarget == index, ImGuiSelectableFlags_SpanAvailWidth | ImGuiSelectableFlags_AllowOverlap,  ImVec2(0, ImGui::GetFrameHeight())))
				selectedTarget = index;
			ImGui::SameLine();
			ImGui::AlignTextToFramePadding();
			ImGui::Text("%s", trk->path.c_str());

			if (trk->remote)
			{
				const char *status;
				auto con = trk->remote->connectionPtr();
				bool delayed = trk->receivedPackets && dt(trk->lastPacket, sclock::now()) > 50;
				if (trk->receivedPackets && !delayed && con->connected())
					status = "Connected";
				else if (trk->receivedPackets && !con->doing_okay())
					status = "Broken";
				else if (trk->receivedPackets && delayed)
					status = "Lost";
				else
					status = "Trying";
				SameLineTrailing(ImGui::CalcTextSize(status).x + ImGui::GetStyle().ItemSpacing.x + ImGui::GetFrameHeight());
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(status);
				ImGui::SameLine();
			}
			else
			{
				SameLineTrailing(ImGui::GetFrameHeight());
			}
			if (CrossButton("Del"))
			{
				trk = state.io.vrpn_trackers.erase(trk);
				if (selectedTarget == index) selectedTarget = -1;
				else if (selectedTarget > index) selectedTarget--;
				index--;
			}
			ImGui::PopID();
			ImGui::PopID();
		}

		io_lock.unlock();

		{
			static std::string path;
			ImGui::SetNextItemWidth(SizeWidthDiv3_2().x);
			ImGui::InputText("##TrkAdr", &path);
			ImGui::SameLine();
			if (ImGui::Button("Connect", SizeWidthDiv3()))
			{
				ConnectVRPNTracker(state, path);
				path.clear();
			}
		}

		EndSection();
	}

	// TODO: Add other protocols UIs here

	ImGui::End();
}