#include "RauchUiRoblox.h"
#include "imgui_internal.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

namespace RauchUi
{
    namespace
    {
        constexpr float kRowH     = 20.0f;
        constexpr float kFieldH   = 22.0f;
        constexpr float kGroupGap = 14.0f;
        constexpr float kRound    = 0.0f;

        struct GroupArea
        {
            bool   active = false;
            ImVec2 origin;
            float  width = 0.0f;
            int    columns = 1;
            float  columnY[2] = {};
        };

        struct GroupFrame
        {
            bool        active = false;
            ImVec2      min;
            float       width = 0.0f;
            int         column = -1;
            const char* title = nullptr;
        };

        GroupArea  g_area;
        GroupFrame g_frame;


        void Hand()
        {
            if (ImGui::IsItemHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        const char* LabelEnd(const char* label) { return ImGui::FindRenderedTextEnd(label); }

        void Field(ImDrawList* dl, ImVec2 min, ImVec2 max, float hover, bool focus)
        {
            dl->AddRectFilled(min, max, Mix(palette::secondary, palette::secondaryHover, hover), kRound);
            dl->AddRect(min, max, focus ? palette::accent : Mix(palette::border, palette::textFaint, hover), kRound);
        }

        void Chevron(ImDrawList* dl, ImVec2 c, bool up, ImU32 col)
        {
            const float s = 3.5f, f = up ? -1.0f : 1.0f;
            dl->AddLine(ImVec2(c.x - s, c.y - s * 0.5f * f), ImVec2(c.x, c.y + s * 0.5f * f), col, 1.4f);
            dl->AddLine(ImVec2(c.x, c.y + s * 0.5f * f), ImVec2(c.x + s, c.y - s * 0.5f * f), col, 1.4f);
        }

        void PushPopupStyle(float padding, float spacing)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, spacing));
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(palette::card));
            ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(palette::border));
        }

        void PopPopupStyle()
        {
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(4);
        }

        constexpr float kPickerW   = 180.0f;
        constexpr float kPickerSV  = 110.0f;
        constexpr float kPickerBar = 8.0f;
        constexpr float kPresetR   = 6.5f;

        constexpr ImU32 kPresets[] = {
            IM_COL32(255, 255, 255, 255), IM_COL32(255, 6, 45, 255),   IM_COL32(255, 4, 91, 255), IM_COL32(59, 130, 246, 255),
            IM_COL32(6, 182, 212, 255),   IM_COL32(34, 197, 94, 255),  IM_COL32(234, 179, 8, 255),  IM_COL32(239, 68, 68, 255),
        };

        ImU32 HueColor(float h)
        {
            float r, g, b;
            ImGui::ColorConvertHSVtoRGB(h, 1, 1, r, g, b);
            return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 1));
        }

        void Checker(ImDrawList* dl, ImVec2 min, ImVec2 max)
        {
            const float cell = 4.0f;
            dl->AddRectFilled(min, max, palette::track);
            for (float y = min.y; y < max.y; y += cell)
                for (float x = min.x; x < max.x; x += cell)
                    if ((static_cast<int>((x - min.x) / cell) + static_cast<int>((y - min.y) / cell)) % 2)
                        dl->AddRectFilled(ImVec2(x, y), ImVec2(ImMin(x + cell, max.x), ImMin(y + cell, max.y)), palette::secondaryHover);
        }

        void Knob(ImDrawList* dl, ImVec2 c, ImU32 fill)
        {
            dl->AddCircleFilled(c, 6.5f, IM_COL32(0, 0, 0, 110));
            dl->AddCircleFilled(c, 5.5f, IM_COL32_WHITE);
            dl->AddCircleFilled(c, 3.8f, fill);
        }

        bool DragRegion(const char* id, ImVec2 size, ImVec2& fraction)
        {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton(id, size);
            Hand();
            if (!ImGui::IsItemActive())
                return false;
            ImVec2 m = ImGui::GetIO().MousePos;
            fraction = ImVec2(ImSaturate((m.x - pos.x) / size.x), ImSaturate((m.y - pos.y) / size.y));
            return true;
        }

        bool ColorPicker(ImVec4* color)
        {
            ImDrawList*   dl      = ImGui::GetWindowDrawList();
            ImGuiStorage* storage = ImGui::GetStateStorage();
            const ImGuiID hueKey  = ImGui::GetID("##hue");

            float h, s, v;
            ImGui::ColorConvertRGBtoHSV(color->x, color->y, color->z, h, s, v);
            float stored = storage->GetFloat(hueKey, -1.0f);
            if ((s <= 0.0f || v <= 0.0f) && stored >= 0.0f)
                h = stored;

            bool   changed = false;
            ImVec2 f;

            ImVec2 sv = ImGui::GetCursorScreenPos();
            if (DragRegion("##sv", ImVec2(kPickerW, kPickerSV), f))
            {
                s = f.x;
                v = 1.0f - f.y;
                changed = true;
            }
            ImVec2 svMax(sv.x + kPickerW, sv.y + kPickerSV);
            dl->AddRectFilledMultiColor(sv, svMax, IM_COL32_WHITE, HueColor(h), HueColor(h), IM_COL32_WHITE);
            dl->AddRectFilledMultiColor(sv, svMax, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32_BLACK, IM_COL32_BLACK);
            dl->AddRect(sv, svMax, palette::border);

            ImVec2 hue = ImGui::GetCursorScreenPos();
            if (DragRegion("##huebar", ImVec2(kPickerW, kPickerBar), f))
            {
                h = ImMin(f.x, 0.9999f);
                changed = true;
            }
            for (int i = 0; i < 6; ++i)
            {
                float x0 = hue.x + kPickerW * i / 6.0f, x1 = hue.x + kPickerW * (i + 1) / 6.0f;
                ImU32 c0 = HueColor(i / 6.0f), c1 = HueColor((i + 1) / 6.0f);
                dl->AddRectFilledMultiColor(ImVec2(x0, hue.y), ImVec2(x1, hue.y + kPickerBar), c0, c1, c1, c0);
            }
            dl->AddRect(hue, ImVec2(hue.x + kPickerW, hue.y + kPickerBar), palette::border);

            ImVec2 alpha = ImGui::GetCursorScreenPos();
            if (DragRegion("##alphabar", ImVec2(kPickerW, kPickerBar), f))
            {
                color->w = f.x;
                changed = true;
            }
            float r, g, b;
            ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
            ImVec2 alphaMax(alpha.x + kPickerW, alpha.y + kPickerBar);
            Checker(dl, alpha, alphaMax);
            ImU32 clear = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 0)), solid = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 1));
            dl->AddRectFilledMultiColor(alpha, alphaMax, clear, solid, solid, clear);
            dl->AddRect(alpha, alphaMax, palette::border);

            const float presetY = ImGui::GetCursorScreenPos().y;
            const int   count   = IM_ARRAYSIZE(kPresets);
            const float gap     = (kPickerW - kPresetR * 2.0f * count) / (count - 1);
            for (int i = 0; i < count; ++i)
            {
                ImVec2 min(sv.x + i * (kPresetR * 2.0f + gap), presetY), c(min.x + kPresetR, min.y + kPresetR);
                ImVec4 preset = ImGui::ColorConvertU32ToFloat4(kPresets[i]);
                ImGui::PushID(i);
                ImGui::SetCursorScreenPos(min);
                bool pressed = ImGui::InvisibleButton("##preset", ImVec2(kPresetR * 2, kPresetR * 2));
                Hand();
                bool hovered = ImGui::IsItemHovered();
                ImGui::PopID();
                if (pressed)
                {
                    ImGui::ColorConvertRGBtoHSV(preset.x, preset.y, preset.z, h, s, v);
                    r = preset.x; g = preset.y; b = preset.z;
                    changed = true;
                }
                const float match = 1.5f / 255.0f;
                bool selected = fabsf(preset.x - r) < match && fabsf(preset.y - g) < match && fabsf(preset.z - b) < match;
                dl->AddCircleFilled(c, kPresetR, kPresets[i]);
                if (selected || hovered)
                    dl->AddCircle(c, kPresetR + 2.0f, selected ? palette::text : palette::textDim, 0, 1.3f);
            }
            ImGui::SetCursorScreenPos(ImVec2(sv.x, presetY));
            ImGui::Dummy(ImVec2(kPickerW, kPresetR * 2));

            char hex[16];
            snprintf(hex, sizeof(hex), "#%02X%02X%02X", static_cast<int>(r * 255 + 0.5f), static_cast<int>(g * 255 + 0.5f), static_cast<int>(b * 255 + 0.5f));
            ImVec2 hp = ImGui::GetCursorScreenPos();
            TextAt(dl, fonts::mono, hp, palette::textDim, hex);
            char pct[8];
            snprintf(pct, sizeof(pct), "%d%%", static_cast<int>(color->w * 100 + 0.5f));
            TextAt(dl, fonts::mono, ImVec2(hp.x + kPickerW - Measure(fonts::mono, pct).x, hp.y), palette::textDim, pct);
            ImGui::Dummy(ImVec2(kPickerW, Measure(fonts::mono, hex).y));

            ImU32 opaque = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 1));
            Knob(dl, ImVec2(sv.x + s * kPickerW, sv.y + (1.0f - v) * kPickerSV), opaque);
            Knob(dl, ImVec2(hue.x + h * kPickerW, hue.y + kPickerBar * 0.5f), HueColor(h));
            Knob(dl, ImVec2(alpha.x + color->w * kPickerW, alpha.y + kPickerBar * 0.5f), opaque);

            storage->SetFloat(hueKey, h);
            if (changed)
            {
                color->x = r;
                color->y = g;
                color->z = b;
            }
            return changed;
        }

        bool ColorPopup(const char* id, ImVec4* color, ImVec2 anchor)
        {
            ImGui::SetNextWindowPos(anchor, ImGuiCond_Appearing, ImVec2(1, 0));
            PushPopupStyle(10.0f, 8.0f);
            bool changed = false;
            if (ImGui::BeginPopup(id, ImGuiWindowFlags_NoMove))
            {
                changed = ColorPicker(color);
                ImGui::EndPopup();
            }
            PopPopupStyle();
            return changed;
        }

        bool Swatch(const char* id, ImVec2 min, ImVec4* color)
        {
            const ImVec2 size(20.0f, 11.0f), max(min.x + size.x, min.y + size.y);
            ImGui::SetCursorScreenPos(min);
            ImGui::PushID(id);
            if (ImGui::InvisibleButton("##swatch", size))
                ImGui::OpenPopup("##color");
            Hand();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            Checker(dl, min, max);
            dl->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(*color));
            dl->AddRect(min, max, ImGui::IsItemHovered() ? palette::textDim : palette::border);
            bool changed = ColorPopup("##color", color, ImVec2(max.x, max.y + 6.0f));
            ImGui::PopID();
            return changed;
        }

        struct Toast
        {
            Tone        tone;
            std::string text;
            float       length;
            double      start = -1.0;
            double      exit  = -1.0;
            float       y     = -1.0f;
        };

        std::mutex         g_toastLock;
        std::vector<Toast> g_pending;
        std::deque<Toast>  g_toasts;

        ImFont* Font(ImFont* f) { return f ? f : ImGui::GetFont(); }

        const char* SafeFloatFormat(const char* format, const char* fallback)
        {
            if (!format)
                return fallback;
            int conversions = 0;
            for (const char* p = format; *p;)
            {
                if (*p++ != '%')
                    continue;
                if (*p == '%')
                {
                    ++p;
                    continue;
                }
                ++conversions;
                while (*p && strchr("-+ #0", *p))
                    ++p;
                while (*p >= '0' && *p <= '9')
                    ++p;
                if (*p == '.')
                    for (++p; *p >= '0' && *p <= '9';)
                        ++p;
                if (!*p || !strchr("fFeEgG", *p))
                    return fallback;
                ++p;
            }
            return conversions == 1 ? format : fallback;
        }

        void AbortGroup()
        {
            if (!g_frame.active)
                return;
            ImGui::PopID();
            ImGui::EndGroup();
            g_frame.active = false;
        }

        constexpr float kTitleH = 30.0f;
        constexpr float kTabH   = 24.0f;
        constexpr float kEdge   = 10.0f;

        struct WindowState
        {
            ImVec2 pos;
            bool   positioned = false;
            bool   moving     = false;
            int    shownTab   = -1;
            double tabStarted = 0.0;
        };

        std::unordered_map<ImGuiID, WindowState> g_windows;
        WindowState* g_current   = nullptr;
        int          g_bodyStart = 0;
    }
}

