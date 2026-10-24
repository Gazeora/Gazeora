#pragma once

struct ImGuiWindow;

namespace gazeora::app {

// The controller moves as in Steam: the sidebar and the page are regions of their own, and
// left or right crosses between them where there's nothing further that way. Focus comes back
// to where it was in a region, and a page just opened starts at its first control, or at the
// one that claims it with FocusHereOnEntry.
class PaneNavigation {
public:
    // Right after BeginChild of each.
    void MarkSidebar();
    void MarkPage();
    // The page changed by a controller: focus goes into it.
    void EnterPage() { m_enterPage = true; }
    // Once a frame, before the sidebar and the page are drawn: moves focus as decided last frame.
    // ImGui only takes it before the items it chooses from.
    void Apply();
    // Once a frame, after everything is drawn: sees where a move found nothing.
    void Follow();
    // Right after a control: while a page is being entered, focus goes to it rather than the
    // first one. True if it took it.
    bool FocusHereOnEntry();

private:
    enum class Target { None, Sidebar, Page, NewPage };

    // `fresh`: start at the first control rather than the one focused last.
    void Focus(ImGuiWindow* window, bool fresh);

    ImGuiWindow* m_sidebar = nullptr;
    ImGuiWindow* m_page = nullptr;
    bool m_enterPage = false;
    bool m_entering = false;  // this frame, a new page is being entered
    Target m_target = Target::None;
};

}  // namespace gazeora::app
