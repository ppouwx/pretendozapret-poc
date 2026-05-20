#include "ui.h"
#include "config.h"
#include "logger.h"
#include <string.h>
#include <wups.h>
#include <wups/config_api.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/config/WUPSConfigItemIntegerRange.h>
#include <wups/config/WUPSConfigItemMultipleValues.h>
#include <wups/config/WUPSConfigItemIPAddress.h>
#include <wups/config/WUPSConfigItemStub.h>

static bool s_initialized = false;

// ── Boolean callbacks ──────────────────────────────────

static void on_enabled_changed(ConfigItemBoolean *item, bool newValue) {
    (void)item;
    g_config.enabled = newValue;
    Log_Info("UI", "Config: enabled = %d", newValue);
}

static void on_show_notif_changed(ConfigItemBoolean *item, bool newValue) {
    (void)item;
    g_config.show_notification = newValue;
}

static void on_udp_bypass_changed(ConfigItemBoolean *item, bool newValue) {
    (void)item;
    g_config.udp_bypass = newValue;
}

static void on_log_enabled_changed(ConfigItemBoolean *item, bool newValue) {
    (void)item;
    g_config.log_enabled = newValue;
    g_log_enabled = newValue;
}

// ── Integer range callbacks ────────────────────────────

static void on_fragment_size_changed(ConfigItemIntegerRange *item, int32_t newValue) {
    (void)item;
    g_config.fragment_size = (uint16_t)newValue;
}

static void on_delay_changed(ConfigItemIntegerRange *item, int32_t newValue) {
    (void)item;
    g_config.inter_fragment_delay_us = (uint32_t)newValue;
}

static void on_split_pos_changed(ConfigItemIntegerRange *item, int32_t newValue) {
    (void)item;
    g_config.split_position = (size_t)newValue;
}

static void on_udp_port_start_changed(ConfigItemIntegerRange *item, int32_t newValue) {
    (void)item;
    g_config.udp_fake_port_start = (uint16_t)newValue;
}

static void on_udp_port_end_changed(ConfigItemIntegerRange *item, int32_t newValue) {
    (void)item;
    g_config.udp_fake_port_end = (uint16_t)newValue;
}

static void on_doh_port_changed(ConfigItemIntegerRange *item, int32_t newValue) {
    (void)item;
    g_config.doh_port = (uint16_t)newValue;
}

static void on_log_level_changed(ConfigItemIntegerRange *item, int32_t newValue) {
    (void)item;
    g_config.log_level = (int)newValue;
    g_log_min_level = (log_level_t)newValue;
}

// ── Multiple values (strategy) callback ────────────────

static ConfigItemMultipleValuesPair g_strategy_values[] = {
    { STRATEGY_SPLIT,      "Split" },
    { STRATEGY_MULTISPLIT,  "Multi-Split" },
    { STRATEGY_FAKEDSPLIT,  "Faked-Split" },
    { STRATEGY_DELAY,       "Delay" },
    { STRATEGY_RAW,         "Raw" },
};

static void on_strategy_changed(ConfigItemMultipleValues *item, uint32_t newValue) {
    (void)item;
    if (newValue < STRATEGY_COUNT) {
        g_config.strategy = (bypass_strategy_t)newValue;
    }
}

// ── IP address callback ────────────────────────────────

static void on_doh_server_changed(ConfigItemIPAddress *item, uint32_t ip) {
    (void)item;
    Config_IPU32ToStr(ip, g_config.doh_server, sizeof(g_config.doh_server));
}

// ── Menu opened: build the entire config tree ──────────

