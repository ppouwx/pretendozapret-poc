#include <wups.h>
#include <wups/function_patching.h>
#include <coreinit/time.h>
#include <coreinit/thread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>

#include "config.h"
#include "dns.h"
#include "netfilter.h"
#include "ui.h"
#include "logger.h"

WUPS_PLUGIN_NAME("WiiU-Bypass");
WUPS_PLUGIN_DESCRIPTION("DPI bypass for blocked services on Wii U");
WUPS_PLUGIN_VERSION("v1.0");
WUPS_PLUGIN_AUTHOR("PoC");
WUPS_PLUGIN_LICENSE("MIT");

WUPS_USE_STORAGE("WiiU-Bypass");

// DECL_FUNCTION creates real_<name> pointer + my_<name> prototype
DECL_FUNCTION(int, socket, int domain, int type, int protocol);
DECL_FUNCTION(int, connect, int sockfd, const struct sockaddr *addr, socklen_t addrlen);
DECL_FUNCTION(ssize_t, send, int sockfd, const void *buf, size_t len, int flags);
DECL_FUNCTION(ssize_t, sendto, int sockfd, const void *buf, size_t len, int flags,
              const struct sockaddr *dest_addr, socklen_t addrlen);
DECL_FUNCTION(ssize_t, recv, int sockfd, void *buf, size_t len, int flags);
DECL_FUNCTION(int, close, int fd);
DECL_FUNCTION(int, getaddrinfo, const char *node, const char *service,
              const struct addrinfo *hints, struct addrinfo **res);

int my_socket(int domain, int type, int protocol) {
    int fd = real_socket(domain, type, protocol);
    return NetFilter_RegisterSocket(fd, (type == SOCK_DGRAM));
}

int my_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return NetFilter_Connect(sockfd, addr, addrlen);
}

ssize_t my_send(int sockfd, const void *buf, size_t len, int flags) {
    return NetFilter_Send(sockfd, buf, len, flags);
}

ssize_t my_sendto(int sockfd, const void *buf, size_t len, int flags,
                  const struct sockaddr *dest_addr, socklen_t addrlen) {
    return NetFilter_SendTo(sockfd, buf, len, flags, dest_addr, addrlen);
}

ssize_t my_recv(int sockfd, void *buf, size_t len, int flags) {
    return real_recv(sockfd, buf, len, flags);
}

int my_close(int fd) {
    return NetFilter_Close(fd);
}

int my_getaddrinfo(const char *node, const char *service,
                   const struct addrinfo *hints, struct addrinfo **res) {
    DNS_HandleGetAddrInfo(node, (void *)hints, (void *)res);
    return real_getaddrinfo(node, service, hints, res);
}

// Function patching registrations must be at file scope (not inside functions)
// due to section attribute constraints in GCC 16.x / devkitPPC
WUPS_MUST_REPLACE(socket,      WUPS_LOADER_LIBRARY_NSYSNET, socket);
WUPS_MUST_REPLACE(connect,     WUPS_LOADER_LIBRARY_NSYSNET, connect);
WUPS_MUST_REPLACE(send,        WUPS_LOADER_LIBRARY_NSYSNET, send);
WUPS_MUST_REPLACE(sendto,      WUPS_LOADER_LIBRARY_NSYSNET, sendto);
WUPS_MUST_REPLACE(recv,        WUPS_LOADER_LIBRARY_NSYSNET, recv);
WUPS_MUST_REPLACE(close,       WUPS_LOADER_LIBRARY_NSYSNET, close);
WUPS_MUST_REPLACE(getaddrinfo, WUPS_LOADER_LIBRARY_NSYSNET, getaddrinfo);

INITIALIZE_PLUGIN() {
    Config_Init();
    Log_Init();
}

ON_APPLICATION_START() {
    Config_LoadFromStorage();
    Config_LoadDomainList();
    UI_Init();
    Log_Info("MAIN", "WiiU-Bypass v1.0 starting");
    Log_Info("MAIN", "Strategy: %d, enabled: %d", g_config.strategy, g_config.enabled);

    DNS_Init();
    NetFilter_Init();

    OSSleepTicks(OSNanosecondsToTicks(2000000000ULL));

    if (g_config.show_notification) {
        UI_ShowNotification("WiiU-Bypass v1.0",
            g_config.enabled ? "Status: ACTIVE" : "Status: DISABLED");
    }
}

ON_APPLICATION_ENDS() {
    Log_Info("MAIN", "Application ended, bypass paused");
    NetFilter_Deinit();
    DNS_Deinit();
}

DEINITIALIZE_PLUGIN() {
    Log_Info("MAIN", "WiiU-Bypass shutting down");
    Config_SaveToStorage();
    UI_Deinit();
    Log_Deinit();
}
