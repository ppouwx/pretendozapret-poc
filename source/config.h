#pragma once
#include <wups.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_DOMAIN_LIST_ENTRIES 256
#define MAX_DOMAIN_LEN 128

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
    bool log_enabled;
    int log_level;
} config_t;

extern config_t g_config;
extern const config_t DEFAULT_CONFIG;
extern char g_domain_list[MAX_DOMAIN_LIST_ENTRIES][MAX_DOMAIN_LEN];
extern int g_domain_count;

void Config_Init(void);
void Config_LoadFromStorage(void);
void Config_SaveToStorage(void);
void Config_LoadDomainList(void);
bool Config_IsDomainListed(const char *domain);

// IP string <-> uint32_t helpers for WUPS IP address config item
uint32_t Config_IPStrToU32(const char *str);
void Config_IPU32ToStr(uint32_t ip, char *buf, size_t bufsize);
