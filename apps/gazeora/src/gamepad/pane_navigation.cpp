#include "gamepad/pane_navigation.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace gazeora::app {

void PaneNavigation::MarkSidebar() { m_sidebar = ImGui::GetCurrentWindow(); }

void PaneNavigation::MarkPage() { m_page = ImGui::GetCurrentWindow(); }

bool PaneNavigation::InSidebar() const {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    return m_sidebar && g.NavWindow && g.NavWindow->RootWindowForNav == m_sidebar;
}

bool PaneNavigation::Settling() const {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    return m_request != Target::None || m_target != Target::None || g.NavInitRequest ||
           g.NavInitResult.ID != 0 || g.NavMoveSubmitted;
}

void PaneNavigation::Apply() {
    m_entering = nullptr;
    Target target = m_target;
    m_target = Target::None;
    if (!m_sidebar || !m_page || target == Target::None) {
        return;
    }
    if (target == Target::Settle) {
        Focus(m_page, false);  // quietly: no highlight until the controller moves
        return;
    }
    Enter(target);
}

void PaneNavigation::Follow() {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    if (m_request != Target::None) {
        m_target = m_request;
        m_request = Target::None;
        return;
    }
    if (!m_sidebar || !m_page || !g.NavWindow) {
        return;
    }
    if (g.NavWindow->RootWindowForNav == m_page->ParentWindow) {
        m_target = Target::Settle;  // at the start, or after ImGui's own back out of a region
        return;
    }
    if (!ImGui::NavMoveRequestButNoResultYet()) {
        return;
    }
    ImGuiWindow* region = g.NavWindow->RootWindowForNav;
    if (g.NavMoveDir == ImGuiDir_Right && region == m_sidebar) {
        m_target = m_pageChanged ? Target::NewPage : Target::Page;
    } else if (g.NavMoveDir == ImGuiDir_Left && region == m_page) {
        m_target = Target::Sidebar;
    }
}

bool PaneNavigation::FocusHereOnEntry() {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (!m_entering || g.NavWindow != m_entering || window->RootWindowForNav != m_entering) {
        return false;
    }
    m_entering = nullptr;
    // Instead of the first control, which the request may have picked already.
    g.NavInitRequest = false;
    g.NavInitResult.ID = 0;
    ImGui::SetFocusID(ImGui::GetItemID(), window);
    ImGui::SetNavCursorVisibleAfterMove();
    if (!ImGui::IsItemVisible()) {
        ImGui::ScrollToRectEx(window, g.LastItemData.Rect);
    }
    return true;
}

void PaneNavigation::Enter(Target target) {
    // The sidebar always starts fresh, so the open page's entry can claim the focus.
    bool sidebar = target == Target::Sidebar;
    ImGuiWindow* window = sidebar ? m_sidebar : m_page;
    bool fresh = sidebar || target == Target::NewPage;
    Focus(window, fresh);
    ImGui::SetNavCursorVisibleAfterMove();
    m_entering = fresh ? window : nullptr;
    if (!sidebar) {
        m_pageChanged = false;
    }
}

void PaneNavigation::Focus(ImGuiWindow* window, bool fresh) {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGui::FocusWindow(window);
    // FocusWindow brings back the control focused there last; a new page has other controls.
    if (fresh || g.NavId == 0) {
        ImGui::NavInitWindow(window, true);
    }
}

}  // namespace gazeora::app
