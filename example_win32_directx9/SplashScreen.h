#pragma once

#include "imgui.h"

class SplashScreen
{
public:
    SplashScreen(float duration = 2.0f);
    
    void Initialize();
    void Update();
    void Draw(float scale);
    
    bool IsActive() const { return m_bActive; }
    void Skip() { m_bActive = false; }
    
private:
    bool m_bActive;
    double m_StartTime;
    float m_Duration;
    
    void TextCentered(const char* text);
};              
