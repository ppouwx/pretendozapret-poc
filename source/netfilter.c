#include "netfilter.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <nsysnet/socket.h>

socket_entry_t g_socket_table[MAX_TRACKED_SOCKETS];

// real_* function pointers are declared extern in netfilter.h
// and DEFINED by DECL_FUNCTION() in main.c.
// WUPS FunctionPatcherModule initializes them at load time.

// Fake TLS ClientHello for benign domain (www.google.com)
static const uint8_t FAKE_TLS_CLIENTHELLO[] = {
    0x16, 0x03, 0x01, 0x00, 0xD6,
    0x01, 0x00, 0x00, 0xD2,
    0x03, 0x03,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00,
    0x00, 0x02,
    0xC0, 0x2B,
    0x01,
    0x00,
    0x00, 0x8B,
    0x00, 0x00, 0x00, 0x13, 0x00, 0x11, 0x00, 0x00,
    0x0E, 0x77, 0x77, 0x77, 0x2E, 0x67, 0x6F, 0x6F,
    0x67, 0x6C, 0x65, 0x2E, 0x63, 0x6F, 0x6D,
    0x00, 0x0A, 0x00, 0x04, 0x00, 0x02, 0x00, 0x1D,
    0x00, 0x0D, 0x00, 0x14, 0x00, 0x12, 0x04, 0x03,
    0x08, 0x04, 0x04, 0x01, 0x05, 0x03, 0x08, 0x05,
    0x05, 0x01, 0x08, 0x06, 0x06, 0x01, 0x02, 0x01,
    0x00, 0x2B, 0x00, 0x03, 0x02, 0x03, 0x04,
    0x00, 0x34, 0x00, 0x32, 0x00, 0x1D, 0x00, 0x30,
    0x04, 0x14, 0xD5, 0x96, 0x21, 0x7B, 0x44, 0x14,
    0x9F, 0x43, 0xAD, 0x47, 0x15, 0xF9, 0x4E, 0x19,
    0x34, 0x08, 0xE6, 0x9F, 0x55, 0x41, 0x96, 0xD8,
    0x3F, 0x50, 0x8E, 0x35, 0x3A, 0xB0, 0x6A, 0x79,
    0x83, 0x02, 0x47, 0xCC, 0xA0, 0x8D, 0x38, 0xDC,
    0xA4, 0x2E, 0x0C, 0xD2, 0xC2, 0x08, 0x06, 0x05,
    0xBF, 0x10
};

static int s_initialized = 0;

static ssize_t strategy_split(int sockfd, const void *buf, size_t len, int flags);
static ssize_t strategy_multisplit(int sockfd, const void *buf, size_t len, int flags);
static ssize_t strategy_fakedsplit(int sockfd, const void *buf, size_t len, int flags);
static ssize_t strategy_delay(int sockfd, const void *buf, size_t len, int flags);

static void sleep_us(uint32_t us) {
    if (us == 0) return;
    OSSleepTicks(OSNanosecondsToTicks((uint64_t)us * 1000));
}

static int find_slot(void) {
    for (int i = 0; i < MAX_TRACKED_SOCKETS; i++) {
        if (g_socket_table[i].fd == SOCKET_TABLE_EMPTY) return i;
    }
    return -1;
}

static int find_slot_by_fd(int fd) {
    for (int i = 0; i < MAX_TRACKED_SOCKETS; i++) {
        if (g_socket_table[i].fd == fd) return i;
    }
    return -1;
}

bool NetFilter_Init(void) {
    if (s_initialized) return true;

    for (int i = 0; i < MAX_TRACKED_SOCKETS; i++) {
        g_socket_table[i].fd = SOCKET_TABLE_EMPTY;
        g_socket_table[i].tracked = false;
    }

    s_initialized = 1;
    return true;
}

void NetFilter_Deinit(void) {
    s_initialized = 0;
}

int NetFilter_RegisterSocket(int fd, bool is_udp) {
    if (fd < 0) return fd;

    int slot = find_slot();
    if (slot < 0) return fd;

    g_socket_table[slot].fd = fd;
    g_socket_table[slot].is_udp = is_udp;
    g_socket_table[slot].is_ssl = false;
    g_socket_table[slot].tracked = true;
    g_socket_table[slot].dest_ip = 0;
    g_socket_table[slot].dest_port = 0;

    memset(&g_socket_table[slot].dest_addr, 0, sizeof(g_socket_table[slot].dest_addr));

    return fd;
}

void NetFilter_ResetSocket(int fd) {
    int slot = find_slot_by_fd(fd);
    if (slot >= 0) {
        g_socket_table[slot].fd = SOCKET_TABLE_EMPTY;
        g_socket_table[slot].tracked = false;
    }
}

bool NetFilter_ShouldFilter(const socket_entry_t *entry) {
    if (!entry || !entry->tracked) return false;
    if (!g_config.enabled) return false;
    return true;
}

int NetFilter_Connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    int slot = find_slot_by_fd(sockfd);
    if (slot >= 0 && addr && addr->sa_family == AF_INET) {
        const struct sockaddr_in *in = (const struct sockaddr_in *)addr;
        g_socket_table[slot].dest_addr = *in;
        g_socket_table[slot].dest_ip = in->sin_addr.s_addr;
        g_socket_table[slot].dest_port = ntohs(in->sin_port);
        g_socket_table[slot].tracked = true;
    }

    return real_connect ? real_connect(sockfd, addr, addrlen) : -1;
}

