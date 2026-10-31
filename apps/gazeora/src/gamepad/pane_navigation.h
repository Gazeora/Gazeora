// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

struct ImGuiWindow;

namespace gazeora::app {

// The controller moves as in Steam: the sidebar and the page are regions of their own, and
// left or right crosses between them where there's nothing further that way. Entering the
// sidebar lands on the open page's entry; entering the page goes back to where it was, or to
// its first control once the page has changed. A control can claim the focus on entry with
// FocusHereOnEntry.
class PaneNavigation {
public:
    // Right after BeginChild of each.
    void MarkSidebar();
    void MarkPage();
    // A controller opened a page: focus goes to its first control.
    void EnterPage() { m_request = Target::NewPage; }
    // Focus goes to the sidebar, on the open page's entry.
    void ReturnToSidebar() { m_request = Target::Sidebar; }
    // The page shows other controls now: entering it starts at the first.
    void PageChanged() { m_pageChanged = true; }
    bool InSidebar() const;
    // A focus move takes a few frames to land: keep drawing until it has.
    bool Settling() const;

    // Once a frame, before the sidebar and the page are drawn: moves focus as decided last
    // frame. ImGui only takes it before the controls it chooses from.
    void Apply();
    // Once a frame, after everything is drawn: sees where a move found nothing.
    void Follow();
    // Right after a control: while its region is being entered, focus goes to it rather than
    // the first one. True if it took it.
    bool FocusHereOnEntry();

private:
    // Settle: the window holding both had the focus, which has nothing of its own to focus.
    enum class Target { None, Sidebar, Page, NewPage, Settle };

    void Enter(Target target);
    // `fresh`: start at the first control rather than the one focused last.
    void Focus(ImGuiWindow* window, bool fresh);

    ImGuiWindow* m_sidebar = nullptr;
    ImGuiWindow* m_page = nullptr;
    Target m_request = Target::None;  // asked for this frame
    Target m_target = Target::None;   // to apply at the start of the next
    bool m_pageChanged = false;
    ImGuiWindow* m_entering = nullptr;  // this frame, the region being entered
};

}  // namespace gazeora::app