static WUPSConfigAPICallbackStatus MenuOpened(WUPSConfigCategoryHandle root) {
    WUPSConfigCategoryHandle catGeneral, catTcp, catUdp, catLog, catDns;
    WUPSConfigAPIStatus err;
    int32_t curFrag = (int32_t)g_config.fragment_size;
    int32_t curDelay = (int32_t)g_config.inter_fragment_delay_us;
    int32_t curPortStart = (int32_t)g_config.udp_fake_port_start;
    int32_t curPortEnd = (int32_t)g_config.udp_fake_port_end;
    int32_t curDohPort = (int32_t)g_config.doh_port;
    int32_t curLogLevel = (int32_t)g_config.log_level;

    // ── General ──────────────────────────────────────
    WUPSConfigAPICreateCategoryOptionsV1 genOpts = { .name = "General" };
    err = WUPSConfigAPI_Category_Create(genOpts, &catGeneral);
    if (err != WUPSCONFIG_API_RESULT_SUCCESS) return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;

    WUPSConfigItemBoolean_AddToCategory(catGeneral, "enabled", "Enabled",
        DEFAULT_CONFIG.enabled, g_config.enabled, on_enabled_changed);

    WUPSConfigItemMultipleValues_AddToCategory(catGeneral, "strategy", "Strategy",
        (int)DEFAULT_CONFIG.strategy, (int)g_config.strategy,
        g_strategy_values, STRATEGY_COUNT, on_strategy_changed);

    WUPSConfigItemBoolean_AddToCategory(catGeneral, "showNotification", "Show Notification",
        DEFAULT_CONFIG.show_notification, g_config.show_notification, on_show_notif_changed);

    WUPSConfigAPI_Category_AddCategory(root, catGeneral);

    // ── TCP ──────────────────────────────────────────
    WUPSConfigAPICreateCategoryOptionsV1 tcpOpts = { .name = "TCP" };
    err = WUPSConfigAPI_Category_Create(tcpOpts, &catTcp);
    if (err != WUPSCONFIG_API_RESULT_SUCCESS) return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;

    WUPSConfigItemIntegerRange_AddToCategory(catTcp, "fragmentSize", "Fragment Size",
        DEFAULT_CONFIG.fragment_size, curFrag, 1, 64, on_fragment_size_changed);

    WUPSConfigItemIntegerRange_AddToCategory(catTcp, "interFragmentDelayUs", "Delay (µs)",
        DEFAULT_CONFIG.inter_fragment_delay_us, curDelay, 0, 100000, on_delay_changed);

    WUPSConfigItemIntegerRange_AddToCategory(catTcp, "splitPosition", "Split Pos (0=auto)",
        (int32_t)DEFAULT_CONFIG.split_position, (int32_t)g_config.split_position,
        0, 64, on_split_pos_changed);

    WUPSConfigAPI_Category_AddCategory(root, catTcp);

    // ── UDP ──────────────────────────────────────────
    WUPSConfigAPICreateCategoryOptionsV1 udpOpts = { .name = "UDP" };
    err = WUPSConfigAPI_Category_Create(udpOpts, &catUdp);
    if (err != WUPSCONFIG_API_RESULT_SUCCESS) return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;

    WUPSConfigItemBoolean_AddToCategory(catUdp, "udpBypass", "UDP Bypass",
        DEFAULT_CONFIG.udp_bypass, g_config.udp_bypass, on_udp_bypass_changed);

    WUPSConfigItemIntegerRange_AddToCategory(catUdp, "udpFakePortStart", "Fake Port Start",
        DEFAULT_CONFIG.udp_fake_port_start, curPortStart, 1024, 65535, on_udp_port_start_changed);

    WUPSConfigItemIntegerRange_AddToCategory(catUdp, "udpFakePortEnd", "Fake Port End",
        DEFAULT_CONFIG.udp_fake_port_end, curPortEnd, 1024, 65535, on_udp_port_end_changed);

    WUPSConfigAPI_Category_AddCategory(root, catUdp);

    // ── Log ──────────────────────────────────────────
    WUPSConfigAPICreateCategoryOptionsV1 logOpts = { .name = "Log" };
    err = WUPSConfigAPI_Category_Create(logOpts, &catLog);
    if (err != WUPSCONFIG_API_RESULT_SUCCESS) return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;

    WUPSConfigItemBoolean_AddToCategory(catLog, "logEnabled", "Log Enabled",
        DEFAULT_CONFIG.log_enabled, g_config.log_enabled, on_log_enabled_changed);

    WUPSConfigItemIntegerRange_AddToCategory(catLog, "logLevel", "Log Level (0-3)",
        DEFAULT_CONFIG.log_level, curLogLevel, 0, 3, on_log_level_changed);

    WUPSConfigItemStub_AddToCategory(catLog, "0=Debug, 1=Info, 2=Warn, 3=Error");

    WUPSConfigAPI_Category_AddCategory(root, catLog);

    // ── DNS ──────────────────────────────────────────
    WUPSConfigAPICreateCategoryOptionsV1 dnsOpts = { .name = "DNS" };
    err = WUPSConfigAPI_Category_Create(dnsOpts, &catDns);
    if (err != WUPSCONFIG_API_RESULT_SUCCESS) return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;

    WUPSConfigItemIPAddress_AddToCategory(catDns, "dohServer", "DNS Server",
        Config_IPStrToU32(DEFAULT_CONFIG.doh_server),
        Config_IPStrToU32(g_config.doh_server),
        on_doh_server_changed);

    WUPSConfigItemIntegerRange_AddToCategory(catDns, "dohPort", "DNS Port",
        DEFAULT_CONFIG.doh_port, curDohPort, 1, 65535, on_doh_port_changed);

    WUPSConfigAPI_Category_AddCategory(root, catDns);

    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

// ── Menu closed: persist config to WUPS storage ────────

static void MenuClosed(void) {
    Config_SaveToStorage();
    Log_Info("UI", "Config menu closed, settings saved");
}

// ── Public API ─────────────────────────────────────────

void UI_Init(void) {
    if (s_initialized) return;

    WUPSConfigAPIOptionsV1 opts = { .name = "WiiU-Bypass" };
    WUPSConfigAPIStatus status = WUPSConfigAPI_Init(opts, MenuOpened, MenuClosed);
    if (status != WUPSCONFIG_API_RESULT_SUCCESS) {
        Log_Warn("UI", "WUPSConfigAPI_Init failed: %s", WUPSConfigAPI_GetStatusStr(status));
    } else {
        Log_Info("UI", "Config menu initialized (L+Down+Minus)");
    }

    s_initialized = true;
}

void UI_ShowNotification(const char *title, const char *message) {
    if (!g_config.show_notification || !s_initialized) return;
    (void)title;
    (void)message;
}

void UI_Deinit(void) {
    s_initialized = false;
}
