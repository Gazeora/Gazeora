#include "keyboard/imgui_text_sink.h"

#include <imgui.h>

namespace gazeora::app {

void ImGuiTextSink::Type(char32_t character) {
    ImGui::GetIO().AddInputCharacter(static_cast<unsigned int>(character));
}

void ImGuiTextSink::Erase() { Tap(ImGuiKey_Backspace); }

void ImGuiTextSink::Finish() { Tap(ImGuiKey_Enter); }

void ImGuiTextSink::Tap(int key) {
    // ImGui takes the press and the release on separate frames.
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiKey(key), true);
    io.AddKeyEvent(ImGuiKey(key), false);
}

}  // namespace gazeora::app