namespace RauchUi::glass
{
    namespace
    {
        const char kShader[] = R"(
    cbuffer P : register(b0)
    {
        float4 rect;
        float2 screen;
        float  radius;
        float  refract;
        float2 blurDir;
        float  noise;
        float  glow;
        float4 tint;
    };

    Texture2D    tex : register(t0);
    SamplerState smp : register(s0);

    struct VO { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

    VO vs(uint id : SV_VertexID)
    {
        float2 t  = float2(id & 1, id >> 1);
        float2 px = lerp(rect.xy, rect.zw, t);
        VO o;
        o.pos = float4(px / screen * float2(2, -2) + float2(-1, 1), 0, 1);
        o.uv  = px / screen;
        return o;
    }

    float4 ps_blur(VO i) : SV_Target
    {
        float2 o1 = 1.411764705882353  * blurDir;
        float2 o2 = 3.2941176470588234 * blurDir;
        float2 o3 = 5.176470588235294  * blurDir;
        float3 c = tex.Sample(smp, i.uv).rgb * 0.1964825501511404;
        c += (tex.Sample(smp, i.uv + o1).rgb + tex.Sample(smp, i.uv - o1).rgb) * 0.2969069646728344;
        c += (tex.Sample(smp, i.uv + o2).rgb + tex.Sample(smp, i.uv - o2).rgb) * 0.09447039785044732;
        c += (tex.Sample(smp, i.uv + o3).rgb + tex.Sample(smp, i.uv - o3).rgb) * 0.010381362401148057;
        return float4(c, 1);
    }

    float sdRoundBox(float2 p, float2 b, float r)
    {
        float2 q = abs(p) - b + r;
        return length(max(q, 0)) + min(max(q.x, q.y), 0) - r;
    }

    float f(float x) { return 1.0 - 2.3 * pow(5.2 * 2.718281828, -6.9 * x - 0.7); }

    float rand(float2 co) { return frac(sin(dot(co, float2(12.9898, 78.233))) * 43758.5453); }

    float4 ps_glass(VO i) : SV_Target
    {
        float2 px = i.pos.xy;
        float2 c  = (rect.xy + rect.zw) * 0.5;
        float2 h  = (rect.zw - rect.xy) * 0.5;
        float  d  = sdRoundBox(px - c, h, radius);
        float  a  = saturate(0.5 - d);
        if (a <= 0) discard;

        float  dist = -d / min(h.x, h.y);
        float2 sp   = c + (px - c) * pow(saturate(f(dist)), refract);
        float3 col  = tex.Sample(smp, sp / screen).rgb;
        col += (rand(px * 1e-3) - 0.5) * noise;

        float2 n = (px - c) / h;
        col *= sin(atan2(-n.y, n.x) - 0.5) * glow * (1 - smoothstep(-0.5, 0.5, dist)) + 1;
        col  = lerp(col, tint.rgb, tint.a);
        return float4(col, a);
    }
    )";

        struct alignas(16) Constants
        {
            float rect[4];
            float screen[2];
            float radius, refract;
            float blurDir[2];
            float noise, glow;
            float tint[4];
        };

        struct Job
        {
            float x0, y0, x1, y1, radius;
            int   frame;
        };

        struct Target
        {
            ID3D11Texture2D*          tex = nullptr;
            ID3D11RenderTargetView*   rtv = nullptr;
            ID3D11ShaderResourceView* srv = nullptr;

            void Release()
            {
                if (srv) srv->Release();
                if (rtv) rtv->Release();
                if (tex) tex->Release();
                *this = {};
            }
        };

        ID3D11Device*        g_dev = nullptr;
        ID3D11DeviceContext* g_ctx = nullptr;
        ID3D11VertexShader*  g_vs  = nullptr;
        ID3D11PixelShader*   g_psBlur  = nullptr;
        ID3D11PixelShader*   g_psGlass = nullptr;
        ID3D11Buffer*        g_cb  = nullptr;
        ID3D11SamplerState*  g_smp = nullptr;

