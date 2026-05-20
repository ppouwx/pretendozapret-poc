#include "config.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>

config_t g_config;
char g_domain_list[MAX_DOMAIN_LIST_ENTRIES][MAX_DOMAIN_LEN];
int g_domain_count = 0;

static const config_t DEFAULT_CONFIG = {
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

bool Config_Load(void) {
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) {
        f = fopen(CONFIG_PATH, "w");
        if (f) {
            fprintf(f,
                "# WiiU-Bypass Configuration\n"
                "# Lines starting with # are comments\n"
                "\n"
                "[Main]\n"
                "enabled = 1\n"
                "strategy = 0\n"
                "show_notification = 1\n"
                "\n"
                "[TCP]\n"
                "fragment_size = 2\n"
                "inter_fragment_delay_us = 1000\n"
                "split_position = 0\n"
                "\n"
                "[UDP]\n"
                "udp_bypass = 1\n"
                "udp_fake_port_start = 60000\n"
                "udp_fake_port_end = 65535\n"
                "\n"
                "[DNS]\n"
                "doh_server = 8.8.8.8\n"
                "doh_port = 443\n"
                "\n"
                "[Log]\n"
                "log_enabled = 1\n"
                "log_level = 1\n"
            );
            fclose(f);
        }
        Config_LoadDomainList();
        return false;
    }

    char line[256];
    char section[32] = {0};

    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';

        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '\0') continue;

        if (*trimmed == '[') {
            char *end = strchr(trimmed + 1, ']');
            if (end) {
                *end = '\0';
                strncpy(section, trimmed + 1, sizeof(section) - 1);
            }
            continue;
        }

        char *eq = strchr(trimmed, '=');
        if (!eq) continue;

        char *key = trimmed;
        *eq = '\0';
        eq++;

        char *val = eq;
        while (*key == ' ' || *key == '\t') key++;
        char *kend = key + strlen(key) - 1;
        while (kend > key && (*kend == ' ' || *kend == '\t')) *kend-- = '\0';

        while (*val == ' ' || *val == '\t') val++;
        char *vend = val + strlen(val) - 1;
        while (vend > val && (*vend == ' ' || *vend == '\t')) *vend-- = '\0';

        if (strcmp(section, "Main") == 0) {
            if (strcmp(key, "enabled") == 0) g_config.enabled = atoi(val) != 0;
            else if (strcmp(key, "strategy") == 0) {
                int s = atoi(val);
                if (s >= 0 && s < STRATEGY_COUNT) g_config.strategy = (bypass_strategy_t)s;
            }
            else if (strcmp(key, "show_notification") == 0) g_config.show_notification = atoi(val) != 0;
        }
        else if (strcmp(section, "TCP") == 0) {
            if (strcmp(key, "fragment_size") == 0) g_config.fragment_size = atoi(val);
            else if (strcmp(key, "inter_fragment_delay_us") == 0) g_config.inter_fragment_delay_us = atoi(val);
            else if (strcmp(key, "split_position") == 0) g_config.split_position = atoi(val);
        }
        else if (strcmp(section, "UDP") == 0) {
            if (strcmp(key, "udp_bypass") == 0) g_config.udp_bypass = atoi(val) != 0;
            else if (strcmp(key, "udp_fake_port_start") == 0) g_config.udp_fake_port_start = atoi(val);
            else if (strcmp(key, "udp_fake_port_end") == 0) g_config.udp_fake_port_end = atoi(val);
        }
        else if (strcmp(section, "DNS") == 0) {
            if (strcmp(key, "doh_server") == 0) strncpy(g_config.doh_server, val, sizeof(g_config.doh_server) - 1);
            else if (strcmp(key, "doh_port") == 0) g_config.doh_port = atoi(val);
        }
        else if (strcmp(section, "Log") == 0) {
            if (strcmp(key, "log_enabled") == 0) g_config.log_enabled = atoi(val) != 0;
            else if (strcmp(key, "log_level") == 0) g_config.log_level = atoi(val);
        }
    }

    fclose(f);
    Config_LoadDomainList();

    // Apply log settings to global logger state
    g_log_enabled = g_config.log_enabled;
    g_log_min_level = (log_level_t)g_config.log_level;

    return true;
}

bool Config_Save(void) {
    FILE *f = fopen(CONFIG_PATH, "w");
    if (!f) return false;

    fprintf(f,
        "# WiiU-Bypass Configuration\n"
        "[Main]\n"
        "enabled = %d\n"
        "strategy = %d\n"
        "show_notification = %d\n"
        "\n"
        "[TCP]\n"
        "fragment_size = %d\n"
        "inter_fragment_delay_us = %u\n"
        "split_position = %zu\n"
        "\n"
        "[UDP]\n"
        "udp_bypass = %d\n"
        "udp_fake_port_start = %u\n"
        "udp_fake_port_end = %u\n"
        "\n"
        "[DNS]\n"
        "doh_server = %s\n"
        "doh_port = %u\n"
        "\n"
        "[Log]\n"
        "log_enabled = %d\n"
        "log_level = %d\n",
        g_config.enabled ? 1 : 0,
        (int)g_config.strategy,
        g_config.show_notification ? 1 : 0,
        g_config.fragment_size,
        g_config.inter_fragment_delay_us,
        g_config.split_position,
        g_config.udp_bypass ? 1 : 0,
        g_config.udp_fake_port_start,
        g_config.udp_fake_port_end,
        g_config.doh_server,
        g_config.doh_port,
        g_config.log_enabled ? 1 : 0,
        g_config.log_level
    );

    fclose(f);
    return true;
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

    // Always load built-in domains first - plugin works without any SD card file
    load_builtin_domains();

    // Optional: extend/override from SD card file
    // If the file doesn't exist, create it with the full list as reference
    FILE *f = fopen(DOMAINS_PATH, "r");
    if (!f) {
        f = fopen(DOMAINS_PATH, "w");
        if (f) {
            fprintf(f, "# WiiU-Bypass Domain List\n"
                "# This file is OPTIONAL. Built-in domains are compiled into the plugin.\n"
                "# Add extra domains here if needed, one per line.\n");
            for (int i = 0; i < g_domain_count; i++) {
                fprintf(f, "%s\n", g_domain_list[i]);
            }
            fclose(f);
        }
        return;
    }

    // If file exists, replace built-in list with the file contents
    g_domain_count = 0;
    char line[MAX_DOMAIN_LEN];
    while (fgets(line, sizeof(line), f) && g_domain_count < MAX_DOMAIN_LIST_ENTRIES) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '\0') continue;
        strncpy(g_domain_list[g_domain_count], trimmed, MAX_DOMAIN_LEN - 1);
        g_domain_list[g_domain_count][MAX_DOMAIN_LEN - 1] = '\0';
        g_domain_count++;
    }
    fclose(f);

    // If the file is empty/corrupted, fall back to built-in
    if (g_domain_count == 0) {
        load_builtin_domains();
    }
}

bool Config_IsDomainListed(const char *domain) {
    if (!domain) return false;

    for (int i = 0; i < g_domain_count; i++) {
        int dlen = strlen(g_domain_list[i]);
        int dlen2 = strlen(domain);

        // Exact match
        if (strcasecmp(domain, g_domain_list[i]) == 0) return true;

        // Subdomain match: if listed domain is ".pretendo.cc" or "pretendo.cc",
        // match "account.pretendo.cc" and "sub.account.pretendo.cc"
        if (dlen2 > dlen && domain[dlen2 - dlen - 1] == '.') {
            if (strcasecmp(domain + (dlen2 - dlen), g_domain_list[i]) == 0) return true;
        }
    }

    return false;
}
