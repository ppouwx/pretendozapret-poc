#include "config.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

config_t g_config;
char g_domain_list[MAX_DOMAIN_LIST_ENTRIES][MAX_DOMAIN_LEN];
int g_domain_count = 0;

const config_t DEFAULT_CONFIG = {
    .enabled = true,
    .strategy = STRATEGY_SPLIT,
    .show_notification = true,
    .fragment_size = 2,
    .inter_fragment_delay_us = 1000,
    .split_position = 0,
    .udp_bypass = true,
    .udp_fake_port_start = 60000,
    .udp_fake_port_end = 65535,
    .doh_server = "8.8.8.8",
    .doh_port = 443,
    .log_enabled = true,
    .log_level = LOG_INFO,
};

void Config_Init(void) {
    g_config = DEFAULT_CONFIG;
    g_domain_count = 0;
}

uint32_t Config_IPStrToU32(const char *str) {
    uint8_t a, b, c, d;
    if (sscanf(str, "%hhu.%hhu.%hhu.%hhu", &a, &b, &c, &d) == 4) {
        return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | d;
    }
    return 0;
}

void Config_IPU32ToStr(uint32_t ip, char *buf, size_t bufsize) {
    snprintf(buf, bufsize, "%d.%d.%d.%d",
        (int)((ip >> 24) & 0xFF), (int)((ip >> 16) & 0xFF),
        (int)((ip >> 8) & 0xFF), (int)(ip & 0xFF));
}

static void load_single_from_storage(void) {
    bool bval;
    int32_t ival;

    if (WUPSStorageAPI_GetBool(NULL, "enabled", &bval) == WUPS_STORAGE_ERROR_SUCCESS)
        g_config.enabled = bval;
    if (WUPSStorageAPI_GetS32(NULL, "strategy", &ival) == WUPS_STORAGE_ERROR_SUCCESS) {
        if (ival >= 0 && ival < STRATEGY_COUNT)
            g_config.strategy = (bypass_strategy_t)ival;
    }
    if (WUPSStorageAPI_GetBool(NULL, "showNotification", &bval) == WUPS_STORAGE_ERROR_SUCCESS)
        g_config.show_notification = bval;
    if (WUPSStorageAPI_GetS32(NULL, "fragmentSize", &ival) == WUPS_STORAGE_ERROR_SUCCESS)
        g_config.fragment_size = (uint16_t)ival;
    if (WUPSStorageAPI_GetS32(NULL, "interFragmentDelayUs", &ival) == WUPS_STORAGE_ERROR_SUCCESS)
        g_config.inter_fragment_delay_us = (uint32_t)ival;
    if (WUPSStorageAPI_GetS32(NULL, "splitPosition", &ival) == WUPS_STORAGE_ERROR_SUCCESS)
        g_config.split_position = (size_t)ival;
    if (WUPSStorageAPI_GetBool(NULL, "udpBypass", &bval) == WUPS_STORAGE_ERROR_SUCCESS)
        g_config.udp_bypass = bval;
    if (WUPSStorageAPI_GetS32(NULL, "udpFakePortStart", &ival) == WUPS_STORAGE_ERROR_SUCCESS)
        g_config.udp_fake_port_start = (uint16_t)ival;
    if (WUPSStorageAPI_GetS32(NULL, "udpFakePortEnd", &ival) == WUPS_STORAGE_ERROR_SUCCESS)
        g_config.udp_fake_port_end = (uint16_t)ival;
    if (WUPSStorageAPI_GetS32(NULL, "dohPort", &ival) == WUPS_STORAGE_ERROR_SUCCESS)
        g_config.doh_port = (uint16_t)ival;
    if (WUPSStorageAPI_GetBool(NULL, "logEnabled", &bval) == WUPS_STORAGE_ERROR_SUCCESS)
        g_config.log_enabled = bval;
    if (WUPSStorageAPI_GetS32(NULL, "logLevel", &ival) == WUPS_STORAGE_ERROR_SUCCESS)
        g_config.log_level = ival;

    char doh_str[64] = {0};
    uint32_t outsize = 0;
    if (WUPSStorageAPI_GetString(NULL, "dohServer", doh_str, sizeof(doh_str), &outsize) == WUPS_STORAGE_ERROR_SUCCESS) {
        size_t doh_len = strlen(doh_str);
        size_t copylen = doh_len < sizeof(g_config.doh_server) - 1 ? doh_len : sizeof(g_config.doh_server) - 1;
        memcpy(g_config.doh_server, doh_str, copylen);
        g_config.doh_server[copylen] = '\0';
    }
}

void Config_LoadFromStorage(void) {
    g_config = DEFAULT_CONFIG;
    load_single_from_storage();
    g_log_enabled = g_config.log_enabled;
    g_log_min_level = (log_level_t)g_config.log_level;
}