        Target      g_scene, g_blurA, g_blurB;
        UINT        g_w = 0, g_h = 0;
        DXGI_FORMAT g_fmt = DXGI_FORMAT_UNKNOWN;
        int         g_lastFrame = -1;

        DXGI_FORMAT Typed(DXGI_FORMAT f)
        {
            switch (f)
            {
            case DXGI_FORMAT_R8G8B8A8_TYPELESS:    return DXGI_FORMAT_R8G8B8A8_UNORM;
            case DXGI_FORMAT_B8G8R8A8_TYPELESS:    return DXGI_FORMAT_B8G8R8A8_UNORM;
            case DXGI_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_UNORM;
            case DXGI_FORMAT_R16G16B16A16_TYPELESS:return DXGI_FORMAT_R16G16B16A16_FLOAT;
            default:                               return f;
            }
        }

        ID3DBlob* Compile(const char* entry, const char* profile)
        {
            ID3DBlob* code = nullptr;
            ID3DBlob* err  = nullptr;
            D3DCompile(kShader, sizeof(kShader) - 1, "glass", nullptr, nullptr, entry, profile,
                       D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &err);
            if (err)
            {
                OutputDebugStringA(static_cast<const char*>(err->GetBufferPointer()));
                err->Release();
            }
            return code;
        }

        bool MakeTarget(Target& t, UINT w, UINT h, DXGI_FORMAT texFmt, DXGI_FORMAT viewFmt, bool renderTarget)
        {
            D3D11_TEXTURE2D_DESC d{};
            d.Width = w; d.Height = h; d.MipLevels = 1; d.ArraySize = 1;
            d.Format = texFmt; d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT;
            d.BindFlags = D3D11_BIND_SHADER_RESOURCE | (renderTarget ? D3D11_BIND_RENDER_TARGET : 0);
            if (FAILED(g_dev->CreateTexture2D(&d, nullptr, &t.tex)))
                return false;

            D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
            sd.Format = viewFmt; sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; sd.Texture2D.MipLevels = 1;
            if (FAILED(g_dev->CreateShaderResourceView(t.tex, &sd, &t.srv)))
                return false;

            return !renderTarget || SUCCEEDED(g_dev->CreateRenderTargetView(t.tex, nullptr, &t.rtv));
        }

        bool EnsureTargets(UINT w, UINT h, DXGI_FORMAT fmt)
        {
            if (g_scene.tex && w == g_w && h == g_h && fmt == g_fmt)
                return true;

            g_scene.Release(); g_blurA.Release(); g_blurB.Release();
            g_w = w; g_h = h; g_fmt = fmt;

            const UINT bw = w / 2 ? w / 2 : 1, bh = h / 2 ? h / 2 : 1;
            bool ok = MakeTarget(g_scene, w, h, fmt, Typed(fmt), false) &&
                      MakeTarget(g_blurA, bw, bh, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, true) &&
                      MakeTarget(g_blurB, bw, bh, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, true);
            if (!ok)
            {
                g_scene.Release(); g_blurA.Release(); g_blurB.Release();
                g_w = g_h = 0;
            }
            return ok;
        }

        void Upload(const Constants& c)
        {
            D3D11_MAPPED_SUBRESOURCE m;
            if (SUCCEEDED(g_ctx->Map(g_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m)))
            {
                memcpy(m.pData, &c, sizeof(c));
                g_ctx->Unmap(g_cb, 0);
            }
        }

        void FullscreenPass(ID3D11ShaderResourceView* src, float srcW, float srcH, Target& dst, float dx, float dy)
        {
            ID3D11ShaderResourceView* none = nullptr;
            g_ctx->PSSetShaderResources(0, 1, &none);
            g_ctx->OMSetRenderTargets(1, &dst.rtv, nullptr);

            Constants c{};
            c.rect[2] = c.screen[0] = static_cast<float>(g_w / 2 ? g_w / 2 : 1);
            c.rect[3] = c.screen[1] = static_cast<float>(g_h / 2 ? g_h / 2 : 1);
            c.blurDir[0] = dx * params.blur / srcW;
            c.blurDir[1] = dy * params.blur / srcH;
            Upload(c);

            g_ctx->PSSetShaderResources(0, 1, &src);
            g_ctx->Draw(4, 0);
        }

        bool Capture(ID3D11RenderTargetView* rtv)
        {
            ID3D11Resource*  res = nullptr;
            ID3D11Texture2D* bb  = nullptr;
            rtv->GetResource(&res);
            if (!res)
                return false;
            res->QueryInterface(IID_PPV_ARGS(&bb));
            res->Release();
            if (!bb)
                return false;

            D3D11_TEXTURE2D_DESC d;
            bb->GetDesc(&d);

            bool ok = EnsureTargets(d.Width, d.Height, d.Format);
            if (ok)
            {
                if (d.SampleDesc.Count > 1)
                    g_ctx->ResolveSubresource(g_scene.tex, 0, bb, 0, Typed(d.Format));
                else
                    g_ctx->CopyResource(g_scene.tex, bb);
            }
            bb->Release();
            if (!ok)
                return false;

            const float bw = static_cast<float>(g_w / 2 ? g_w / 2 : 1), bh = static_cast<float>(g_h / 2 ? g_h / 2 : 1);
            D3D11_VIEWPORT vp{ 0, 0, bw, bh, 0, 1 };
            D3D11_RECT     sc{ 0, 0, static_cast<LONG>(bw), static_cast<LONG>(bh) };
            g_ctx->RSSetViewports(1, &vp);
            g_ctx->RSSetScissorRects(1, &sc);
            g_ctx->OMSetBlendState(nullptr, nullptr, 0xffffffff);
            g_ctx->PSSetShader(g_psBlur, nullptr, 0);

            FullscreenPass(g_scene.srv, static_cast<float>(g_w), static_cast<float>(g_h), g_blurA, 1, 0);
            FullscreenPass(g_blurA.srv, bw, bh, g_blurB, 0, 1);
            FullscreenPass(g_blurB.srv, bw, bh, g_blurA, 1, 0);
            FullscreenPass(g_blurA.srv, bw, bh, g_blurB, 0, 1);
            return true;
        }

        void DrawCallback(const ImDrawList*, const ImDrawCmd* cmd)
        {
            const Job& j = *static_cast<const Job*>(cmd->UserCallbackData);

            ID3D11RenderTargetView* rtv = nullptr;
            ID3D11DepthStencilView* dsv = nullptr;
            g_ctx->OMGetRenderTargets(1, &rtv, &dsv);
            if (!rtv)
            {
                if (dsv) dsv->Release();
                return;
            }

            ID3D11BlendState* blend = nullptr;
            float             blendFactor[4];
            UINT              sampleMask;
            g_ctx->OMGetBlendState(&blend, blendFactor, &sampleMask);

            ID3D11Buffer* gamePsCb = nullptr;
            g_ctx->PSGetConstantBuffers(0, 1, &gamePsCb);

            g_ctx->IASetInputLayout(nullptr);
            g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            g_ctx->VSSetShader(g_vs, nullptr, 0);
            g_ctx->VSSetConstantBuffers(0, 1, &g_cb);
            g_ctx->PSSetConstantBuffers(0, 1, &g_cb);
            g_ctx->PSSetSamplers(0, 1, &g_smp);

            bool ready = j.frame == g_lastFrame;
            if (!ready && Capture(rtv))
            {
                g_lastFrame = j.frame;
                ready = true;
            }

            if (ready)
            {
                g_ctx->OMSetRenderTargets(1, &rtv, dsv);
                g_ctx->OMSetBlendState(blend, blendFactor, sampleMask);

                D3D11_VIEWPORT vp{ 0, 0, static_cast<float>(g_w), static_cast<float>(g_h), 0, 1 };
                D3D11_RECT sc{ static_cast<LONG>(cmd->ClipRect.x), static_cast<LONG>(cmd->ClipRect.y),
                               static_cast<LONG>(cmd->ClipRect.z), static_cast<LONG>(cmd->ClipRect.w) };
                g_ctx->RSSetViewports(1, &vp);
                g_ctx->RSSetScissorRects(1, &sc);

                const glass::Params& p = params;
                Constants c{};
                c.rect[0] = j.x0; c.rect[1] = j.y0; c.rect[2] = j.x1; c.rect[3] = j.y1;
                c.screen[0] = static_cast<float>(g_w); c.screen[1] = static_cast<float>(g_h);
                c.radius = j.radius; c.refract = p.refraction; c.noise = p.noise; c.glow = p.glow;
                c.tint[0] = p.tint.x; c.tint[1] = p.tint.y; c.tint[2] = p.tint.z; c.tint[3] = p.tint.w;
                Upload(c);

                g_ctx->PSSetShader(g_psGlass, nullptr, 0);
                g_ctx->PSSetShaderResources(0, 1, &g_blurB.srv);
                g_ctx->Draw(4, 0);

                ID3D11ShaderResourceView* none = nullptr;
                g_ctx->PSSetShaderResources(0, 1, &none);
            }
            else
            {
                g_ctx->OMSetRenderTargets(1, &rtv, dsv);
            }

            g_ctx->PSSetConstantBuffers(0, 1, &gamePsCb);
            if (gamePsCb) gamePsCb->Release();
            if (blend) blend->Release();
            rtv->Release();
            if (dsv) dsv->Release();
        }
    }
}

