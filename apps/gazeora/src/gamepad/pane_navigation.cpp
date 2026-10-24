#include "gamepad/pane_navigation.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace gazeora::app {

void PaneNavigation::MarkSidebar() { m_sidebar = ImGui::GetCurrentWindow(); }

void PaneNavigation::MarkPage() { m_page = ImGui::GetCurrentWindow(); }

void PaneNavigation::Apply() {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    if (!m_sidebar || !m_page) {
        return;
    }
    if (g.NavWindow && g.NavWindow->RootWindowForNav == m_page->ParentWindow) {
        // The window holding both has nothing of its own to focus: start in the page.
        Focus(m_page, false);
    }
    Target target = m_target;
    m_target = Target::None;
    if (target == Target::None) {
        return;
    }
    Focus(target == Target::Sidebar ? m_sidebar : m_page, target == Target::NewPage);
    ImGui::SetNavCursorVisibleAfterMove();
    m_entering = target == Target::NewPage;
}

void PaneNavigation::Follow() {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    m_entering = false;
    if (m_enterPage) {
        m_enterPage = false;
        m_target = Target::NewPage;
        return;
    }
    if (!m_sidebar || !m_page || !g.NavWindow || !ImGui::NavMoveRequestButNoResultYet()) {
        return;
    }
    ImGuiWindow* region = g.NavWindow->RootWindowForNav;
    if (g.NavMoveDir == ImGuiDir_Right && region == m_sidebar) {
        m_target = Target::Page;
    } else if (g.NavMoveDir == ImGuiDir_Left && region == m_page) {
        m_target = Target::Sidebar;
    }
}

bool PaneNavigation::FocusHereOnEntry() {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (!m_entering || g.NavWindow != m_page || window->RootWindowForNav != m_page) {
        return false;
    }
    m_entering = false;
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

void PaneNavigation::Focus(ImGuiWindow* window, bool fresh) {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGui::FocusWindow(window);
    // FocusWindow brings back the control focused there last; a new page has other controls.
    if (fresh || g.NavId == 0) {
        ImGui::NavInitWindow(window, true);
    }
}

}  // namespace gazeora::app
