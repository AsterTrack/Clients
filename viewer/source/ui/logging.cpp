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

void InterfaceState::UpdateLogging(InterfaceWindow &window)
{
	if (!ImGui::Begin(window.title.c_str(), &window.open))
	{
		ImGui::End();
		return;
	}

	static std::size_t selectedLog = -1, focusedLog = -1;
	GetApp().logEntries.delete_culled();
	auto logs = GetApp().logEntries.getView();

	if (ImGui::BeginChild("scrolling", ImGui::GetContentRegionAvail(), false, ImGuiWindowFlags_HorizontalScrollbar))
	{
		bool logDirty = false;
		int findItem = -1;

		{ // Update filtered logs
			auto pos = logs.begin();
			bool newFilter = logsFilterPos <= pos.index();
			if (newFilter)
			{ // Have not sorted yet or sorting got reset (e.g. logs culled, or manually reset)
				logsFiltered.clear();
			}
			else
			{ // Continue sorting new items from filterPos
				pos = logs.pos(std::max(logs.beginIndex(), logsFilterPos));
				assert(pos.valid());
			}

			while (pos < logs.end())
			{
				if (pos->level >= LogFilterTable[pos->category])
				{
					if (selectedLog == pos.index())
						findItem = logsFiltered.size();
					logsFiltered.push_back(pos.index());
					logDirty = true;
				}
				pos++;
			}
			logsFilterPos = pos.index();

			// If not visible anymore, clear selection to prevent unintended behaviour
			if (newFilter && findItem < 0)
				selectedLog = -1;

			// If we have to jump to a selected log, stop sticking to bottom
			if (findItem >= 0)
				logsStickToNew = false;
		}

		float preWidth1 = 50.0f;
		float preWidth2 = 60.0f;
		float startLevel = ImGui::GetCursorPosX() + preWidth1;
		float startLog = startLevel + preWidth2;
		bool focusVisible = false;

		{ // Has some glitching sometimes, scrollbar shows you can go down further, but it refuses, resulting in visual glitching
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			// Has no StyleVar ID
			ImVec2 extraPad = ImGui::GetStyle().TouchExtraPadding;
			ImGui::GetStyle().TouchExtraPadding = ImVec2(0,0);
			
			ImGuiListClipper clipper;
			clipper.Begin(logsFiltered.size());
			if (findItem >= 0) // Makes sure it's not clipped, so SetScrollHereY works
				clipper.IncludeItemByIndex(findItem);
			while (clipper.Step())
			{
				//for (auto entry = log.pos(startIndex+clipper.DisplayStart); entry.index() < clipper.DisplayEnd; entry++)
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
				{
					const auto &entry = logs[logsFiltered[i]];
					ImGui::PushID(logsFiltered[i]);

					bool selected = logsFiltered[i] == selectedLog;
					ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAvailWidth | ImGuiSelectableFlags_NoPadWithHalfSpacing;
					if (ImGui::Selectable("##SelectLog", selected, flags, ImVec2(0, ImGui::GetTextLineHeight())))
					{
						selected = !selected;
						selectedLog = selected? logsFiltered[i] : -1;
					}
					bool focused = logsFiltered[i] == focusedLog;
					if (ImGui::IsItemFocused())
					{
						focusVisible = true;
						if (!focused)
						{ // New keyboard focus, jump to it once
							focused = true;
							focusedLog = logsFiltered[i];
							logsStickToNew = i == logsFiltered.size()-1;
							if (logsStickToNew)
							{ // TODO: Somehow cancel focus on log entry to allow keepAtBottom to function
							}
						}
					}

					if (findItem == i)
					{ // Jump to previously selected item after re-sorting
						ImGui::SetScrollHereY();
						logsStickToNew = false;
						//assert(selectedLog == filteredLogs[i]);
					}

					ImGui::SameLine(0);
					ImGui::TextUnformatted(LogCategoryIdentifiers[entry.category], LogCategoryIdentifiers[entry.category]+4);
					ImGui::SetItemTooltip("%s", LogCategoryDescriptions[entry.category]);

					ImGui::PushStyleColor(ImGuiCol_Text, LogLevelHexColors[entry.level]);

					ImGui::SameLine(startLevel);
					ImGui::TextUnformatted(LogLevelIdentifiers[entry.level], LogLevelIdentifiers[entry.level]+5);

					ImGui::SameLine(startLog);
					ImGui::TextUnformatted(entry.log.c_str(), entry.log.c_str()+entry.log.size());

					ImGui::PopStyleColor();

					ImGui::PopID();
				}
			}
			clipper.End();

			ImGui::GetStyle().TouchExtraPadding = extraPad;
			ImGui::PopStyleVar();
		}

		if ((ImGui::GetIO().MouseWheel > 0.0f || findItem >= 0) && ImGui::GetScrollY() < ImGui::GetScrollMaxY())
			logsStickToNew = false;
		else if (ImGui::GetIO().MouseWheel < 0.0f && ImGui::GetScrollY() == ImGui::GetScrollMaxY())
			logsStickToNew = true;
		else if (logsStickToNew && logDirty)
			ImGui::SetScrollHereY(1.0f);
	}
	ImGui::EndChild();

	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		ImGui::OpenPopup("Context");
	if (ImGui::BeginPopup("Context"))
	{
		if (ImGui::Selectable("Clear"))
		{
			GetApp().logEntries.cull_all();
			// Could reset index as well with cull_clear, but not necessary
			logsFilterPos = 0; // New filtering
		}
		if (ImGui::MenuItem("Jump To Bottom", nullptr, &logsStickToNew))
		{
			if (logsStickToNew)
			{
				selectedLog = -1;
				ImGui::SetScrollY(ImGui::GetScrollMaxY());
				RequestUpdates();
			}
		}
		ImGui::EndPopup();
	}

	ImGui::End();
}

