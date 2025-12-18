#include "ExternalAppDetector.h"
#include <psapi.h>
#include <tlhelp32.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <ctime>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

// Callback for EnumWindows to find Explorer windows
static std::vector<HWND> g_ExplorerWindows;

BOOL CALLBACK EnumExplorerWindowsProc(HWND hwnd, LPARAM lParam)
{
    char className[256] = { 0 };
    ::GetClassNameA(hwnd, className, sizeof(className));
    
    // Look for Explorer window classes
    if (strcmp(className, "CabinetWClass") == 0 || 
        strcmp(className, "ExploreWClass") == 0)
    {
        DWORD processId = 0;
        ::GetWindowThreadProcessId(hwnd, &processId);
        
        // Verify this is an explorer.exe process
        HANDLE processHandle = ::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
        if (processHandle)
        {
            char exeName[MAX_PATH] = { 0 };
            if (::GetProcessImageFileNameA(processHandle, exeName, MAX_PATH))
            {
                std::string fullPath(exeName);
                size_t lastSlash = fullPath.find_last_of("\\");
                if (lastSlash != std::string::npos)
                {
                    std::string name = fullPath.substr(lastSlash + 1);
                    if (name == "explorer.exe")
                    {
                        g_ExplorerWindows.push_back(hwnd);
                    }
                }
            }
            ::CloseHandle(processHandle);
        }
    }
    
    return TRUE;
}

ExternalAppDetector::ExternalAppDetector()
    : m_bMediaPlaying(false),
      m_LastForegroundWindow(nullptr),
      m_bExplorerRunning(false),
      m_bExplorerActive(false),
      m_ExplorerProcessId(0),
      m_ExplorerMainWindow(nullptr),
      m_ExplorerWindowCount(0),
      m_LastExplorerCheckTime(0),
      m_LogCallback(nullptr)
{
}

ExternalAppDetector::~ExternalAppDetector()
{
    Shutdown();
}

void ExternalAppDetector::Initialize()
{
    m_LastEvent = "External App Detector initialized";
    LogEvent("[INIT] External App Detector initialized");
    LogEvent("[EXPLORER] Starting explorer.exe monitoring...");
    AddExplorerEvent(ExplorerEventType::Unknown, "Explorer monitoring initialized");
}

void ExternalAppDetector::Shutdown()
{
    // Clean up any resources if needed
    m_ExplorerEvents.clear();
}

void ExternalAppDetector::Update()
{
    // Check for active application changes
    HWND foregroundWindow = ::GetForegroundWindow();
    if (foregroundWindow != m_LastForegroundWindow)
    {
        m_LastForegroundWindow = foregroundWindow;
        std::string appName = GetActiveApplicationName();
        if (!appName.empty())
        {
            m_ActiveAppName = appName;
            LogEvent("[APP] Application switched to: " + appName);
        }
    }
    
    // Check media playback status
    CheckMediaPlayback();
    
    // Check explorer status (throttle to every 200ms to reduce CPU usage)
    ULONGLONG currentTime = ::GetTickCount64();
    if (currentTime - m_LastExplorerCheckTime > 200)
    {
        m_LastExplorerCheckTime = currentTime;
        CheckExplorerStatus();
    }
}

std::string ExternalAppDetector::GetActiveApplicationName()
{
    HWND foregroundWindow = ::GetForegroundWindow();
    if (!foregroundWindow)
        return "";
    
    DWORD processId = 0;
    ::GetWindowThreadProcessId(foregroundWindow, &processId);
    
    HANDLE processHandle = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!processHandle)
        return "";
    
    char filename[MAX_PATH];
    if (::GetProcessImageFileNameA(processHandle, filename, MAX_PATH))
    {
        std::string fullPath(filename);
        size_t lastSlash = fullPath.find_last_of("\\");
        if (lastSlash != std::string::npos)
        {
            ::CloseHandle(processHandle);
            return fullPath.substr(lastSlash + 1);
        }
    }
    
    ::CloseHandle(processHandle);
    return "";
}

void ExternalAppDetector::CheckMediaPlayback()
{
    HWND winampWindow = ::FindWindowA("Winamp v1.x", nullptr);
    
    bool wasPlaying = m_bMediaPlaying;
    m_bMediaPlaying = (winampWindow != nullptr);
    
    if (m_bMediaPlaying && !wasPlaying)
    {
        LogEvent("[MEDIA] Media playback detected");
    }
    else if (!m_bMediaPlaying && wasPlaying)
    {
        LogEvent("[MEDIA] Media playback stopped");
    }
}

