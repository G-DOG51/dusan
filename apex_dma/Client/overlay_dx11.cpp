// DirectX11 + ImGui Overlay & Settings Menu for Windows
// Replaces the original GLFW/X11 Linux overlay

#include "overlay_dx11.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "../apex_sky.h"
#include "../vector.h"
#include "../Game.h"

#include <d3d11.h>
#include <dwmapi.h>
#include <cstdio>
#include <string>
#include <vector>
#include <mutex>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

// External globals from apex_dma.cpp
extern bool overlay_t;
extern bool valid;
extern bool next2;
extern Vector esp_local_pos;
extern std::vector<player> players;
extern std::vector<Entity> spectators, allied_spectators;
extern std::mutex spectatorsMtx;
extern GlobalVar globals;
extern AimAssist aimbot;
extern uint64_t g_Base;

// Originally defined in Client/main.cpp
Vector aim_target = Vector(0, 0, 0);

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// D3D11 globals
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;
static HWND                     g_hwnd = nullptr;
static HWND                     g_gameHwnd = nullptr;
static bool                     g_showMenu = true;
static int                      g_windowWidth = 0;
static int                      g_windowHeight = 0;
static int                      g_windowX = 0;
static int                      g_windowY = 0;

static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void CreateRenderTarget();
static void CleanupRenderTarget();
static void UpdateWindowData();
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd,
        &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd,
            &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;
    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain)       { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice)       { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

static void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer)
    {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_CREATE:
    {
        MARGINS margin = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(hWnd, &margin);
        return 0;
    }
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void UpdateWindowData()
{
    if (!g_gameHwnd || !IsWindow(g_gameHwnd))
    {
        g_gameHwnd = FindWindowA("Respawn001", nullptr);
        if (!g_gameHwnd)
            return;
    }

    RECT rect;
    if (!GetClientRect(g_gameHwnd, &rect))
        return;

    POINT pt = { rect.left, rect.top };
    ClientToScreen(g_gameHwnd, &pt);

    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;

    if (w != g_windowWidth || h != g_windowHeight || pt.x != g_windowX || pt.y != g_windowY)
    {
        g_windowWidth = w;
        g_windowHeight = h;
        g_windowX = pt.x;
        g_windowY = pt.y;
        SetWindowPos(g_hwnd, HWND_TOPMOST, g_windowX, g_windowY, g_windowWidth, g_windowHeight, SWP_NOACTIVATE);
    }

    // Click-through toggle (matches reference OS-ImGui_External.cpp UpdateWindowData)
    // When ImGui wants mouse input, remove WS_EX_LAYERED so we can interact
    // Otherwise add WS_EX_LAYERED so clicks pass through to the game
    if (g_showMenu && ImGui::GetIO().WantCaptureMouse)
        SetWindowLong(g_hwnd, GWL_EXSTYLE, GetWindowLong(g_hwnd, GWL_EXSTYLE) & (~WS_EX_LAYERED));
    else
        SetWindowLong(g_hwnd, GWL_EXSTYLE, GetWindowLong(g_hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
}

// ──────────────────────────────────────
// Menu rendering
// ──────────────────────────────────────

static void RenderAimbotTab(settings_t &s)
{
    const char* aimModes[] = { "Off", "Closest to Crosshair", "Closest Distance" };
    int aimMode = s.aim;
    if (aimMode < 0 || aimMode > 2) aimMode = 0;
    ImGui::Combo("Aim Mode", &aimMode, aimModes, IM_ARRAYSIZE(aimModes));
    s.aim = aimMode;

    ImGui::Separator();

    const char* boneNames[] = { "Head", "Neck", "Chest", "Stomach", "Hip" };
    int boneIdx = s.bone;
    if (boneIdx < 0 || boneIdx > 4) boneIdx = 2;
    ImGui::Combo("Bone Target", &boneIdx, boneNames, IM_ARRAYSIZE(boneNames));
    s.bone = boneIdx;

    ImGui::Checkbox("Auto Bone Selection", &s.bone_auto);
    ImGui::Checkbox("Nearest Bone", &s.bone_nearest);

    ImGui::Separator();
    ImGui::Text("FOV & Smoothing");
    ImGui::SliderFloat("ADS FOV", &s.ads_fov, 1.0f, 50.0f, "%.1f");
    ImGui::SliderFloat("Non-ADS FOV", &s.non_ads_fov, 1.0f, 100.0f, "%.1f");
    ImGui::SliderFloat("Smooth", &s.smooth, 1.0f, 500.0f, "%.1f");
    ImGui::SliderFloat("Smooth (Sub)", &s.smooth_sub, 1.0f, 500.0f, "%.1f");
    ImGui::SliderFloat("Aim Distance", &s.aim_dist, 10.0f, 1000.0f, "%.0f");
    ImGui::SliderFloat("Headshot Distance", &s.headshot_dist, 10.0f, 500.0f, "%.0f");

    ImGui::Separator();
    ImGui::Text("Recoil Control");
    ImGui::Checkbox("No Recoil", &s.aim_no_recoil);
    ImGui::SliderFloat("Recoil Pitch", &s.recoil_pitch, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Recoil Yaw", &s.recoil_yaw, 0.0f, 1.0f, "%.2f");

    ImGui::Separator();
    ImGui::Text("Flick Bot");
    ImGui::SliderFloat("Flick FOV", &s.flick_fov, 1.0f, 30.0f, "%.1f");
    ImGui::Checkbox("Flick Nearest", &s.flick_nearest);

    ImGui::Separator();
    ImGui::Text("Trigger Bot");
    ImGui::Checkbox("Trigger Bot", &s.trigger_bot_shot);
    ImGui::Checkbox("No Nade Aim", &s.no_nade_aim);
    ImGui::Checkbox("Bow/Charge Rifle Aim", &s.bow_charge_rifle_aim);

    ImGui::Separator();
    ImGui::Text("Skynade");
    ImGui::SliderFloat("Skynade Distance", &s.skynade_dist, 10.0f, 500.0f, "%.0f");
    ImGui::SliderFloat("Skynade Smooth", &s.skynade_smooth, 1.0f, 200.0f, "%.1f");
}

static void RenderVisualsTab(settings_t &s)
{
    ImGui::Text("ESP Options");
    ImGui::Checkbox("Enable ESP", &s.esp);
    ImGui::Checkbox("Show Aim Target", &s.show_aim_target);
    ImGui::SliderFloat("Max Distance", &s.max_dist, 50.0f, 2000.0f, "%.0f");

    ImGui::Separator();
    ImGui::Text("ESP Elements");
    ImGui::Checkbox("Box", &s.esp_visuals.box);
    ImGui::Checkbox("Line", &s.esp_visuals.line);
    ImGui::Checkbox("Distance", &s.esp_visuals.distance);
    ImGui::Checkbox("Health Bar", &s.esp_visuals.healthbar);
    ImGui::Checkbox("Shield Bar", &s.esp_visuals.shieldbar);
    ImGui::Checkbox("Name", &s.esp_visuals.name);

    ImGui::Separator();
    ImGui::Text("Radar");
    ImGui::Checkbox("Mini Map Radar", &s.mini_map_radar);
    ImGui::Checkbox("Mini Map Guides", &s.mini_map_guides);
    int dot1 = s.mini_map_radar_dot_size1;
    int dot2 = s.mini_map_radar_dot_size2;
    ImGui::SliderInt("Mini Dot Size 1", &dot1, 1, 20);
    ImGui::SliderInt("Mini Dot Size 2", &dot2, 1, 20);
    s.mini_map_radar_dot_size1 = dot1;
    s.mini_map_radar_dot_size2 = dot2;
    ImGui::Checkbox("Main Radar Map", &s.main_radar_map);
}

static void RenderGlowTab(settings_t &s)
{
    ImGui::Text("Player Glow");
    ImGui::Checkbox("Player Glow", &s.player_glow);
    ImGui::Checkbox("Armor Color", &s.player_glow_armor_color);
    ImGui::Checkbox("Love User Glow", &s.player_glow_love_user);
    ImGui::Checkbox("Weapon Model Glow", &s.weapon_model_glow);
    ImGui::SliderFloat("Glow Distance", &s.glow_dist, 10.0f, 500.0f, "%.0f");

    int inside_val = s.player_glow_inside_value;
    int outline_sz = s.player_glow_outline_size;
    ImGui::SliderInt("Inside Value", &inside_val, 0, 255);
    ImGui::SliderInt("Outline Size", &outline_sz, 0, 255);
    s.player_glow_inside_value = (uint8_t)inside_val;
    s.player_glow_outline_size = (uint8_t)outline_sz;

    ImGui::Separator();
    ImGui::Text("Not Visible Color");
    float notViz[3] = { s.glow_r_not, s.glow_g_not, s.glow_b_not };
    ImGui::ColorEdit3("Not Visible##glow", notViz);
    s.glow_r_not = notViz[0]; s.glow_g_not = notViz[1]; s.glow_b_not = notViz[2];

    ImGui::Text("Visible Color");
    float viz[3] = { s.glow_r_viz, s.glow_g_viz, s.glow_b_viz };
    ImGui::ColorEdit3("Visible##glow", viz);
    s.glow_r_viz = viz[0]; s.glow_g_viz = viz[1]; s.glow_b_viz = viz[2];

    ImGui::Text("Knocked Color");
    float knocked[3] = { s.glow_r_knocked, s.glow_g_knocked, s.glow_b_knocked };
    ImGui::ColorEdit3("Knocked##glow", knocked);
    s.glow_r_knocked = knocked[0]; s.glow_g_knocked = knocked[1]; s.glow_b_knocked = knocked[2];

    ImGui::Separator();
    ImGui::Text("Item Glow");
    ImGui::Checkbox("Item Glow", &s.item_glow);
    ImGui::Checkbox("Deathbox Glow", &s.deathbox);

    int lootFilled = s.loot_filled;
    int lootOutline = s.loot_outline;
    ImGui::SliderInt("Loot Filled", &lootFilled, 0, 255);
    ImGui::SliderInt("Loot Outline", &lootOutline, 0, 255);
    s.loot_filled = (uint8_t)lootFilled;
    s.loot_outline = (uint8_t)lootOutline;
}

static void RenderLootTab(settings_t &s)
{
    loots &l = s.loot;

    if (ImGui::CollapsingHeader("Healing & Misc"))
    {
        ImGui::Checkbox("Phoenix Kit", &l.phoenix);
        ImGui::Checkbox("Med Kit", &l.healthlarge);
        ImGui::Checkbox("Syringe", &l.healthsmall);
        ImGui::Checkbox("Shield Battery", &l.shieldbattlarge);
        ImGui::Checkbox("Shield Cell", &l.shieldbattsmall);
        ImGui::Checkbox("Ultimate Accelerant", &l.accelerant);
        ImGui::Checkbox("Mobile Respawn", &l.mobile_respawn);
    }
    if (ImGui::CollapsingHeader("Backpacks"))
    {
        ImGui::Checkbox("Light Backpack", &l.lightbackpack);
        ImGui::Checkbox("Medium Backpack", &l.medbackpack);
        ImGui::Checkbox("Heavy Backpack", &l.heavybackpack);
        ImGui::Checkbox("Gold Backpack", &l.goldbackpack);
    }
    if (ImGui::CollapsingHeader("Body Shields"))
    {
        ImGui::Checkbox("White Shield", &l.shieldupgrade1);
        ImGui::Checkbox("Blue Shield", &l.shieldupgrade2);
        ImGui::Checkbox("Purple Shield", &l.shieldupgrade3);
        ImGui::Checkbox("Gold Shield", &l.shieldupgrade4);
        ImGui::Checkbox("Red Shield", &l.shieldupgrade5);
    }
    if (ImGui::CollapsingHeader("Helmets"))
    {
        ImGui::Checkbox("White Helmet", &l.shieldupgradehead1);
        ImGui::Checkbox("Blue Helmet", &l.shieldupgradehead2);
        ImGui::Checkbox("Purple Helmet", &l.shieldupgradehead3);
        ImGui::Checkbox("Gold Helmet", &l.shieldupgradehead4);
    }
    if (ImGui::CollapsingHeader("Knockdown Shields"))
    {
        ImGui::Checkbox("White Knockdown", &l.shielddown1);
        ImGui::Checkbox("Blue Knockdown", &l.shielddown2);
        ImGui::Checkbox("Purple Knockdown", &l.shielddown3);
        ImGui::Checkbox("Gold Knockdown", &l.shielddown4);
    }
    if (ImGui::CollapsingHeader("Ammo"))
    {
        ImGui::Checkbox("Light Ammo", &l.lightammo);
        ImGui::Checkbox("Heavy Ammo", &l.heavyammo);
        ImGui::Checkbox("Energy Ammo", &l.energyammo);
        ImGui::Checkbox("Sniper Ammo", &l.sniperammo);
        ImGui::Checkbox("Shotgun Ammo", &l.shotgunammo);
    }
    if (ImGui::CollapsingHeader("Optics"))
    {
        ImGui::Checkbox("1x HCOG", &l.optic1xhcog);
        ImGui::Checkbox("2x HCOG", &l.optic2xhcog);
        ImGui::Checkbox("1x Holo", &l.opticholo1x);
        ImGui::Checkbox("1x-2x Holo", &l.opticholo1x2x);
        ImGui::Checkbox("Digi Threat", &l.opticthreat);
        ImGui::Checkbox("3x HCOG", &l.optic3xhcog);
        ImGui::Checkbox("2x-4x", &l.optic2x4x);
        ImGui::Checkbox("6x Sniper", &l.opticsniper6x);
        ImGui::Checkbox("4x-8x Sniper", &l.opticsniper4x8x);
        ImGui::Checkbox("Sniper Digi Threat", &l.opticsniperthreat);
    }
    if (ImGui::CollapsingHeader("Magazines"))
    {
        ImGui::Text("Light Mags");
        ImGui::Checkbox("Lv1##lightmag", &l.lightammomag1); ImGui::SameLine();
        ImGui::Checkbox("Lv2##lightmag", &l.lightammomag2); ImGui::SameLine();
        ImGui::Checkbox("Lv3##lightmag", &l.lightammomag3); ImGui::SameLine();
        ImGui::Checkbox("Lv4##lightmag", &l.lightammomag4);
        ImGui::Text("Heavy Mags");
        ImGui::Checkbox("Lv1##heavymag", &l.heavyammomag1); ImGui::SameLine();
        ImGui::Checkbox("Lv2##heavymag", &l.heavyammomag2); ImGui::SameLine();
        ImGui::Checkbox("Lv3##heavymag", &l.heavyammomag3); ImGui::SameLine();
        ImGui::Checkbox("Lv4##heavymag", &l.heavyammomag4);
        ImGui::Text("Energy Mags");
        ImGui::Checkbox("Lv1##energymag", &l.energyammomag1); ImGui::SameLine();
        ImGui::Checkbox("Lv2##energymag", &l.energyammomag2); ImGui::SameLine();
        ImGui::Checkbox("Lv3##energymag", &l.energyammomag3); ImGui::SameLine();
        ImGui::Checkbox("Lv4##energymag", &l.energyammomag4);
        ImGui::Text("Sniper Mags");
        ImGui::Checkbox("Lv1##snipermag", &l.sniperammomag1); ImGui::SameLine();
        ImGui::Checkbox("Lv2##snipermag", &l.sniperammomag2); ImGui::SameLine();
        ImGui::Checkbox("Lv3##snipermag", &l.sniperammomag3); ImGui::SameLine();
        ImGui::Checkbox("Lv4##snipermag", &l.sniperammomag4);
    }
    if (ImGui::CollapsingHeader("Attachments"))
    {
        ImGui::Checkbox("Turbocharger", &l.turbo_charger);
        ImGui::Checkbox("Skullpiercer", &l.skull_piecer);
        ImGui::Checkbox("Hammerpoint", &l.hammer_point);
        ImGui::Checkbox("Disruptor Rounds", &l.disruptor_rounds);
        ImGui::Checkbox("Boosted Loader", &l.boosted_loader);
        ImGui::Checkbox("Anvil Receiver", &l.anvil_receiver);
        ImGui::Checkbox("Double Tap", &l.doubletap_trigger);
        ImGui::Checkbox("Dual Shell", &l.dual_shell);
        ImGui::Checkbox("Kinetic Feeder", &l.kinetic_feeder);
        ImGui::Checkbox("Quickdraw Holster", &l.quickdraw_holster);
        ImGui::Text("Laser Sights");
        ImGui::Checkbox("Lv1##laser", &l.lasersight1); ImGui::SameLine();
        ImGui::Checkbox("Lv2##laser", &l.lasersight2); ImGui::SameLine();
        ImGui::Checkbox("Lv3##laser", &l.lasersight3); ImGui::SameLine();
        ImGui::Checkbox("Lv4##laser", &l.lasersight4);
        ImGui::Text("Stocks (Regular)");
        ImGui::Checkbox("Lv1##regstock", &l.stockregular1); ImGui::SameLine();
        ImGui::Checkbox("Lv2##regstock", &l.stockregular2); ImGui::SameLine();
        ImGui::Checkbox("Lv3##regstock", &l.stockregular3);
        ImGui::Text("Stocks (Sniper)");
        ImGui::Checkbox("Lv1##snistock", &l.stocksniper1); ImGui::SameLine();
        ImGui::Checkbox("Lv2##snistock", &l.stocksniper2); ImGui::SameLine();
        ImGui::Checkbox("Lv3##snistock", &l.stocksniper3); ImGui::SameLine();
        ImGui::Checkbox("Lv4##snistock", &l.stocksniper4);
        ImGui::Text("Barrel Stabilizers");
        ImGui::Checkbox("Lv1##barrel", &l.suppressor1); ImGui::SameLine();
        ImGui::Checkbox("Lv2##barrel", &l.suppressor2); ImGui::SameLine();
        ImGui::Checkbox("Lv3##barrel", &l.suppressor3);
        ImGui::Text("Shotgun Bolts");
        ImGui::Checkbox("Lv1##bolt", &l.shotgunbolt1); ImGui::SameLine();
        ImGui::Checkbox("Lv2##bolt", &l.shotgunbolt2); ImGui::SameLine();
        ImGui::Checkbox("Lv3##bolt", &l.shotgunbolt3); ImGui::SameLine();
        ImGui::Checkbox("Lv4##bolt", &l.shotgunbolt4);
    }
    if (ImGui::CollapsingHeader("Grenades"))
    {
        ImGui::Checkbox("Frag Grenade", &l.grenade_frag);
        ImGui::Checkbox("Arc Star", &l.grenade_arc_star);
        ImGui::Checkbox("Thermite", &l.grenade_thermite);
    }
    if (ImGui::CollapsingHeader("Weapons on Ground"))
    {
        ImGui::Checkbox("Kraber", &l.weapon_kraber);
        ImGui::Checkbox("Bocek Bow", &l.weapon_bow);
        ImGui::Separator();
        ImGui::Checkbox("Mastiff", &l.weapon_mastiff);
        ImGui::Checkbox("EVA-8", &l.weapon_eva8);
        ImGui::Checkbox("Peacekeeper", &l.weapon_peacekeeper);
        ImGui::Checkbox("Mozambique", &l.weapon_mozambique);
        ImGui::Separator();
        ImGui::Checkbox("L-Star", &l.weapon_lstar);
        ImGui::Checkbox("Nemesis", &l.weapon_nemesis);
        ImGui::Checkbox("Havoc", &l.weapon_havoc);
        ImGui::Checkbox("Devotion", &l.weapon_devotion);
        ImGui::Checkbox("Triple Take", &l.weapon_triple_take);
        ImGui::Checkbox("Volt", &l.weapon_volt);
        ImGui::Separator();
        ImGui::Checkbox("Flatline", &l.weapon_flatline);
        ImGui::Checkbox("Hemlock", &l.weapon_hemlock);
        ImGui::Checkbox("30-30 Repeater", &l.weapon_3030_repeater);
        ImGui::Checkbox("Rampage", &l.weapon_rampage);
        ImGui::Checkbox("CAR SMG", &l.weapon_car_smg);
        ImGui::Checkbox("Prowler", &l.weapon_prowler);
        ImGui::Separator();
        ImGui::Checkbox("P2020", &l.weapon_p2020);
        ImGui::Checkbox("RE-45", &l.weapon_re45);
        ImGui::Checkbox("G7 Scout", &l.weapon_g7_scout);
        ImGui::Checkbox("Alternator", &l.weapon_alternator);
        ImGui::Checkbox("R-99", &l.weapon_r99);
        ImGui::Checkbox("Spitfire", &l.weapon_spitfire);
        ImGui::Checkbox("R-301", &l.weapon_r301);
        ImGui::Separator();
        ImGui::Checkbox("Wingman", &l.weapon_wingman);
        ImGui::Checkbox("Longbow", &l.weapon_longbow);
        ImGui::Checkbox("Charge Rifle", &l.weapon_charge_rifle);
        ImGui::Checkbox("Sentinel", &l.weapon_sentinel);
    }
}

static void RenderMiscTab(settings_t &s)
{
    ImGui::Text("Movement");
    ImGui::Checkbox("Super Glide", &s.super_glide);
    ImGui::Checkbox("Super Grapple", &s.super_grpple);
    ImGui::Checkbox("Auto Tapstrafe", &s.auto_tapstrafe);

    ImGui::Separator();
    ImGui::Text("Game Mode");
    ImGui::Checkbox("1v1 Mode", &s.onevone);
    ImGui::Checkbox("Firing Range", &s.firing_range);

    ImGui::Separator();
    ImGui::Text("Screen");
    int w = (int)s.screen_width;
    int h = (int)s.screen_height;
    ImGui::InputInt("Screen Width", &w);
    ImGui::InputInt("Screen Height", &h);
    s.screen_width = (uint32_t)w;
    s.screen_height = (uint32_t)h;
    ImGui::SliderFloat("Game FPS", &s.game_fps, 30.0f, 300.0f, "%.0f");
    ImGui::Checkbox("Calculate Game FPS", &s.calc_game_fps);

    ImGui::Separator();
    ImGui::Text("Input");
    ImGui::Checkbox("Keyboard", &s.keyboard);
    ImGui::Checkbox("Gamepad", &s.gamepad);
    ImGui::Checkbox("KBD Backlight Control", &s.kbd_backlight_control);

    ImGui::Separator();
    ImGui::Text("Toggles");
    ImGui::Checkbox("Loot Filled Toggle", &s.loot_filled_toggle);
    ImGui::Checkbox("Player Filled Toggle", &s.player_filled_toggle);
}

static void RenderPredictionTab(settings_t &s)
{
    predict &p = s.weapon_predict;
    ImGui::Text("Weapon Prediction Multipliers");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Supply Drop"))
    {
        ImGui::SliderFloat("Kraber##pred", &p.weapon_kraber, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("Bocek Bow##pred", &p.weapon_bow, 0.5f, 2.0f, "%.2f");
    }
    if (ImGui::CollapsingHeader("Shotguns##pred"))
    {
        ImGui::SliderFloat("Mastiff##pred", &p.weapon_mastiff, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("EVA-8##pred", &p.weapon_eva8, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("Peacekeeper##pred", &p.weapon_peacekeeper, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("Mozambique##pred", &p.weapon_mozambique, 0.5f, 2.0f, "%.2f");
    }
    if (ImGui::CollapsingHeader("Energy##pred"))
    {
        ImGui::SliderFloat("L-Star##pred", &p.weapon_lstar, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("Nemesis##pred", &p.weapon_nemesis, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("Havoc##pred", &p.weapon_havoc, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("Devotion##pred", &p.weapon_devotion, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("Triple Take##pred", &p.weapon_triple_take, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("Volt##pred", &p.weapon_volt, 0.5f, 2.0f, "%.2f");
    }
    if (ImGui::CollapsingHeader("Heavy##pred"))
    {
        ImGui::SliderFloat("Flatline##pred", &p.weapon_flatline, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("Hemlock##pred", &p.weapon_hemlock, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("30-30##pred", &p.weapon_3030_repeater, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("Rampage##pred", &p.weapon_rampage, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("CAR SMG##pred", &p.weapon_car_smg, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("Prowler##pred", &p.weapon_prowler, 0.5f, 2.0f, "%.2f");
    }
    if (ImGui::CollapsingHeader("Light##pred"))
    {
        ImGui::SliderFloat("P2020##pred", &p.weapon_p2020, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("RE-45##pred", &p.weapon_re45, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("G7 Scout##pred", &p.weapon_g7_scout, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("Alternator##pred", &p.weapon_alternator, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("R-99##pred", &p.weapon_r99, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("Spitfire##pred", &p.weapon_spitfire, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("R-301##pred", &p.weapon_r301, 0.5f, 2.0f, "%.2f");
    }
    if (ImGui::CollapsingHeader("Snipers##pred"))
    {
        ImGui::SliderFloat("Wingman##pred", &p.weapon_wingman, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("Longbow##pred", &p.weapon_longbow, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("Charge Rifle##pred", &p.weapon_charge_rifle, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("Sentinel##pred", &p.weapon_sentinel, 0.5f, 2.0f, "%.2f");
    }
}

static void RenderMenu()
{
    settings_t s = global_settings();

    ImGui::SetNextWindowSize(ImVec2(650.0f, 700.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Apex DMA - Settings [INSERT to toggle]", &g_showMenu, ImGuiWindowFlags_NoCollapse);

    // Status bar
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: Connected");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "| Base: 0x%llX", (unsigned long long)g_Base);
    {
        std::lock_guard<std::mutex> lock(spectatorsMtx);
        if (!spectators.empty())
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "| SPECTATORS: %d", (int)spectators.size());
        }
    }
    ImGui::Separator();

    if (ImGui::BeginTabBar("SettingsTabs"))
    {
        if (ImGui::BeginTabItem("Aimbot"))
        {
            RenderAimbotTab(s);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Visuals"))
        {
            RenderVisualsTab(s);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Glow"))
        {
            RenderGlowTab(s);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Loot Filter"))
        {
            RenderLootTab(s);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Prediction"))
        {
            RenderPredictionTab(s);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Misc"))
        {
            RenderMiscTab(s);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();

    // Push settings back
    update_settings(s);
}

// ──────────────────────────────────────
// Main overlay entry point
// ──────────────────────────────────────
void start_overlay()
{
    overlay_t = true;

    // Wait for game window before creating overlay
    printf("Waiting for game window...\n");
    while (!g_gameHwnd)
    {
        g_gameHwnd = FindWindowA("Respawn001", nullptr);
        if (!g_gameHwnd)
            g_gameHwnd = FindWindowA(nullptr, "Apex Legends");
        if (!g_gameHwnd)
            Sleep(500);
        if (!overlay_t)
            return;
    }
    printf("Game window found: 0x%p\n", g_gameHwnd);

    // Get initial game window dimensions
    RECT gameRect;
    GetClientRect(g_gameHwnd, &gameRect);
    POINT pt = { gameRect.left, gameRect.top };
    ClientToScreen(g_gameHwnd, &pt);
    g_windowX = pt.x;
    g_windowY = pt.y;
    g_windowWidth = gameRect.right - gameRect.left;
    g_windowHeight = gameRect.bottom - gameRect.top;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"ApexDMAOverlay";
    RegisterClassExW(&wc);

    // Create transparent topmost overlay window attached over the game
    // Flags match reference: WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED
    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
        wc.lpszClassName, L"Apex DMA Overlay",
        WS_POPUP,
        g_windowX, g_windowY, g_windowWidth, g_windowHeight,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!g_hwnd)
    {
        printf("Failed to create overlay window.\n");
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        overlay_t = false;
        return;
    }

    // Make the window transparent via layered attributes (matches reference: LWA_ALPHA, alpha=255)
    SetLayeredWindowAttributes(g_hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);

    if (!CreateDeviceD3D(g_hwnd))
    {
        CleanupDeviceD3D();
        DestroyWindow(g_hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        printf("Failed to create D3D11 device.\n");
        overlay_t = false;
        return;
    }

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);

    // Prevent screen capture of overlay window
    SetWindowDisplayAffinity(g_hwnd, WDA_NONE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Dark theme
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.10f, 0.95f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.14f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.29f, 0.48f, 1.00f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.24f, 0.50f, 0.83f, 1.00f);
    style.Colors[ImGuiCol_TabSelected] = ImVec4(0.20f, 0.40f, 0.70f, 1.00f);

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    printf("Overlay started. Press INSERT to toggle menu.\n");

    // Transparent clear color so the overlay is see-through
    const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));

    while (msg.message != WM_QUIT && overlay_t)
    {
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                break;
        }
        if (msg.message == WM_QUIT)
            break;

        // Track game window position/size and handle click-through
        UpdateWindowData();

        // If game window is gone, exit overlay
        if (!g_gameHwnd || !IsWindow(g_gameHwnd))
        {
            printf("Game window lost, closing overlay.\n");
            break;
        }

        // Toggle menu with INSERT key
        if (GetAsyncKeyState(VK_INSERT) & 1)
            g_showMenu = !g_showMenu;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (g_showMenu)
            RenderMenu();

        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present with no vsync for lowest latency
        g_pSwapChain->Present(0, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(g_hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    g_hwnd = nullptr;
    g_gameHwnd = nullptr;
    overlay_t = false;
    printf("Overlay closed.\n");
}
