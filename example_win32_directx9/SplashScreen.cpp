#include "SplashScreen.h"
#include "imgui.h"
#include <imgui_internal.h>

SplashScreen::SplashScreen(float duration)
    : m_bActive(true), m_StartTime(0.0), m_Duration(duration)
{
}

void SplashScreen::Initialize()
{
    m_StartTime = ImGui::GetTime();
    m_bActive = true;
}

void SplashScreen::TextCentered(const char* text)
{
    ImVec2 size = ImGui::CalcTextSize(text);
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (!window) return;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - size.x) * 0.5f);
    ImGui::TextUnformatted(text);
}

void SplashScreen::Update()
{
    double now = ImGui::GetTime();
    float elapsed = (float)(now - m_StartTime);
    
    if (elapsed >= m_Duration)
    {
        m_bActive = false;
    }
    
    // Allow skipping by click or key press
    if (ImGui::IsMouseClicked(0) || ImGui::IsKeyPressed(ImGuiKey_Space))
    {
        m_bActive = false;
    }
}

void SplashScreen::Draw(float scale)
{
    if (!m_bActive)
        return;
    
    ImGuiIO& io = ImGui::GetIO();
    double now = ImGui::GetTime();
    float elapsed = (float)(now - m_StartTime);
    
    // Setup window size and position (centered)
    ImVec2 splash_size = ImVec2(500.0f * scale, 220.0f * scale);
    ImVec2 display = io.DisplaySize;
    ImVec2 pos = ImVec2((display.x - splash_size.x) * 0.5f, (display.y - splash_size.y) * 0.5f);
    ImGui::SetNextWindowBgAlpha(0.95f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(splash_size, ImGuiCond_Always);
    
    ImGuiWindowFlags splash_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
    
    ImGui::Begin("##splash", nullptr, splash_flags);
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::PushFont(io.Fonts->Fonts.size() ? io.Fonts->Fonts[0] : nullptr);
    TextCentered("Image Debugger");
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
    ImGui::TextWrapped("Welcome to the Image Debugger. Loading...");
    
    // Progress bar
    float t = elapsed / m_Duration;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.0f);
    ImGui::ProgressBar(t, ImVec2(-1, 0));
    
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::Text("Click anywhere to skip");
    
    ImGui::End();
}
