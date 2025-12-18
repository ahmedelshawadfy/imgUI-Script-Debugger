#pragma once

#include "imgui.h"

enum class ThemeType
{
    Dark,
    Purple,
    Light,
    Custom
};

class Theme
{
public:
    static void ApplyDarkTheme();
    static void ApplyPurpleTheme();
    static void ApplyLightTheme();
    static void ApplyTheme(ThemeType theme);
    
private:
    Theme() = default;
};
