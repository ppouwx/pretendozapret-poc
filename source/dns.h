#pragma once
#include <stdbool.h>

bool DNS_Init(void);
void DNS_Deinit(void);

// Hooked getaddrinfo - returns false if real function should be called
bool DNS_HandleGetAddrInfo(const char *node, void *hints, void *res);
