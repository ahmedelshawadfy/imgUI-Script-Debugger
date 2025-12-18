// Dear ImGui: standalone example application for Windows API + DirectX 9

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"
#include "SplashScreen.h"
#include "Theme.h"
#include "ExternalAppDetector.h"
#include <d3d9.h>
#include <tchar.h>
#include <cstdarg> // for va_list used by the log helpers
//#include "vsshint.h"

// Data
static LPDIRECT3D9              g_pD3D = nullptr;
static LPDIRECT3DDEVICE9        g_pd3dDevice = nullptr;
static bool                     g_DeviceLost = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// -- Simple ImGui log console helpers (kept static to this translation unit)
static ImGuiTextBuffer     g_LogBuf;
static ImGuiTextFilter     g_LogFilter;
static ImVector<int>      g_LogLineOffsets; // index to lines start (for quick iteration)
static bool               g_LogAutoScroll = true;

static void LogClear()
{
    g_LogBuf.clear();
    g_LogLineOffsets.clear();
    g_LogLineOffsets.push_back(0);
}

// Small helper for centered text (keeps existing code blocks intact)
namespace {
    static void TextCentered(const char* text)
    {
        ImVec2 size = ImGui::CalcTextSize(text);
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window) return;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - size.x) * 0.5f);
        ImGui::TextUnformatted(text);
    }
} // namespace

static void LogAdd(const char* fmt, ...)
{
    int old_size = g_LogBuf.size();
    va_list args;
    va_start(args, fmt);
    g_LogBuf.appendfv(fmt, args);
    va_end(args);
    for (int new_size = g_LogBuf.size(); old_size < new_size; old_size++)
        if (g_LogBuf[old_size] == '\n')
            g_LogLineOffsets.push_back(old_size + 1);
}

// Main code
int main(int, char**)
{
    // Make process DPI aware and obtain main monitor scale
    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX9 Example", WS_OVERLAPPEDWINDOW, 100, 100, (int)(1280 * main_scale), (int)(800 * main_scale), nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style - Apply Purple Theme
    Theme::ApplyTheme(ThemeType::Purple);

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    // Initialize UI components
    SplashScreen splashScreen(2.0f);
    splashScreen.Initialize();
    
    ExternalAppDetector mediaDetector;
    // Set the log callback to send messages to the ImGui log console
    mediaDetector.SetLogCallback([](const std::string& message) {
        LogAdd("%s", message.c_str());
    });
    mediaDetector.Initialize();

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    bool opened = true; // <-- Add this line to define 'opened'
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ThemeType currentTheme = ThemeType::Purple;

    // Splash screen state (shows at startup)
    bool show_splash = true;
    const float splash_duration = 2.0f; // seconds
    double splash_start_time = ImGui::GetTime(); // recorded after ImGui context is created

    // Initialize log console
    LogClear();
    LogAdd("Application started\n");
    LogAdd("Main DPI scale: %.2f\n", main_scale);
    LogAdd("Direct3D initialized successfully\n");
    LogAdd("Theme: Purple\n");

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle lost D3D9 device
        if (g_DeviceLost)
        {
            HRESULT hr = g_pd3dDevice->TestCooperativeLevel();
            if (hr == D3DERR_DEVICELOST)
            {
                ::Sleep(10);
                continue;
            }
            if (hr == D3DERR_DEVICENOTRESET)
                ResetDevice();
            g_DeviceLost = false;
        }

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            g_d3dpp.BackBufferWidth = g_ResizeWidth;
            g_d3dpp.BackBufferHeight = g_ResizeHeight;
            g_ResizeWidth = g_ResizeHeight = 0;
            ResetDevice();
            LogAdd("Window resized to %u x %u\n", g_d3dpp.BackBufferWidth, g_d3dpp.BackBufferHeight);
        }

        // Update external app detector
        mediaDetector.Update();

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Draw splash screen interface
        splashScreen.Update();
        splashScreen.Draw(main_scale);

        // Draw main application interface (only after splash is done)
        if (!splashScreen.IsActive())
        {
            if (ImGui::Begin("Image Debugger Demo", &opened))
            {

                ImGui::Text("Hello, world!");
                ImGui::Checkbox("Demo Window", &show_demo_window);
                ImGui::Checkbox("Another Window", &show_another_window);
                ImGui::ColorEdit3("clear color", (float*)&clear_color);
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

                ImGui::Separator();
                ImGui::Text("Active Application: %s", mediaDetector.GetActiveAppName().c_str());
                ImGui::Text("Media Playing: %s", mediaDetector.IsMediaPlaying() ? "Yes" : "No");
                ImGui::Text("Last Event: %s", mediaDetector.GetLastEvent().c_str());

                if (mediaDetector.IsMediaPlaying())
                {
                    ImGui::Text("Current Media App: %s", mediaDetector.GetCurrentMediaApp().c_str());
                    ImGui::Text("Status: %s", mediaDetector.GetLastEvent().c_str());
                }

                ImGui::Separator();
                ImGui::Text("Theme Selection:");
                if (ImGui::RadioButton("Dark", (int*)&currentTheme, (int)ThemeType::Dark))
                {
                    Theme::ApplyTheme(ThemeType::Dark);
                    LogAdd("Theme changed to: Dark\n");
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Purple", (int*)&currentTheme, (int)ThemeType::Purple))
                {
                    Theme::ApplyTheme(ThemeType::Purple);
                    LogAdd("Theme changed to: Purple\n");
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Light", (int*)&currentTheme, (int)ThemeType::Light))
                {
                    Theme::ApplyTheme(ThemeType::Light);
                    LogAdd("Theme changed to: Light\n");
                }

                ImGui::Separator();
                ImGui::Text("Application Log:");
                ImGui::BeginChild("LogRegion", ImVec2(0, 150), true, ImGuiWindowFlags_HorizontalScrollbar);

                // Controls
                if (ImGui::Button("Clear")) { LogClear(); }
                ImGui::SameLine();
                ImGui::Checkbox("Auto-scroll", &g_LogAutoScroll);
                ImGui::SameLine();
                g_LogFilter.Draw("Filter", -100.0f);

                ImGui::Separator();

                // Display log contents (with simple filter)
                const char* buf = g_LogBuf.begin();
                if (g_LogFilter.IsActive())
                {
                    for (int line_no = 0; line_no < g_LogLineOffsets.Size; line_no++)
                    {
                        const char* line_start = buf + g_LogLineOffsets[line_no];
                        const char* line_end = (line_no + 1 < g_LogLineOffsets.Size) ? (buf + g_LogLineOffsets[line_no + 1] - 1) : nullptr;
                        if (g_LogFilter.PassFilter(line_start, line_end))
                            ImGui::TextUnformatted(line_start, line_end);
                    }
                }
                else
                {
                    ImGui::TextUnformatted(buf);
                }

                // Auto-scroll logic
                if (g_LogAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                    ImGui::SetScrollHereY(1.0f);

                ImGui::EndChild();


                ImGui::End();

            }
        }


        // Rendering
        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x*clear_color.w*255.0f), (int)(clear_color.y*clear_color.w*255.0f), (int)(clear_color.z*clear_color.w*255.0f), (int)(clear_color.w*255.0f));
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
        if (result == D3DERR_DEVICELOST)
            g_DeviceLost = true;
    }

    // Cleanup
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;

    // Create the D3DDevice
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
    //g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

void CleanupDeviceD3D()
{
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}

void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}