ssize_t NetFilter_Send(int sockfd, const void *buf, size_t len, int flags) {
    if (!g_config.enabled || len == 0 || !real_send) {
        return real_send ? real_send(sockfd, buf, len, flags) : -1;
    }

    int slot = find_slot_by_fd(sockfd);
    if (slot < 0 || !g_socket_table[slot].tracked) {
        return real_send(sockfd, buf, len, flags);
    }

    socket_entry_t *entry = &g_socket_table[slot];

    if (entry->dest_port != 443 && entry->dest_port != 80 && entry->dest_port != 0) {
        return real_send(sockfd, buf, len, flags);
    }

    switch (g_config.strategy) {
    case STRATEGY_SPLIT:
        return strategy_split(sockfd, buf, len, flags);
    case STRATEGY_MULTISPLIT:
        return strategy_multisplit(sockfd, buf, len, flags);
    case STRATEGY_FAKEDSPLIT:
        return strategy_fakedsplit(sockfd, buf, len, flags);
    case STRATEGY_DELAY:
        return strategy_delay(sockfd, buf, len, flags);
    case STRATEGY_RAW:
    default:
        return real_send(sockfd, buf, len, flags);
    }
}

ssize_t NetFilter_SendTo(int sockfd, const void *buf, size_t len, int flags,
                          const struct sockaddr *dest_addr, socklen_t addrlen) {
    if (!g_config.enabled || !g_config.udp_bypass || len == 0 || !real_sendto) {
        return real_sendto ? real_sendto(sockfd, buf, len, flags, dest_addr, addrlen) : -1;
    }

    int slot = find_slot_by_fd(sockfd);
    if (slot < 0) {
        return real_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
    }

    uint16_t port = 0;
    if (dest_addr && dest_addr->sa_family == AF_INET) {
        port = ntohs(((const struct sockaddr_in *)dest_addr)->sin_port);
    } else {
        port = g_socket_table[slot].dest_port;
    }

    if (port < g_config.udp_fake_port_start || port > g_config.udp_fake_port_end) {
        return real_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
    }

    uint8_t fake_padding[32] = {0};
    real_sendto(sockfd, fake_padding, sizeof(fake_padding), flags, dest_addr, addrlen);
    sleep_us(2000);

    size_t offset = 0;
    size_t frag = 64;
    int total = 0;

    while (offset < len) {
        size_t chunk = (len - offset > frag) ? frag : (len - offset);
        ssize_t sent = real_sendto(sockfd, (const uint8_t *)buf + offset, chunk,
                                    flags, dest_addr, addrlen);
        if (sent < 0) return total > 0 ? total : sent;
        total += sent;
        offset += chunk;
        sleep_us(500);
    }

    return total;
}

ssize_t NetFilter_Recv(int sockfd, void *buf, size_t len, int flags) {
    return real_recv ? real_recv(sockfd, buf, len, flags) : -1;
}

int NetFilter_Close(int fd) {
    NetFilter_ResetSocket(fd);
    return real_close ? real_close(fd) : -1;
}

// ========== STRATEGY IMPLEMENTATIONS ==========

static ssize_t strategy_split(int sockfd, const void *buf, size_t len, int flags) {
    size_t frag_size = g_config.fragment_size;
    if (frag_size == 0) frag_size = 2;
    if (frag_size > len) frag_size = len;

    int total_sent = 0;

    ssize_t sent = real_send(sockfd, buf, frag_size, flags);
    if (sent < 0) return sent;
    total_sent += sent;

    sleep_us(g_config.inter_fragment_delay_us);

    if ((size_t)total_sent < len) {
        sent = real_send(sockfd, (const uint8_t *)buf + total_sent,
                         len - total_sent, flags);
        if (sent < 0) return total_sent > 0 ? total_sent : sent;
        total_sent += sent;
    }

    return total_sent;
}

static ssize_t strategy_multisplit(int sockfd, const void *buf, size_t len, int flags) {
    if (len < 10) return real_send(sockfd, buf, len, flags);

    size_t overlap = g_config.split_position > 0 ? g_config.split_position : 5;
    if (overlap >= len) overlap = len / 2;

    ssize_t s1 = real_send(sockfd, buf, overlap, flags);
    if (s1 < 0) return s1;
    sleep_us(g_config.inter_fragment_delay_us);

    size_t resume = (overlap > 2) ? overlap - 2 : 0;
    ssize_t s2 = real_send(sockfd, (const uint8_t *)buf + resume,
                            len - resume, flags);
    if (s2 < 0) return (ssize_t)len;

    return (ssize_t)len;
}

static ssize_t strategy_fakedsplit(int sockfd, const void *buf, size_t len, int flags) {
    real_send(sockfd, FAKE_TLS_CLIENTHELLO, sizeof(FAKE_TLS_CLIENTHELLO), flags);
    sleep_us(g_config.inter_fragment_delay_us * 2);

    return strategy_split(sockfd, buf, len, flags);
}

static ssize_t strategy_delay(int sockfd, const void *buf, size_t len, int flags) {
    int total_sent = 0;

    for (size_t offset = 0; offset < len; offset++) {
        ssize_t sent = real_send(sockfd, (const uint8_t *)buf + offset, 1, flags);
        if (sent < 0) return total_sent > 0 ? total_sent : sent;
        total_sent += sent;
        sleep_us(g_config.inter_fragment_delay_us * 5);
    }

    return total_sent;
}
