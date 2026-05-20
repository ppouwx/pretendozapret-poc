#pragma once
#include <wups.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_DOMAIN_LIST_ENTRIES 256
#define MAX_DOMAIN_LEN 128
#define CONFIG_PATH "sd:/wiiu-bypass/settings.ini"
#define DOMAINS_PATH "sd:/wiiu-bypass/domains.txt"

typedef enum {
    STRATEGY_SPLIT = 0,
    STRATEGY_MULTISPLIT,
    STRATEGY_FAKEDSPLIT,
    STRATEGY_DELAY,
    STRATEGY_RAW,
    STRATEGY_COUNT
} bypass_strategy_t;

typedef struct {
    bool enabled;
    bypass_strategy_t strategy;
    bool show_notification;
    uint16_t fragment_size;
    uint32_t inter_fragment_delay_us;
    size_t split_position;
    bool udp_bypass;
    uint16_t udp_fake_port_start;
    uint16_t udp_fake_port_end;
    char doh_server[64];
    uint16_t doh_port;
} config_t;

extern config_t g_config;
extern char g_domain_list[MAX_DOMAIN_LIST_ENTRIES][MAX_DOMAIN_LEN];
extern int g_domain_count;

bool Config_Load(void);
bool Config_Save(void);
void Config_Init(void);
void Config_LoadDomainList(void);
bool Config_IsDomainListed(const char *domain);
