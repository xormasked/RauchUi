#pragma once

#include "imgui.h"

#include <cstddef>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace RauchUi
{
    namespace palette
    {
        inline constexpr ImU32 background      = IM_COL32(10, 10, 11, 255);
        inline constexpr ImU32 sidebar         = IM_COL32(6, 6, 7, 255);
        inline constexpr ImU32 card            = IM_COL32(15, 15, 16, 255);
        inline constexpr ImU32 cardRaised      = IM_COL32(20, 20, 22, 255);
        inline constexpr ImU32 border          = IM_COL32(34, 34, 37, 255);
        inline constexpr ImU32 track           = IM_COL32(40, 40, 44, 255);
        inline constexpr ImU32 highlight       = IM_COL32(255, 255, 255, 12);
        inline constexpr ImU32 highlightHover  = IM_COL32(255, 255, 255, 7);
        inline constexpr ImU32 text            = IM_COL32(240, 240, 242, 255);
        inline constexpr ImU32 textBody        = IM_COL32(190, 190, 196, 255);
        inline constexpr ImU32 textDim         = IM_COL32(120, 120, 128, 255);
        inline constexpr ImU32 textFaint       = IM_COL32(78, 78, 86, 255);
        inline constexpr ImU32 positive        = IM_COL32(64, 201, 110, 255);
        inline constexpr ImU32 warning         = IM_COL32(232, 184, 64, 255);
        inline constexpr ImU32 negative        = IM_COL32(255, 6, 45, 255);
        inline constexpr ImU32 accent          = IM_COL32(255, 6, 45, 255);
        inline constexpr ImU32 accentHover     = IM_COL32(255, 52, 84, 255);
        inline constexpr ImU32 accentActive    = IM_COL32(208, 4, 36, 255);
        inline constexpr ImU32 accent2         = IM_COL32(255, 4, 91, 255);
        inline constexpr ImU32 onAccent        = IM_COL32(255, 255, 255, 255);
        inline constexpr ImU32 secondary       = IM_COL32(16, 16, 18, 255);
        inline constexpr ImU32 secondaryHover  = IM_COL32(24, 24, 27, 255);
        inline constexpr ImU32 secondaryActive = IM_COL32(12, 12, 13, 255);
    }

    namespace fonts
    {
        inline ImFont* regular  = nullptr;
        inline ImFont* semibold = nullptr;
        inline ImFont* mono     = nullptr;
    }

    enum class Kind { Primary, Secondary, Danger };
    enum class Tone { Info, Success, Warning, Error };

    struct Settings
    {
        bool glassBackground = false;
    };

    inline Settings settings;

    void Init();

    ImU32  Alpha(ImU32 color, float alpha);
    ImU32  Mix(ImU32 from, ImU32 to, float t);
    float  Fade(ImGuiID id, bool target, float speed = 16.0f);
    ImVec2 Measure(ImFont* font, const char* text, const char* end = nullptr);
    void   TextAt(ImDrawList* dl, ImFont* font, ImVec2 pos, ImU32 color, const char* text, const char* end = nullptr);
    void   Gradient(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 from, ImU32 to, float rounding = 0.0f, bool vertical = false);
    void   FadeIn(ImDrawList* dl, int vertexStart, double started);

    bool BeginWindow(const char* title, bool* open, ImVec2 size, const char* const* tabs = nullptr, int tabCount = 0, int* tab = nullptr);
    void EndWindow();
    bool Tabs(const char* const* items, int count, int* selected, float height);

    void  BeginGroups();
    void  EndGroups();
    void  BeginGroup(const char* title);
    void  EndGroup();
    void  Separator(const char* label);
    float RowWidth();

    bool Checkbox(const char* label, bool* value, ImVec4* color = nullptr);
    bool Slider(const char* label, float* value, float min, float max, const char* format = "%.2f");
    bool SliderInt(const char* label, int* value, int min, int max);
    bool Dropdown(const char* label, int* selected, const char* const* items, int count);
    bool Keybind(const char* label, ImGuiKey* key);
    bool ColorRow(const char* label, ImVec4* color);
    bool TextBox(const char* id, char* buffer, size_t size, const char* hint, float width = 0.0f);
    bool Button(const char* label, Kind kind = Kind::Secondary, float width = 0.0f);
    void Value(const char* label, const char* value);

    void Watermark(bool visible, const char* const* segments, int count);
    void Notify(Tone tone, const char* format, ...) IM_FMTARGS(2);
    void DrawNotifications();

    namespace glass
    {
        struct Params
        {
            bool   enabled    = true;
            float  blur       = 4.0f;
            float  refraction = 1.0f;
            float  noise      = 0.03f;
            float  glow       = 0.25f;
            ImVec4 tint       = ImVec4(0.04f, 0.03f, 0.06f, 0.45f);
        };

        inline Params params;

        bool Init(ID3D11Device* device, ID3D11DeviceContext* context);
        void Shutdown();
        void Panel(ImDrawList* dl, ImVec2 min, ImVec2 max, float radius);
    }
}

namespace ui = RauchUi;
