#include "dns.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <nsysnet/socket.h>

typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;

typedef struct __attribute__((packed)) {
    uint16_t type;
    uint16_t qclass;
} dns_question_t;

typedef struct __attribute__((packed)) {
    uint16_t name_ptr;
    uint16_t type;
    uint16_t dclass;
    uint32_t ttl;
    uint16_t rdlength;
    uint32_t rdata;
} dns_answer_t;

#define DNS_BUFFER_SIZE 1024
#define DNS_TIMEOUT_MS 5000
#define CACHE_SIZE 64

typedef struct {
    char domain[MAX_DOMAIN_LEN];
    uint32_t ipv4;
    uint64_t timestamp_ms;
} dns_cache_entry_t;

static dns_cache_entry_t s_cache[CACHE_SIZE];
static int s_cache_count = 0;
static uint16_t s_next_id = 1;
static bool s_initialized = false;

static uint64_t millis(void) {
    return OSTicksToMilliseconds(OSGetTime());
}

static bool parse_ipv4(const char *str, uint32_t *out) {
    uint8_t octets[4];
    int count = 0;
    const char *p = str;

    while (*p && count < 4) {
        char *end;
        long val = strtol(p, &end, 10);
        if (end == p || val < 0 || val > 255) return false;
        octets[count++] = (uint8_t)val;
        p = end;
        if (*p == '.') p++;
        else if (*p != '\0') return false;
    }

    if (count != 4) return false;
    *out = (octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3];
    return true;
}

static void dns_name_to_wire(const char *name, uint8_t *out, int *outlen) {
    int pos = 0;
    while (*name) {
        const char *dot = strchr(name, '.');
        int seglen = dot ? (int)(dot - name) : (int)strlen(name);
        out[pos++] = (uint8_t)seglen;
        memcpy(out + pos, name, seglen);
        pos += seglen;
        name = dot ? dot + 1 : name + seglen;
    }
    out[pos++] = 0;
    *outlen = pos;
}

static bool resolve_direct_udp(const char *domain, uint32_t *out_ip) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return false;

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(53);

    if (!parse_ipv4(g_config.doh_server, (uint32_t *)&server.sin_addr.s_addr)) {
        server.sin_addr.s_addr = htonl(0x01010101); // fallback 1.1.1.1
    }

    uint8_t buf[DNS_BUFFER_SIZE];
    memset(buf, 0, sizeof(buf));

    dns_header_t *hdr = (dns_header_t *)buf;
    hdr->id = htons(s_next_id++);
    hdr->flags = htons(0x0100);
    hdr->qdcount = htons(1);

    int name_len;
    dns_name_to_wire(domain, buf + sizeof(dns_header_t), &name_len);

    dns_question_t *q = (dns_question_t *)(buf + sizeof(dns_header_t) + name_len);
    q->type = htons(1);
    q->qclass = htons(1);

    int qlen = sizeof(dns_header_t) + name_len + sizeof(dns_question_t);

    if (sendto(sock, buf, qlen, 0, (struct sockaddr *)&server, sizeof(server)) < 0) {
        close(sock);
        return false;
    }

    struct timeval tv;
    tv.tv_sec = DNS_TIMEOUT_MS / 1000;
    tv.tv_usec = (DNS_TIMEOUT_MS % 1000) * 1000;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        close(sock);
        return false;
    }

    uint8_t resp[DNS_BUFFER_SIZE];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);

    int rlen = recvfrom(sock, resp, sizeof(resp), 0, (struct sockaddr *)&from, &fromlen);
    close(sock);

    if (rlen < (int)(sizeof(dns_header_t) + name_len + sizeof(dns_question_t) + (int)sizeof(dns_answer_t))) {
        return false;
    }

    dns_header_t *rhdr = (dns_header_t *)resp;
    if (ntohs(rhdr->ancount) == 0) return false;

    int offset = sizeof(dns_header_t);
    while (offset < rlen) {
        if (resp[offset] == 0) { offset++; break; }
        if (resp[offset] & 0xC0) { offset += 2; break; }
        offset += resp[offset] + 1;
    }
    offset += sizeof(dns_question_t);

    for (int i = 0; i < ntohs(rhdr->ancount) && offset + 12 <= rlen; i++) {
        if (resp[offset] & 0xC0) {
            offset += 2;
        } else {
            while (offset < rlen && resp[offset] != 0) offset += resp[offset] + 1;
            if (offset >= rlen) break;
            offset++;
        }

        if (offset + 10 > rlen) break;
        uint16_t atype = ntohs(*(uint16_t *)(resp + offset));
        offset += 8;
        uint16_t rdlen = ntohs(*(uint16_t *)(resp + offset));
        offset += 2;

        if (atype == 1 && rdlen == 4 && offset + 4 <= rlen) {
            *out_ip = *(uint32_t *)(resp + offset);
            return true;
        }

        offset += rdlen;
    }

    return false;
}

static void cache_result(const char *domain, uint32_t ip) {
    if (s_cache_count >= CACHE_SIZE) {
        s_cache_count = 0;
    }
    strncpy(s_cache[s_cache_count].domain, domain, MAX_DOMAIN_LEN - 1);
    s_cache[s_cache_count].domain[MAX_DOMAIN_LEN - 1] = '\0';
    s_cache[s_cache_count].ipv4 = ip;
    s_cache[s_cache_count].timestamp_ms = millis();
    s_cache_count++;
}

static bool cache_lookup(const char *domain, uint32_t *out_ip) {
    for (int i = 0; i < s_cache_count; i++) {
        if (strcasecmp(s_cache[i].domain, domain) == 0) {
            if (millis() - s_cache[i].timestamp_ms < 300000) {
                *out_ip = s_cache[i].ipv4;
                return true;
            }
            for (int j = i; j < s_cache_count - 1; j++) {
                s_cache[j] = s_cache[j + 1];
            }
            s_cache_count--;
            return false;
        }
    }
    return false;
}

bool DNS_HandleGetAddrInfo(const char *node, void *hints, void *res) {
    if (!g_config.enabled || !node) return false;
    if (!Config_IsDomainListed(node)) return false;

    uint32_t cached_ip;
    if (cache_lookup(node, &cached_ip)) {
        return false;
    }

    uint32_t resolved_ip = 0;
    if (resolve_direct_udp(node, &resolved_ip)) {
        cache_result(node, resolved_ip);
    }

    return false;
}

bool DNS_Init(void) {
    s_initialized = true;
    s_cache_count = 0;
    return true;
}

void DNS_Deinit(void) {
    s_initialized = false;
}