void ExternalAppDetector::CheckExplorerStatus()
{
    // Create a process snapshot to find explorer.exe
    HANDLE hSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return;
    
    PROCESSENTRY32 pe32 = { 0 };
    pe32.dwSize = sizeof(PROCESSENTRY32);
    
    bool explorerFoundInSnapshot = false;
    DWORD explorerPid = 0;
    
    if (::Process32First(hSnapshot, &pe32))
    {
        do
        {
            if (_wcsicmp(pe32.szExeFile, L"explorer.exe") == 0)
            {
                explorerFoundInSnapshot = true;
                explorerPid = pe32.th32ProcessID;
                break;
            }
        } while (::Process32Next(hSnapshot, &pe32));
    }
    
    ::CloseHandle(hSnapshot);
    
    // Check for state change: started or terminated
    bool wasRunning = m_bExplorerRunning;
    m_bExplorerRunning = explorerFoundInSnapshot;
    
    if (m_bExplorerRunning && !wasRunning)
    {
        m_ExplorerProcessId = explorerPid;
        std::string logMsg = "[EXPLORER] explorer.exe STARTED (PID: " + std::to_string(explorerPid) + ")";
        LogEvent(logMsg);
        AddExplorerEvent(ExplorerEventType::ProcessStarted, 
                        "explorer.exe started", 
                        "PID: " + std::to_string(explorerPid));
    }
    else if (!m_bExplorerRunning && wasRunning)
    {
        LogEvent("[EXPLORER] explorer.exe TERMINATED");
        AddExplorerEvent(ExplorerEventType::ProcessTerminated, "explorer.exe terminated");
        m_ExplorerProcessId = 0;
        m_ExplorerMainWindow = nullptr;
        m_ExplorerWindowTitle = "";
        m_ExplorerCurrentPath = "";
        m_ExplorerWindowCount = 0;
        m_bExplorerActive = false;
        return;
    }
    
    if (!m_bExplorerRunning)
        return;
    
    // Enumerate Explorer windows
    DetectExplorerWindows();
    
    // Check if main window is in foreground
    HWND foregroundWindow = ::GetForegroundWindow();
    bool wasActive = m_bExplorerActive;
    m_bExplorerActive = false;
    
    for (HWND hwnd : g_ExplorerWindows)
    {
        if (hwnd == foregroundWindow)
        {
            m_bExplorerActive = true;
            m_ExplorerMainWindow = hwnd;
            break;
        }
    }
    
    if (m_bExplorerActive && !wasActive)
    {
        LogEvent("[EXPLORER] Explorer window ACTIVATED");
        AddExplorerEvent(ExplorerEventType::WindowCreated, "Explorer window activated");
    }
    else if (!m_bExplorerActive && wasActive)
    {
        LogEvent("[EXPLORER] Explorer window DEACTIVATED");
    }
    
    // Extract current path and window info
    if (m_ExplorerMainWindow)
    {
        ExtractPathFromExplorer();
    }
}

void ExternalAppDetector::DetectExplorerWindows()
{
    g_ExplorerWindows.clear();
    ::EnumWindows(EnumExplorerWindowsProc, 0);
    
    int newWindowCount = static_cast<int>(g_ExplorerWindows.size());
    if (newWindowCount != m_ExplorerWindowCount)
    {
        m_ExplorerWindowCount = newWindowCount;
        if (newWindowCount > 0)
        {
            m_ExplorerMainWindow = g_ExplorerWindows[0];
            LogEvent("[EXPLORER] Found " + std::to_string(newWindowCount) + " explorer window(s)");
        }
    }
}

void ExternalAppDetector::ExtractPathFromExplorer()
{
    if (!m_ExplorerMainWindow || !::IsWindow(m_ExplorerMainWindow))
        return;
    
    // Get window title (often contains the current path)
    wchar_t windowTitle[MAX_PATH] = { 0 };
    ::GetWindowTextW(m_ExplorerMainWindow, windowTitle, MAX_PATH);
    
    // Convert wide string to regular string
    int size = ::WideCharToMultiByte(CP_UTF8, 0, windowTitle, -1, nullptr, 0, nullptr, nullptr);
    if (size > 0)
    {
        std::string newPath(size - 1, 0);
        ::WideCharToMultiByte(CP_UTF8, 0, windowTitle, -1, &newPath[0], size, nullptr, nullptr);
        
        // Update if changed
        if (newPath != m_ExplorerCurrentPath)
        {
            m_ExplorerCurrentPath = newPath;
            m_ExplorerWindowTitle = newPath;
            LogEvent("[EXPLORER] Directory changed to: " + newPath);
            AddExplorerEvent(ExplorerEventType::DirectoryChanged, 
                           "Directory changed", 
                           newPath);
        }
    }
}

void ExternalAppDetector::AddExplorerEvent(ExplorerEventType type, const std::string& description, const std::string& path)
{
    ExplorerEvent event;
    event.type = type;
    event.description = description;
    event.path = path;
    event.timestamp = static_cast<double>(::GetTickCount64()) / 1000.0;
    
    m_ExplorerEvents.push_back(event);
    
    // Keep only the last 50 events to prevent memory bloat
    if (m_ExplorerEvents.size() > 50)
    {
        m_ExplorerEvents.erase(m_ExplorerEvents.begin());
    }
}

ExplorerEvent ExternalAppDetector::GetLatestExplorerEvent() const
{
    if (m_ExplorerEvents.empty())
    {
        ExplorerEvent emptyEvent;
        emptyEvent.type = ExplorerEventType::Unknown;
        emptyEvent.description = "No events";
        emptyEvent.timestamp = 0.0;
        return emptyEvent;
    }
    return m_ExplorerEvents.back();
}

std::vector<ExplorerEvent> ExternalAppDetector::GetExplorerEvents(size_t maxCount) const
{
    std::vector<ExplorerEvent> result;
    
    size_t startIdx = m_ExplorerEvents.size() > maxCount ? m_ExplorerEvents.size() - maxCount : 0;
    for (size_t i = startIdx; i < m_ExplorerEvents.size(); ++i)
    {
        result.push_back(m_ExplorerEvents[i]);
    }
    
    return result;
}

std::string ExternalAppDetector::EventTypeToString(ExplorerEventType type) const
{
    switch (type)
    {
        case ExplorerEventType::ProcessStarted:       return "Process Started";
        case ExplorerEventType::ProcessTerminated:    return "Process Terminated";
        case ExplorerEventType::WindowCreated:        return "Window Created";
        case ExplorerEventType::WindowClosed:         return "Window Closed";
        case ExplorerEventType::DirectoryChanged:     return "Directory Changed";
        case ExplorerEventType::FileOperationDetected: return "File Operation";
        default:                                      return "Unknown";
    }
}

void ExternalAppDetector::LogEvent(const std::string& event)
{
    m_LastEvent = event;
    
    // Call the external log callback if set
    if (m_LogCallback)
    {
        m_LogCallback(event + "\n");
    }
}