void Config_SaveToStorage(void) {
    WUPSStorageAPI_StoreBool(NULL, "enabled", g_config.enabled);
    WUPSStorageAPI_StoreS32(NULL, "strategy", (int32_t)g_config.strategy);
    WUPSStorageAPI_StoreBool(NULL, "showNotification", g_config.show_notification);
    WUPSStorageAPI_StoreS32(NULL, "fragmentSize", (int32_t)g_config.fragment_size);
    WUPSStorageAPI_StoreS32(NULL, "interFragmentDelayUs", (int32_t)g_config.inter_fragment_delay_us);
    WUPSStorageAPI_StoreS32(NULL, "splitPosition", (int32_t)g_config.split_position);
    WUPSStorageAPI_StoreBool(NULL, "udpBypass", g_config.udp_bypass);
    WUPSStorageAPI_StoreS32(NULL, "udpFakePortStart", (int32_t)g_config.udp_fake_port_start);
    WUPSStorageAPI_StoreS32(NULL, "udpFakePortEnd", (int32_t)g_config.udp_fake_port_end);
    WUPSStorageAPI_StoreString(NULL, "dohServer", g_config.doh_server);
    WUPSStorageAPI_StoreS32(NULL, "dohPort", (int32_t)g_config.doh_port);
    WUPSStorageAPI_StoreBool(NULL, "logEnabled", g_config.log_enabled);
    WUPSStorageAPI_StoreS32(NULL, "logLevel", (int32_t)g_config.log_level);
    WUPSStorageAPI_SaveStorage(false);
}

// Built-in domain list (always compiled into the plugin, no SD file needed)
static const char *BUILTIN_DOMAINS[] = {
    "pretendo.cc",
    "account.pretendo.cc",
    "saccount.pretendo.cc",
    "nasc.pretendo.cc",
    "nppl.app.pretendo.cc",
    "nppl.c.app.pretendo.cc",
    "npts.app.pretendo.cc",
    "npdi.cdn.pretendo.cc",
    "npdl.cdn.pretendo.cc",
    "npfl.c.app.pretendo.cc",
    "service.spr.app.pretendo.cc",
    "n.app.pretendo.cc",
    "ecs.wup.shop.pretendo.cc",
    "ecs.c.shop.pretendo.cc",
    "ias.c.shop.pretendo.cc",
    "cas.c.shop.pretendo.cc",
    "nus.wup.shop.pretendo.cc",
    "nus.c.shop.pretendo.cc",
    "tagaya.wup.shop.pretendo.cc",
    "pushmore.wup.shop.pretendo.cc",
    "pushmo.wup.shop.pretendo.cc",
    "pls.wup.shop.pretendo.cc",
    "samurai.wup.shop.pretendo.cc",
    "npns-dev.c.app.pretendo.cc",
    "npvk.app.pretendo.cc",
    "npvk-dev.app.pretendo.cc",
    "discovery.olv.pretendo.cc",
    "cdn.pretendo.cc",
    "idbe-wup.cdn.pretendo.cc",
    "idbe-ctr.cdn.pretendo.cc",
    "pretendo.network",
    "juxt.pretendo.network",
    "forum.pretendo.network",
    "developer.pretendo.network",
    "nintendo-wiki.pretendo.network",
    "mario-kart-8.pretendo.network",
    "status.pretendo.zip",
    "account.nintendo.net",
    "saccount.nintendo.net",
    "nasc.nintendo.net",
    "conntest.nintendowifi.net",
    "discovery.olv.nintendo.net",
    "nintendo.net",
    "nintendowifi.net",
    "cdn.nintendo.net",
    "cloudflare.com",
    "cloudflare.net",
    "cloudflare-dns.com",
    "google.com",
    "dns.google",
};
#define BUILTIN_DOMAIN_COUNT (sizeof(BUILTIN_DOMAINS) / sizeof(BUILTIN_DOMAINS[0]))

static void load_builtin_domains(void) {
    for (int i = 0; i < (int)BUILTIN_DOMAIN_COUNT && g_domain_count < MAX_DOMAIN_LIST_ENTRIES; i++) {
        strncpy(g_domain_list[g_domain_count], BUILTIN_DOMAINS[i], MAX_DOMAIN_LEN - 1);
        g_domain_list[g_domain_count][MAX_DOMAIN_LEN - 1] = '\0';
        g_domain_count++;
    }
}

void Config_LoadDomainList(void) {
    g_domain_count = 0;
    load_builtin_domains();

    // Optional: extend from SD card domains.txt (appended to built-in list)
    FILE *f = fopen("sd:/wiiu-bypass/domains.txt", "r");
    if (!f) return;

    char line[MAX_DOMAIN_LEN];
    while (fgets(line, sizeof(line), f) && g_domain_count < MAX_DOMAIN_LIST_ENTRIES) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '\0') continue;
        size_t dlen = strlen(trimmed);
        size_t cp = dlen < MAX_DOMAIN_LEN - 1 ? dlen : MAX_DOMAIN_LEN - 1;
        memcpy(g_domain_list[g_domain_count], trimmed, cp);
        g_domain_list[g_domain_count][cp] = '\0';
        g_domain_count++;
    }
    fclose(f);
}

bool Config_IsDomainListed(const char *domain) {
    if (!domain) return false;

    for (int i = 0; i < g_domain_count; i++) {
        int dlen = strlen(g_domain_list[i]);
        int dlen2 = strlen(domain);

        if (strcasecmp(domain, g_domain_list[i]) == 0) return true;

        if (dlen2 > dlen && domain[dlen2 - dlen - 1] == '.') {
            if (strcasecmp(domain + (dlen2 - dlen), g_domain_list[i]) == 0) return true;
        }
    }

    return false;
}
