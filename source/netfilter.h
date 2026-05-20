#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MAX_TRACKED_SOCKETS 128
#define SOCKET_TABLE_EMPTY -1

typedef struct {
    int fd;
    struct sockaddr_in dest_addr;
    uint32_t dest_ip;
    uint16_t dest_port;
    bool is_udp;
    bool is_ssl;
    bool tracked;
} socket_entry_t;

extern socket_entry_t g_socket_table[MAX_TRACKED_SOCKETS];

// Real (unhooked) system function pointers, initialized by
// the WUPS FunctionPatcherModule at load time.
// These are DEFINED by DECL_FUNCTION() in main.c.
extern int (*real_socket)(int, int, int);
extern int (*real_connect)(int, const struct sockaddr *, socklen_t);
extern ssize_t (*real_send)(int, const void *, size_t, int);
extern ssize_t (*real_sendto)(int, const void *, size_t, int,
                              const struct sockaddr *, socklen_t);
extern ssize_t (*real_recv)(int, void *, size_t, int);
extern int (*real_close)(int);

bool NetFilter_Init(void);
void NetFilter_Deinit(void);

int NetFilter_RegisterSocket(int fd, bool is_udp);
int NetFilter_Connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t NetFilter_Send(int sockfd, const void *buf, size_t len, int flags);
ssize_t NetFilter_SendTo(int sockfd, const void *buf, size_t len, int flags,
                         const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t NetFilter_Recv(int sockfd, void *buf, size_t len, int flags);
int NetFilter_Close(int fd);

void NetFilter_ResetSocket(int fd);
bool NetFilter_ShouldFilter(const socket_entry_t *entry);