void RauchUi::Init()
{
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowPadding     = ImVec2(0, 0);
    st.FramePadding      = ImVec2(8, 4);
    st.ItemSpacing       = ImVec2(6, 4);
    st.ItemInnerSpacing  = ImVec2(6, 4);
    st.ScrollbarSize     = 4.0f;
    st.WindowRounding    = 0.0f;
    st.ChildRounding     = 0.0f;
    st.FrameRounding     = kRound;
    st.PopupRounding     = 0.0f;
    st.ScrollbarRounding = 0.0f;
    st.GrabRounding      = 0.0f;
    st.WindowBorderSize  = 1.0f;
    st.ChildBorderSize   = 0.0f;
    st.PopupBorderSize   = 1.0f;
    st.FrameBorderSize   = 1.0f;

    auto f4 = [](ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); };
    ImVec4* c = st.Colors;
    c[ImGuiCol_Text]                 = f4(palette::text);
    c[ImGuiCol_TextDisabled]         = f4(palette::textFaint);
    c[ImGuiCol_WindowBg]             = f4(palette::background);
    c[ImGuiCol_ChildBg]              = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg]              = f4(palette::card);
    c[ImGuiCol_Border]               = f4(palette::border);
    c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]              = f4(palette::secondary);
    c[ImGuiCol_FrameBgHovered]       = f4(palette::secondaryHover);
    c[ImGuiCol_FrameBgActive]        = f4(palette::secondaryHover);
    c[ImGuiCol_TitleBg]              = f4(palette::sidebar);
    c[ImGuiCol_TitleBgActive]        = f4(palette::sidebar);
    c[ImGuiCol_Button]               = f4(palette::secondary);
    c[ImGuiCol_ButtonHovered]        = f4(palette::secondaryHover);
    c[ImGuiCol_ButtonActive]         = f4(palette::secondaryActive);
    c[ImGuiCol_Header]               = f4(Alpha(palette::accent, 0.18f));
    c[ImGuiCol_HeaderHovered]        = f4(Alpha(palette::accent, 0.26f));
    c[ImGuiCol_HeaderActive]         = f4(Alpha(palette::accent, 0.34f));
    c[ImGuiCol_Separator]            = f4(palette::border);
    c[ImGuiCol_CheckMark]            = f4(palette::accent);
    c[ImGuiCol_SliderGrab]           = f4(palette::accent);
    c[ImGuiCol_SliderGrabActive]     = f4(palette::accent);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]        = f4(palette::track);
    c[ImGuiCol_ScrollbarGrabHovered] = f4(palette::textFaint);
    c[ImGuiCol_ScrollbarGrabActive]  = f4(palette::accent);
    c[ImGuiCol_TextSelectedBg]       = f4(Alpha(palette::accent, 0.40f));
    c[ImGuiCol_InputTextCursor]      = f4(palette::accent);
    c[ImGuiCol_NavCursor]            = ImVec4(0, 0, 0, 0);

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    auto load = [&](const char* file, float size) -> ImFont* {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "C:\\Windows\\Fonts\\%s", file);
        DWORD attr = GetFileAttributesA(path);
        ImFontConfig cfg;
        cfg.OversampleH = 3;
        cfg.OversampleV = 2;
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
            if (ImFont* f = io.Fonts->AddFontFromFileTTF(path, size, &cfg))
                return f;
        ImFontConfig fallback;
        fallback.SizePixels = size;
        return io.Fonts->AddFontDefault(&fallback);
    };
    fonts::regular  = load("segoeui.ttf", 14.0f);
    fonts::semibold = load("seguisb.ttf", 14.0f);
    fonts::mono     = load("consola.ttf", 12.5f);
    io.FontDefault  = fonts::regular;
}

ImU32 RauchUi::Alpha(ImU32 color, float alpha)
{
    float a = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF) * alpha;
    return (color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(ImClamp(a, 0.0f, 255.0f)) << IM_COL32_A_SHIFT);
}

ImU32 RauchUi::Mix(ImU32 from, ImU32 to, float t)
{
    return ImGui::ColorConvertFloat4ToU32(ImLerp(ImGui::ColorConvertU32ToFloat4(from), ImGui::ColorConvertU32ToFloat4(to), ImSaturate(t)));
}

float RauchUi::Fade(ImGuiID id, bool target, float speed)
{
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID key = ImHashStr("##fade", 0, id);
    float v = ImLerp(storage->GetFloat(key, 0.0f), target ? 1.0f : 0.0f, ImSaturate(ImGui::GetIO().DeltaTime * speed));
    storage->SetFloat(key, v);
    return v;
}

ImVec2 RauchUi::Measure(ImFont* font, const char* text, const char* end)
{
    if (!text)
        return ImVec2(0, 0);
    if (!font)
        font = ImGui::GetFont();
    return font->CalcTextSizeA(font->LegacySize, FLT_MAX, 0.0f, text, end);
}

void RauchUi::TextAt(ImDrawList* dl, ImFont* font, ImVec2 pos, ImU32 color, const char* text, const char* end)
{
    if (!text)
        return;
    if (!font)
        font = ImGui::GetFont();
    dl->AddText(font, font->LegacySize, ImVec2(IM_ROUND(pos.x), IM_ROUND(pos.y)), color, text, end);
}

