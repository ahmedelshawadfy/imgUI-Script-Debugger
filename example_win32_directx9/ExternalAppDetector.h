#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>

enum class ExplorerEventType
{
    Unknown,
    ProcessStarted,
    ProcessTerminated,
    WindowCreated,
    WindowClosed,
    DirectoryChanged,
    FileOperationDetected
};

struct ExplorerEvent
{
    ExplorerEventType type;
    std::string description;
    std::string path;
    double timestamp;  // Changed from ULONGLONG to double for consistency
};

// Callback type for external logging
using LogCallbackFn = std::function<void(const std::string&)>;

class ExternalAppDetector
{
public:
    ExternalAppDetector();
    ~ExternalAppDetector();
    
    void Initialize();
    void Update();
    void Shutdown();
    
    // Set an external log callback to display messages in UI log console
    void SetLogCallback(LogCallbackFn callback) { m_LogCallback = callback; }
    
    // Existing media detection
    bool IsMediaPlaying() const { return m_bMediaPlaying; }
    std::string GetActiveAppName() const { return m_ActiveAppName; }
    std::string GetLastEvent() const { return m_LastEvent; }
    std::string GetCurrentMediaApp() const { return m_ActiveAppName; }
    
    // Explorer.exe detection methods
    bool IsExplorerRunning() const { return m_bExplorerRunning; }
    bool IsExplorerActive() const { return m_bExplorerActive; }
    DWORD GetExplorerProcessId() const { return m_ExplorerProcessId; }
    std::string GetExplorerWindowTitle() const { return m_ExplorerWindowTitle; }
    std::string GetExplorerCurrentPath() const { return m_ExplorerCurrentPath; }
    int GetExplorerWindowCount() const { return m_ExplorerWindowCount; }
    size_t GetExplorerEventCount() const { return m_ExplorerEvents.size(); }
    
    // Event management
    ExplorerEvent GetLatestExplorerEvent() const;
    std::vector<ExplorerEvent> GetExplorerEvents(size_t maxCount = 20) const;
    void ClearExplorerEvents() { m_ExplorerEvents.clear(); }
    
private:
    // Existing members
    bool m_bMediaPlaying;
    std::string m_ActiveAppName;
    std::string m_LastEvent;
    HWND m_LastForegroundWindow;
    
    // Explorer.exe monitoring
    bool m_bExplorerRunning;
    bool m_bExplorerActive;
    DWORD m_ExplorerProcessId;
    HWND m_ExplorerMainWindow;
    std::string m_ExplorerWindowTitle;
    std::string m_ExplorerCurrentPath;
    int m_ExplorerWindowCount;
    ULONGLONG m_LastExplorerCheckTime;
    
    std::vector<ExplorerEvent> m_ExplorerEvents;
    
    // Log callback for external logging
    LogCallbackFn m_LogCallback;
    
    // Private methods
    std::string GetActiveApplicationName();
    void CheckMediaPlayback();
    void CheckExplorerStatus();
    void DetectExplorerWindows();
    void ExtractPathFromExplorer();
    void AddExplorerEvent(ExplorerEventType type, const std::string& description, const std::string& path = "");
    std::string EventTypeToString(ExplorerEventType type) const;
    void LogEvent(const std::string& event);
};
