// C++ stubs replacing the Rust apexsky library functions.
// These provide default settings and no-op implementations for Windows.

#include "apex_sky.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static global_state_t g_state = {};
static bool g_initialized = false;

static void init_default_settings()
{
    if (g_initialized)
        return;
    g_initialized = true;

    memset(&g_state, 0, sizeof(g_state));
    g_state.terminal_t = true;

    settings_t &s = g_state.settings;
    s.load_settings = false;
    s.no_overlay = false; // DX11 overlay enabled
    s.screen_width = 1920;
    s.screen_height = 1080;
    s.yuan_p = false;
    s.debug_mode = false;
    s.keyboard = true;
    s.gamepad = false;
    s.aimbot_hot_key_1 = 0x02; // Right mouse button
    s.aimbot_hot_key_2 = 0xA4; // Left Alt
    s.trigger_bot_hot_key = 0;
    s.flick_bot_hot_key = 0;
    s.quickglow_hot_key = 0;
    s.quickaim_hot_key = 0;
    s.loot_filled_toggle = false;
    s.player_filled_toggle = false;
    s.super_glide = false;
    s.super_grpple = false;
    s.auto_tapstrafe = false;
    s.onevone = false;
    s.item_glow = true;
    s.player_glow = true;
    s.player_glow_armor_color = true;
    s.player_glow_love_user = false;
    s.weapon_model_glow = false;
    s.kbd_backlight_control = false;
    s.bow_charge_rifle_aim = true;
    s.trigger_bot_shot = false;
    s.deathbox = false;
    s.aim_no_recoil = true;
    s.recoil_pitch = 0.5f;
    s.recoil_yaw = 0.0f;
    s.ads_fov = 10.0f;
    s.non_ads_fov = 50.0f;
    s.flick_fov = 5.0f;
    s.aim = 1;
    s.esp = true;
    s.esp_visuals.box = true;
    s.esp_visuals.line = true;
    s.esp_visuals.distance = true;
    s.esp_visuals.healthbar = true;
    s.esp_visuals.shieldbar = true;
    s.esp_visuals.name = true;
    s.mini_map_radar = false;
    s.mini_map_guides = false;
    s.mini_map_radar_dot_size1 = 5;
    s.mini_map_radar_dot_size2 = 5;
    s.main_radar_map = false;
    s.main_map_radar_dot_size1 = 5;
    s.main_map_radar_dot_size2 = 5;
    s.aim_dist = 200.0f;
    s.max_dist = 500.0f;
    s.glow_dist = 200.0f;
    s.map_radar_hotkey = 0;
    s.show_aim_target = false;
    s.game_fps = 75.0f;
    s.calc_game_fps = false;
    s.no_nade_aim = true;
    s.firing_range = false;
    s.bone = 2;
    s.bone_nearest = false;
    s.bone_auto = true;
    s.flick_nearest = false;
    s.headshot_dist = 50.0f;
    s.skynade_dist = 150.0f;
    s.smooth = 120.0f;
    s.smooth_sub = 200.0f;
    s.skynade_smooth = 50.0f;
    s.player_glow_inside_value = 15;
    s.player_glow_outline_size = 32;
    s.glow_r_not = 1.0f;
    s.glow_g_not = 0.0f;
    s.glow_b_not = 0.0f;
    s.glow_r_viz = 0.0f;
    s.glow_g_viz = 1.0f;
    s.glow_b_viz = 0.0f;
    s.glow_r_knocked = 1.0f;
    s.glow_g_knocked = 1.0f;
    s.glow_b_knocked = 0.0f;
    s.loot_filled = 0;
    s.loot_outline = 32;

    // Default weapon predictions
    predict &wp = s.weapon_predict;
    wp.weapon_kraber = 1.0f;
    wp.weapon_bow = 1.0f;
    wp.weapon_mastiff = 1.0f;
    wp.weapon_eva8 = 1.0f;
    wp.weapon_peacekeeper = 1.0f;
    wp.weapon_mozambique = 1.0f;
    wp.weapon_lstar = 1.0f;
    wp.weapon_nemesis = 1.0f;
    wp.weapon_havoc = 1.0f;
    wp.weapon_devotion = 1.0f;
    wp.weapon_triple_take = 1.0f;
    wp.weapon_volt = 1.0f;
    wp.weapon_flatline = 1.0f;
    wp.weapon_hemlock = 1.0f;
    wp.weapon_3030_repeater = 1.0f;
    wp.weapon_rampage = 1.0f;
    wp.weapon_car_smg = 1.0f;
    wp.weapon_prowler = 1.0f;
    wp.weapon_p2020 = 1.0f;
    wp.weapon_re45 = 1.0f;
    wp.weapon_g7_scout = 1.0f;
    wp.weapon_alternator = 1.0f;
    wp.weapon_r99 = 1.0f;
    wp.weapon_spitfire = 1.0f;
    wp.weapon_r301 = 1.0f;
    wp.weapon_wingman = 1.0f;
    wp.weapon_longbow = 1.0f;
    wp.weapon_charge_rifle = 1.0f;
    wp.weapon_sentinel = 1.0f;
}

void print_run_as_root()
{
    printf("Note: On Windows, run as Administrator for driver access.\n");
}

global_state_t __get_global_states()
{
    init_default_settings();
    return g_state;
}

void __update_global_states(global_state_t state)
{
    g_state = state;
}

void __load_settings()
{
    init_default_settings();
}

bool save_settings()
{
    // No-op: settings are in-memory only
    return true;
}

void run_tui_menu()
{
    // No TUI menu on Windows - just print status
    printf("=== Apex DMA (km driver) ===\n");
    printf("Settings loaded with defaults.\n");
    printf("Running in debug mode (no TUI).\n");
}

bool check_love_player(uint64_t puid, uint64_t euid, const char *name)
{
    (void)puid;
    (void)euid;
    (void)name;
    return false;
}

vector2d_t skynade_angle(uint32_t weapon_id, uint32_t weapon_mod_bitfield,
                         float weapon_projectile_scale,
                         float weapon_projectile_speed,
                         float local_view_origin_x, float local_view_origin_y,
                         float local_view_origin_z, float target_x,
                         float target_y, float target_z)
{
    (void)weapon_id;
    (void)weapon_mod_bitfield;

    vector2d_t result = {0.0f, 0.0f};

    float dx = target_x - local_view_origin_x;
    float dy = target_y - local_view_origin_y;
    float dz = target_z - local_view_origin_z;
    float distance_xy = sqrtf(dx * dx + dy * dy);

    if (distance_xy < 0.001f || weapon_projectile_speed < 0.001f)
        return result;

    float time = distance_xy / weapon_projectile_speed;
    float drop = 0.5f * weapon_projectile_scale * time * time;

    result.y = atan2f(dy, dx);
    result.x = atan2f(dz + drop, distance_xy);

    return result;
}