void RauchUi::Gradient(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 from, ImU32 to, float rounding, bool vertical)
{
    if (max.x <= min.x || max.y <= min.y)
        return;
    const int start = dl->VtxBuffer.Size;
    dl->AddRectFilled(min, max, IM_COL32_WHITE, rounding);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, start, dl->VtxBuffer.Size, min, vertical ? ImVec2(min.x, max.y) : ImVec2(max.x, min.y), from, to);
    const float a = static_cast<float>((from >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
    if (a < 1.0f)
        for (int i = start; i < dl->VtxBuffer.Size; ++i)
            dl->VtxBuffer[i].col = Alpha(dl->VtxBuffer[i].col, a);
}

void RauchUi::FadeIn(ImDrawList* dl, int start, double started)
{
    float p = ImSaturate(static_cast<float>(ImGui::GetTime() - started) / 0.16f);
    if (p >= 1.0f || start < 0 || start >= dl->VtxBuffer.Size)
        return;
    float e = 1.0f - (1.0f - p) * (1.0f - p);
    for (int i = start; i < dl->VtxBuffer.Size; ++i)
    {
        ImDrawVert& v = dl->VtxBuffer[i];
        float a = static_cast<float>((v.col & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT);
        v.col = (v.col & ~IM_COL32_A_MASK) | (static_cast<ImU32>(a * e) << IM_COL32_A_SHIFT);
        v.pos.y += (1.0f - e) * 4.0f;
    }
}

bool RauchUi::BeginWindow(const char* title, bool* open, ImVec2 size, const char* const* tabs, int tabCount, int* tab)
{
    if (g_current || !title || (open && !*open))
        return false;

    WindowState& w = g_windows[ImHashStr(title)];
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    size = ImVec2(ImMax(size.x, 120.0f), ImMax(size.y, 120.0f));
    if (!w.positioned && ds.x > 0.0f && ds.y > 0.0f)
    {
        w.positioned = true;
        w.pos = ImVec2((ds.x - size.x) * 0.5f, (ds.y - size.y) * 0.5f);
    }
    w.pos.x = ImClamp(w.pos.x, 0.0f, ImMax(ds.x - size.x, 0.0f));
    w.pos.y = ImClamp(w.pos.y, 0.0f, ImMax(ds.y - size.y, 0.0f));

    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(w.pos, ImGuiCond_Always);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin(title, nullptr, flags);
    ImGui::PopStyleVar();
    g_current = &w;

    const ImVec2 a = ImGui::GetWindowPos(), b(a.x + size.x, a.y + size.y);
    ImDrawList*  dl = ImGui::GetWindowDrawList();
    const bool   glassy = settings.glassBackground;
    if (glassy)
        glass::Panel(dl, a, b, 0.0f);
    dl->AddRectFilled(a, b, Alpha(palette::sidebar, glassy ? 0.6f : 1.0f));
    dl->AddRect(a, b, palette::border);

    const char* titleEnd = LabelEnd(title);
    ImVec2 ts = Measure(fonts::semibold, title, titleEnd);
    TextAt(dl, fonts::semibold, ImVec2(a.x + 12.0f, a.y + (kTitleH - ts.y) * 0.5f), palette::text, title, titleEnd);
    Gradient(dl, ImVec2(a.x + 1.0f, a.y + kTitleH - 1.0f), ImVec2(b.x - 1.0f, a.y + kTitleH + 1.0f), palette::accent, palette::accent2);

    if (open)
    {
        const float  cs = 20.0f;
        const ImVec2 cmin(b.x - cs - 6.0f, a.y + (kTitleH - cs) * 0.5f);
        ImGui::SetCursorScreenPos(cmin);
        if (ImGui::InvisibleButton("##close", ImVec2(cs, cs)))
            *open = false;
        bool hovered = ImGui::IsItemHovered();
        if (hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImVec2 c(cmin.x + cs * 0.5f, cmin.y + cs * 0.5f);
        ImU32  x = hovered ? palette::accent : palette::textDim;
        dl->AddLine(ImVec2(c.x - 4.0f, c.y - 4.0f), ImVec2(c.x + 4.0f, c.y + 4.0f), x, 1.4f);
        dl->AddLine(ImVec2(c.x + 4.0f, c.y - 4.0f), ImVec2(c.x - 4.0f, c.y + 4.0f), x, 1.4f);
    }

    const bool   hasTabs = tabs && tabCount > 0 && tab;
    const float  top = a.y + kTitleH + 8.0f;
    const ImVec2 pmin(a.x + kEdge, hasTabs ? top + kTabH : top), pmax(b.x - kEdge, b.y - kEdge);
    dl->AddRectFilled(pmin, pmax, Alpha(palette::background, glassy ? 0.75f : 1.0f));
    dl->AddRect(pmin, pmax, palette::border);

    int key = 0;
    if (hasTabs)
    {
        ImGui::SetCursorScreenPos(ImVec2(a.x + kEdge, top));
        Tabs(tabs, tabCount, tab, kTabH);
        key = *tab;
    }
    if (key != w.shownTab)
    {
        w.shownTab = key;
        w.tabStarted = ImGui::GetTime();
    }

    ImGui::SetCursorScreenPos(ImVec2(pmin.x + 12.0f, pmin.y + 12.0f));
    ImGui::PushID(key);
    ImGui::BeginChild("##body", ImVec2(ImMax(pmax.x - pmin.x - 20.0f, 1.0f), ImMax(pmax.y - pmin.y - 20.0f, 1.0f)), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground);
    g_bodyStart = ImGui::GetWindowDrawList()->VtxBuffer.Size;
    ImFont* font = Font(fonts::regular);
    ImGui::PushFont(font, font->LegacySize);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(palette::textBody));
    return true;
}

void RauchUi::EndWindow()
{
    if (!g_current)
        return;

    AbortGroup();
    g_area.active = false;
    ImGui::PopStyleColor();
    ImGui::PopFont();
    FadeIn(ImGui::GetWindowDrawList(), g_bodyStart, g_current->tabStarted);
    ImGui::EndChild();
    ImGui::PopID();

    const ImVec2 wp = ImGui::GetWindowPos();
    const float  ww = ImGui::GetWindowWidth();
    if (!ImGui::IsAnyItemHovered() && ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(wp, ImVec2(wp.x + ww, wp.y + kTitleH)) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        g_current->moving = true;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        g_current->moving = false;
    if (g_current->moving)
    {
        g_current->pos.x += ImGui::GetIO().MouseDelta.x;
        g_current->pos.y += ImGui::GetIO().MouseDelta.y;
    }

    ImGui::End();
    g_current = nullptr;
}

bool RauchUi::Tabs(const char* const* items, int count, int* selected, float height)
{
    if (!items || count <= 0 || !selected)
        return false;
    *selected = ImClamp(*selected, 0, count - 1);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 start = pos;
    bool changed = false;
    for (int i = 0; i < count; ++i)
    {
        const char* item = items[i] ? items[i] : "";
        ImVec2 ts = Measure(fonts::regular, item);
        ImVec2 min = pos, max(pos.x + ts.x + 26.0f, pos.y + height);
        ImGui::PushID(i);
        ImGui::SetCursorScreenPos(min);
        if (ImGui::InvisibleButton("##tab", ImVec2(max.x - min.x, height)) && *selected != i)
        {
            *selected = i;
            changed = true;
        }
        bool active = *selected == i;
        if (!active)
            Hand();
        float t = Fade(ImGui::GetItemID(), ImGui::IsItemHovered());
        ImGui::PopID();

        if (active)
        {
            dl->AddRectFilled(min, ImVec2(max.x, max.y + 1.0f), palette::background);
            dl->AddRect(min, ImVec2(max.x, max.y + 2.0f), palette::border);
            dl->AddRectFilled(ImVec2(min.x + 1.0f, max.y), ImVec2(max.x - 1.0f, max.y + 2.0f), palette::background);
            Gradient(dl, ImVec2(min.x, min.y), ImVec2(max.x, min.y + 2.0f), palette::accent, palette::accent2);
        }
        else
        {
            dl->AddRectFilled(ImVec2(min.x, min.y + 2.0f), max, Mix(palette::secondaryActive, palette::secondary, t));
        }
        TextAt(dl, fonts::regular, ImVec2(min.x + 13.0f, min.y + (height - ts.y) * 0.5f + (active ? 0.0f : 1.0f)),
                 active ? palette::text : Mix(palette::textDim, palette::textBody, t), item);
        pos.x = max.x + 2.0f;
    }
    ImGui::SetCursorScreenPos(start);
    ImGui::Dummy(ImVec2(pos.x - start.x, height));
    return changed;
}

float RauchUi::RowWidth()
{
    return ImMax(g_frame.active ? g_frame.width : ImGui::GetContentRegionAvail().x, 1.0f);
}

void RauchUi::BeginGroups()
{
    g_area.active     = true;
    g_area.origin     = ImGui::GetCursorScreenPos();
    g_area.width      = ImGui::GetContentRegionAvail().x;
    g_area.columns    = g_area.width > 360.0f ? 2 : 1;
    g_area.columnY[0] = 0.0f;
    g_area.columnY[1] = 0.0f;
}

void RauchUi::EndGroups()
{
    g_area.active = false;
    ImGui::SetCursorScreenPos(g_area.origin);
    ImGui::Dummy(ImVec2(g_area.width, ImMax(ImMax(g_area.columnY[0], g_area.columnY[1]) - kGroupGap, 0.0f)));
}

void RauchUi::BeginGroup(const char* title)
{
    if (g_frame.active)
        EndGroup();
    if (!title)
        title = "";
    g_frame.active = true;
    g_frame.title  = title;

    if (g_area.active)
    {
        int   column = (g_area.columns == 2 && g_area.columnY[1] < g_area.columnY[0]) ? 1 : 0;
        float w = (g_area.width - kGroupGap * (g_area.columns - 1)) / g_area.columns;
        g_frame.min    = ImVec2(g_area.origin.x + column * (w + kGroupGap), g_area.origin.y + g_area.columnY[column]);
        g_frame.width  = w;
        g_frame.column = column;
    }
    else
    {
        g_frame.min    = ImGui::GetCursorScreenPos();
        g_frame.width  = ImGui::GetContentRegionAvail().x;
        g_frame.column = -1;
    }

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    const char* end = LabelEnd(title);
    ImVec2 ts = Measure(fonts::semibold, title, end);
    TextAt(dl, fonts::semibold, g_frame.min, palette::text, title, end);
    const float ly = g_frame.min.y + ts.y + 4.0f;
    dl->AddRectFilledMultiColor(ImVec2(g_frame.min.x, ly), ImVec2(g_frame.min.x + g_frame.width, ly + 1.0f), palette::accent,
                                Alpha(palette::accent2, 0.0f), Alpha(palette::accent2, 0.0f), palette::accent);

    ImGui::SetCursorScreenPos(ImVec2(g_frame.min.x, ly + 9.0f));
    ImGui::BeginGroup();
    ImGui::PushID(title);
}

void RauchUi::EndGroup()
{
    if (!g_frame.active)
        return;
    ImGui::PopID();
    ImGui::EndGroup();

    const float bottom = ImGui::GetItemRectMax().y;
    if (g_frame.column >= 0)
    {
        g_area.columnY[g_frame.column] = bottom - g_area.origin.y + kGroupGap;
    }
    else
    {
        ImGui::SetCursorScreenPos(ImVec2(g_frame.min.x, bottom + kGroupGap));
        ImGui::Dummy(ImVec2(0, 0));
    }
    g_frame.active = false;
}

void RauchUi::Separator(const char* label)
{
    if (!label)
        label = "";
    const float  width = RowWidth();
    const ImVec2 pos   = ImGui::GetCursorScreenPos();
    ImDrawList*  dl    = ImGui::GetWindowDrawList();
    ImVec2 ts = Measure(fonts::regular, label);
    float  cy = pos.y + kRowH * 0.5f, cx = pos.x + width * 0.5f;
    dl->AddRectFilledMultiColor(ImVec2(pos.x, cy), ImVec2(cx - ts.x * 0.5f - 8.0f, cy + 1.0f), Alpha(palette::border, 0.0f), palette::border, palette::border, Alpha(palette::border, 0.0f));
    dl->AddRectFilledMultiColor(ImVec2(cx + ts.x * 0.5f + 8.0f, cy), ImVec2(pos.x + width, cy + 1.0f), palette::border, Alpha(palette::border, 0.0f), Alpha(palette::border, 0.0f), palette::border);
    TextAt(dl, fonts::regular, ImVec2(cx - ts.x * 0.5f, cy - ts.y * 0.5f), palette::textDim, label);
    ImGui::Dummy(ImVec2(width, kRowH));
}

bool RauchUi::Checkbox(const char* label, bool* value, ImVec4* color)
{
    if (!label || !value)
        return false;
    ImGui::PushID(label);
    const float  width = RowWidth();
    const ImVec2 pos   = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton("##check", ImVec2(ImMax(color ? width - 28.0f : width, 1.0f), kRowH));
    Hand();
    ImGuiID id = ImGui::GetItemID();
    float hover = Fade(id, ImGui::IsItemHovered());
    if (pressed)
        *value = !*value;
    float on = Fade(ImHashStr("##on", 0, id), *value, 20.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float box = 14.0f;
    ImVec2 bmin(pos.x, pos.y + (kRowH - box) * 0.5f), bmax(bmin.x + box, bmin.y + box);
    dl->AddRectFilled(bmin, bmax, palette::sidebar);
    if (on > 0.01f)
        Gradient(dl, ImVec2(bmin.x + 1.0f, bmin.y + 1.0f), ImVec2(bmax.x - 1.0f, bmax.y - 1.0f), Alpha(palette::accent, on), Alpha(palette::accent2, on));
    dl->AddRect(bmin, bmax, Mix(palette::border, palette::textFaint, hover));

    const char* end = LabelEnd(label);
    ImVec2 ts = Measure(fonts::regular, label, end);
    TextAt(dl, fonts::regular, ImVec2(bmax.x + 8.0f, pos.y + (kRowH - ts.y) * 0.5f), Mix(palette::textBody, palette::text, ImMax(on, hover)), label, end);

    if (color)
        Swatch("color", ImVec2(pos.x + width - 20.0f, pos.y + (kRowH - 11.0f) * 0.5f), color);

    ImGui::SetCursorScreenPos(pos);
    ImGui::Dummy(ImVec2(width, kRowH));
    ImGui::PopID();
    return pressed;
}

bool RauchUi::Slider(const char* label, float* value, float min, float max, const char* format)
{
    if (!label || !value)
        return false;
    format = SafeFloatFormat(format, "%.2f");
    const float barY = 19.0f, barH = 6.0f, height = barY + barH + 4.0f;
    const float  width = RowWidth();
    const ImVec2 pos   = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(label, ImVec2(width, height));
    Hand();
    ImGuiID id = ImGui::GetItemID();
    bool held = ImGui::IsItemActive();
    float t = Fade(id, ImGui::IsItemHovered() || held);

    bool changed = false;
    if (held && max > min)
    {
        float next = min + (max - min) * ImSaturate((ImGui::GetIO().MousePos.x - pos.x) / width);
        if (next != *value)
        {
            *value = next;
            changed = true;
        }
    }

    float target = max > min ? ImSaturate((*value - min) / (max - min)) : 0.0f;
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID glideKey = ImHashStr("##glide", 0, id);
    float shown = ImLerp(storage->GetFloat(glideKey, target), target, ImSaturate(ImGui::GetIO().DeltaTime * 20.0f));
    storage->SetFloat(glideKey, shown);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const char* end = LabelEnd(label);
    TextAt(dl, fonts::regular, pos, Mix(palette::textBody, palette::text, t), label, end);

    ImVec2 bmin(pos.x, pos.y + barY), bmax(pos.x + width, pos.y + barY + barH);
    dl->AddRectFilled(bmin, bmax, palette::sidebar);
    Gradient(dl, ImVec2(bmin.x + 1.0f, bmin.y + 1.0f), ImVec2(bmin.x + 1.0f + (width - 2.0f) * shown, bmax.y - 1.0f), palette::accent, palette::accent2);
    dl->AddRect(bmin, bmax, Mix(palette::border, palette::textFaint, t));

    char text[64];
    snprintf(text, sizeof(text), format, *value);
    ImVec2 vs = Measure(fonts::regular, text);
    TextAt(dl, fonts::regular, ImVec2(pos.x + width - vs.x, pos.y), Mix(palette::textDim, palette::text, t), text);
    return changed;
}

bool RauchUi::SliderInt(const char* label, int* value, int min, int max)
{
    if (!value)
        return false;
    float f = static_cast<float>(*value);
    Slider(label, &f, static_cast<float>(min), static_cast<float>(max), "%.0f");
    int next = static_cast<int>(lroundf(f));
    bool changed = next != *value;
    *value = next;
    return changed;
}

bool RauchUi::Dropdown(const char* label, int* selected, const char* const* items, int count)
{
    if (!label || !selected)
        return false;
    if (!items || count < 0)
        count = 0;
    ImGui::PushID(label);
    const float  width = RowWidth();
    const char*  end   = LabelEnd(label);
    const bool   titled = end != label;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (titled)
    {
        TextAt(dl, fonts::regular, pos, palette::textBody, label, end);
        pos.y += Measure(fonts::regular, label, end).y + 3.0f;
    }

    ImGui::SetCursorScreenPos(pos);
    bool pressed = ImGui::InvisibleButton("##field", ImVec2(width, kFieldH));
    Hand();
    bool open = ImGui::IsPopupOpen("##list");
    float t = Fade(ImGui::GetItemID(), ImGui::IsItemHovered() || open);
    if (pressed && count > 0)
        ImGui::OpenPopup("##list");

    ImVec2 bmin = pos, bmax(pos.x + width, pos.y + kFieldH);
    Field(dl, bmin, bmax, t, open);
    bool valid = *selected >= 0 && *selected < count;
    const char* preview = valid && items[*selected] ? items[*selected] : "None";
    ImVec2 ps = Measure(fonts::regular, preview);
    TextAt(dl, fonts::regular, ImVec2(bmin.x + 8.0f, bmin.y + (kFieldH - ps.y) * 0.5f), valid ? palette::text : palette::textFaint, preview);
    Chevron(dl, ImVec2(bmax.x - 12.0f, (bmin.y + bmax.y) * 0.5f), open, Mix(palette::textDim, palette::text, t));

    ImGui::SetNextWindowPos(ImVec2(bmin.x, bmax.y + 3.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0), ImVec2(width, 240.0f));
    PushPopupStyle(3.0f, 1.0f);

    bool changed = false;
    if (ImGui::BeginPopup("##list", ImGuiWindowFlags_NoMove))
    {
        ImDrawList* pdl = ImGui::GetWindowDrawList();
        float itemW = ImGui::GetContentRegionAvail().x;
        for (int i = 0; i < count; ++i)
        {
            ImGui::PushID(i);
            ImVec2 imin = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##item", ImVec2(itemW, 22.0f)))
            {
                changed = *selected != i;
                *selected = i;
                ImGui::CloseCurrentPopup();
            }
            Hand();
            float ht = Fade(ImGui::GetItemID(), ImGui::IsItemHovered());
            bool  sel = *selected == i;
            ImVec2 imax(imin.x + itemW, imin.y + 22.0f);
            if (ht > 0.01f)
                pdl->AddRectFilled(imin, imax, Alpha(palette::highlight, ht));
            if (sel)
                Gradient(pdl, ImVec2(imin.x, imin.y + 5.0f), ImVec2(imin.x + 2.0f, imax.y - 5.0f), palette::accent, palette::accent2, 0.0f, true);
            const char* item = items[i] ? items[i] : "";
            ImVec2 is = Measure(fonts::regular, item);
            TextAt(pdl, fonts::regular, ImVec2(imin.x + 9.0f, imin.y + (22.0f - is.y) * 0.5f), sel ? palette::accent : Mix(palette::textBody, palette::text, ht), item);
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
    PopPopupStyle();

    ImGui::SetCursorScreenPos(ImVec2(pos.x, bmax.y + 2.0f));
    ImGui::Dummy(ImVec2(width, 0.0f));
    ImGui::PopID();
    return changed;
}

bool RauchUi::Keybind(const char* label, ImGuiKey* key)
{
    static ImGuiID listening = 0;
    if (!label || !key)
        return false;

    ImGui::PushID(label);
    const float  width = RowWidth();
    const ImVec2 pos   = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton("##row", ImVec2(width, kRowH));
    Hand();
    ImGuiID id = ImGui::GetItemID();
    bool hovered = ImGui::IsItemHovered();
    float t = Fade(id, hovered);
    if (pressed)
        listening = id;

    bool changed = false;
    if (listening == id && !pressed)
    {
        for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_GamepadStart; ++k)
        {
            if (!ImGui::IsKeyPressed(static_cast<ImGuiKey>(k), false))
                continue;
            ImGuiKey next = k == ImGuiKey_Escape ? ImGuiKey_None : static_cast<ImGuiKey>(k);
            changed = next != *key;
            *key = next;
            listening = 0;
            break;
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !hovered)
            listening = 0;
    }
    bool waiting = listening == id;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const char* end = LabelEnd(label);
    ImVec2 ls = Measure(fonts::regular, label, end);
    TextAt(dl, fonts::regular, ImVec2(pos.x, pos.y + (kRowH - ls.y) * 0.5f), Mix(palette::textBody, palette::text, t), label, end);

    char shown[40];
    snprintf(shown, sizeof(shown), "[%s]", waiting ? "..." : *key == ImGuiKey_None ? "NONE" : ImGui::GetKeyName(*key));
    for (char* c = shown; *c; ++c)
        *c = static_cast<char>(toupper(static_cast<unsigned char>(*c)));
    ImVec2 ss = Measure(fonts::mono, shown);
    TextAt(dl, fonts::mono, ImVec2(pos.x + width - ss.x, pos.y + (kRowH - ss.y) * 0.5f), waiting ? palette::accent : Mix(palette::textDim, palette::text, t), shown);

    ImGui::PopID();
    return changed;
}

bool RauchUi::ColorRow(const char* label, ImVec4* color)
{
    if (!label || !color)
        return false;
    ImGui::PushID(label);
    const float  width = RowWidth();
    const ImVec2 pos   = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const char* end = LabelEnd(label);
    ImVec2 ls = Measure(fonts::regular, label, end);
    TextAt(dl, fonts::regular, ImVec2(pos.x, pos.y + (kRowH - ls.y) * 0.5f), palette::textBody, label, end);
    bool changed = Swatch("color", ImVec2(pos.x + width - 20.0f, pos.y + (kRowH - 11.0f) * 0.5f), color);
    ImGui::SetCursorScreenPos(pos);
    ImGui::Dummy(ImVec2(width, kRowH));
    ImGui::PopID();
    return changed;
}

bool RauchUi::TextBox(const char* id, char* buffer, size_t size, const char* hint, float width)
{
    if (!id || !buffer || size == 0)
        return false;
    buffer[size - 1] = '\0';
    ImFont* font = Font(fonts::regular);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, ImMax((kFieldH - font->LegacySize) * 0.5f, 1.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kRound);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushFont(font, font->LegacySize);

    ImGui::SetNextItemWidth(width > 0.0f ? width : RowWidth());
    bool enter = ImGui::InputTextWithHint(id, hint ? hint : "", buffer, size, ImGuiInputTextFlags_EnterReturnsTrue);
    if (ImGui::IsItemActive())
        ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), palette::accent, kRound);
    if (enter)
        ImGui::SetKeyboardFocusHere(-1);

    ImGui::PopFont();
    ImGui::PopStyleVar(3);
    return enter;
}

bool RauchUi::Button(const char* label, Kind kind, float width)
{
    if (!label)
        return false;
    const float  w   = width > 0.0f ? width : RowWidth();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(label, ImVec2(w, kFieldH));
    Hand();
    bool held = ImGui::IsItemActive();
    float t = Fade(ImGui::GetItemID(), ImGui::IsItemHovered());

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 max(pos.x + w, pos.y + kFieldH);
    ImU32 text = Mix(palette::textBody, palette::text, t);
    switch (kind)
    {
    case Kind::Primary:
        Gradient(dl, pos, max, held ? palette::accentActive : Mix(palette::accent, palette::accentHover, t), palette::accent2, kRound);
        text = palette::onAccent;
        break;
    case Kind::Danger:
        dl->AddRectFilled(pos, max, Alpha(palette::negative, held ? 0.28f : ImLerp(0.10f, 0.18f, t)), kRound);
        dl->AddRect(pos, max, Alpha(palette::negative, ImLerp(0.45f, 0.8f, t)), kRound);
        text = Mix(palette::negative, palette::text, 0.3f);
        break;
    default:
        dl->AddRectFilled(pos, max, held ? palette::secondaryActive : Mix(palette::secondary, palette::secondaryHover, t), kRound);
        dl->AddRect(pos, max, Mix(palette::border, Mix(palette::accent, palette::accent2, 0.5f), t * 0.8f), kRound);
        break;
    }
    const char* end = LabelEnd(label);
    ImVec2 ts = Measure(fonts::regular, label, end);
    TextAt(dl, fonts::regular, ImVec2(pos.x + (w - ts.x) * 0.5f, pos.y + (kFieldH - ts.y) * 0.5f), text, label, end);
    return pressed;
}

void RauchUi::Value(const char* label, const char* value)
{
    if (!label)
        return;
    if (!value)
        value = "";
    const float  width = RowWidth();
    const ImVec2 pos   = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 ls = Measure(fonts::regular, label), vs = Measure(fonts::mono, value);
    TextAt(dl, fonts::regular, ImVec2(pos.x, pos.y + (kRowH - ls.y) * 0.5f), palette::textBody, label);
    TextAt(dl, fonts::mono, ImVec2(pos.x + width - vs.x, pos.y + (kRowH - vs.y) * 0.5f), palette::textDim, value);
    ImGui::Dummy(ImVec2(width, kRowH));
}

void RauchUi::Watermark(bool visible, const char* const* segments, int count)
{
    static float progress = 0.0f;
    const ImGuiIO& io = ImGui::GetIO();
    progress = ImSaturate(progress + io.DeltaTime / (visible ? 0.35f : -0.25f));
    if (progress <= 0.0f || count <= 0 || !segments)
        return;

    const float margin = 14.0f, height = 30.0f, pad = 14.0f, spacing = 10.0f;
    float e = progress < 0.5f ? 4.0f * progress * progress * progress : 1.0f - powf(-2.0f * progress + 2.0f, 3.0f) * 0.5f;

    float width = pad * 2.0f + 12.0f;
    for (int i = 0; i < count; ++i)
        width += Measure(i == 0 ? fonts::semibold : fonts::regular, segments[i] ? segments[i] : "").x + (i > 0 ? spacing * 2.0f + 1.0f : 0.0f);
    width = IM_ROUND(width);

    const float y = IM_ROUND(margin - (1.0f - e) * (height + margin + 8.0f));
    const ImVec2 min(IM_ROUND((io.DisplaySize.x - width) * 0.5f), y), max(min.x + width, min.y + height);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    glass::Panel(dl, min, max, height * 0.5f);
    dl->AddRect(min, max, IM_COL32(255, 255, 255, 40), height * 0.5f);

    float cy = (min.y + max.y) * 0.5f, x = min.x + pad;
    dl->AddCircleFilled(ImVec2(x + 3.0f, cy), 3.0f, palette::accent);
    x += 12.0f;
    for (int i = 0; i < count; ++i)
    {
        if (i > 0)
        {
            x += spacing;
            dl->AddRectFilled(ImVec2(x, cy - 6.0f), ImVec2(x + 1.0f, cy + 6.0f), IM_COL32(255, 255, 255, 60));
            x += 1.0f + spacing;
        }
        ImFont* f = Font(i == 0 ? fonts::semibold : fonts::regular);
        const char* segment = segments[i] ? segments[i] : "";
        ImVec2 s = Measure(f, segment);
        TextAt(dl, f, ImVec2(x, cy - s.y * 0.5f), IM_COL32_WHITE, segment);
        x += s.x;
    }
}

void RauchUi::Notify(Tone tone, const char* format, ...)
{
    if (!format)
        return;
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    std::lock_guard lock(g_toastLock);
    if (g_pending.size() >= 64)
        g_pending.erase(g_pending.begin());
    g_pending.push_back({ tone, buf, 4.0f });
}

void RauchUi::DrawNotifications()
{
    const float  width = 240.0f, margin = 16.0f, gap = 6.0f, padX = 12.0f, padTop = 9.0f, padBottom = 8.0f;
    const float  barGap = 7.0f, barH = 2.0f;
    const double enterTime = 0.16, exitTime = 0.12;

    const double now = ImGui::GetTime();
    {
        std::lock_guard lock(g_toastLock);
        for (Toast& t : g_pending)
        {
            t.start = now;
            g_toasts.push_front(std::move(t));
        }
        g_pending.clear();
    }
    if (g_toasts.empty())
        return;

    ImDrawList*  dl = ImGui::GetForegroundDrawList();
    ImFont*      font = Font(fonts::regular);
    const float  size = font->LegacySize;
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    const float  dt = ImGui::GetIO().DeltaTime;
    const float  wrap = width - padX * 2.0f - 4.0f, barW = width - padX * 2.0f;

    float targetY = margin;
    for (size_t i = 0; i < g_toasts.size(); ++i)
    {
        Toast& t = g_toasts[i];
        float elapsed = static_cast<float>(now - t.start);
        if (t.exit < 0.0 && (elapsed >= t.length || i >= 6))
            t.exit = now;

        ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, wrap, t.text.c_str());
        float  h = padTop + ts.y + barGap + barH + padBottom;
        t.y = t.y < 0.0f ? targetY : ImLerp(t.y, targetY, ImSaturate(dt * 26.0f));
        targetY += h + gap;

        float enter = ImSaturate(static_cast<float>(elapsed / enterTime));
        enter = 1.0f - powf(1.0f - enter, 4.0f);
        float exitT = t.exit < 0.0 ? 0.0f : ImSaturate(static_cast<float>((now - t.exit) / exitTime));
        exitT *= exitT;
        float a = enter * (1.0f - exitT);
        float ox = ((1.0f - enter) + exitT) * (width + margin);
        float progress = t.exit < 0.0 ? 1.0f - ImSaturate(elapsed / t.length) : 0.0f;
        ImU32 from = t.tone == Tone::Warning ? palette::warning : t.tone == Tone::Error ? palette::negative
                   : t.tone == Tone::Success ? palette::positive : palette::accent;
        ImU32 to = t.tone == Tone::Info ? palette::accent2 : from;

        ImVec2 min(IM_ROUND(ds.x - margin - width + ox), IM_ROUND(t.y)), max(min.x + width, min.y + h);
        dl->AddRectFilled(min, max, Alpha(palette::card, a));
        dl->AddRect(min, max, Alpha(palette::border, a));
        dl->AddText(font, size, ImVec2(IM_ROUND(min.x + padX), IM_ROUND(min.y + padTop)), Alpha(palette::text, a), t.text.c_str(), nullptr, wrap);

        ImVec2 bar(min.x + padX, max.y - padBottom - barH);
        dl->AddRectFilled(bar, ImVec2(bar.x + barW, bar.y + barH), Alpha(palette::track, a), 1.0f);
        if (progress > 0.0f)
            Gradient(dl, bar, ImVec2(bar.x + barW * progress, bar.y + barH), Alpha(from, a), Alpha(to, a), 1.0f);
        Gradient(dl, ImVec2(min.x, min.y), ImVec2(max.x, min.y + 2.0f), Alpha(from, a), Alpha(to, a));
    }

    for (auto it = g_toasts.begin(); it != g_toasts.end();)
        it = it->exit >= 0.0 && now - it->exit >= exitTime ? g_toasts.erase(it) : it + 1;
}

bool RauchUi::glass::Init(ID3D11Device* device, ID3D11DeviceContext* context)
{
    g_dev = device;
    g_ctx = context;

    ID3DBlob* vs = Compile("vs", "vs_4_0");
    ID3DBlob* pb = Compile("ps_blur", "ps_4_0");
    ID3DBlob* pg = Compile("ps_glass", "ps_4_0");

    bool ok = vs && pb && pg &&
        SUCCEEDED(g_dev->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &g_vs)) &&
        SUCCEEDED(g_dev->CreatePixelShader(pb->GetBufferPointer(), pb->GetBufferSize(), nullptr, &g_psBlur)) &&
        SUCCEEDED(g_dev->CreatePixelShader(pg->GetBufferPointer(), pg->GetBufferSize(), nullptr, &g_psGlass));

    if (vs) vs->Release();
    if (pb) pb->Release();
    if (pg) pg->Release();

    if (ok)
    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(Constants);
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ok = SUCCEEDED(g_dev->CreateBuffer(&bd, nullptr, &g_cb));
    }
    if (ok)
    {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        ok = SUCCEEDED(g_dev->CreateSamplerState(&sd, &g_smp));
    }

    if (!ok)
        Shutdown();
    return ok;
}

void RauchUi::glass::Shutdown()
{
    g_scene.Release(); g_blurA.Release(); g_blurB.Release();
    if (g_smp)     { g_smp->Release();     g_smp = nullptr; }
    if (g_cb)      { g_cb->Release();      g_cb = nullptr; }
    if (g_psGlass) { g_psGlass->Release(); g_psGlass = nullptr; }
    if (g_psBlur)  { g_psBlur->Release();  g_psBlur = nullptr; }
    if (g_vs)      { g_vs->Release();      g_vs = nullptr; }
    g_dev = nullptr;
    g_ctx = nullptr;
    g_w = g_h = 0;
    g_lastFrame = -1;
}

void RauchUi::glass::Panel(ImDrawList* dl, ImVec2 min, ImVec2 max, float radius)
{
    if (!g_psGlass || !params.enabled)
    {
        dl->AddRectFilled(min, max, IM_COL32(18, 16, 22, 235), radius);
        return;
    }

    Job j{ min.x, min.y, max.x, max.y, radius, ImGui::GetFrameCount() };
    dl->AddCallback(DrawCallback, &j, sizeof(j));
    dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, nullptr);
}
