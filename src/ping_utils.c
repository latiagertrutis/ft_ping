#include <bits/time.h>
#include <bits/types/struct_timespec.h>
#include <netdb.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ping_utils.h"

#define PING_NSEC_PER_SEC 1000000000

host *ping_get_host(char *hostname)
{
    host * h = NULL;
    struct addrinfo hints, *res;
    int ret;

    /* Configure hints to limit information that will be returned */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_CANONNAME;

    /* Get the requested information */

    ret = getaddrinfo(hostname, NULL, &hints, &res);
    if (ret != 0) {
        return NULL;
    }

    h = malloc(sizeof(host));
    if (h == NULL) {
        return NULL;
    }

    /* Copy information */
    memcpy(&h->addr, res->ai_addr, res->ai_addrlen);
    if (res->ai_canonname != NULL) {
        h->name = strdup(res->ai_canonname);
    }

    /* Clean */
    freeaddrinfo(res);

    return h;
}

/* TODO: Manage pattern lenght */
unsigned char *ping_generate_data(unsigned char * pat, unsigned char *data, size_t len)
{
    size_t i = 0;
    struct timespec now;
    unsigned char *data_p;

    if (len == 0 || data == NULL) {
        return NULL;
    }

    data_p = data;

    /* If there is space add the timing */
    if (len >= sizeof(struct timespec)) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        memcpy(data_p, &now, sizeof(struct timespec));
        data_p += sizeof(struct timespec);
    }

    if (pat != NULL) {
        for (; data_p < data + len; data_p++) {
            *data_p = pat[i];
            if (++i >= len) {
                i = 0;
            }
        }
    }
    else {
        for (; data_p < data + len; data_p++) {
            *data_p = i++;
        }
    }

    return data;
}

/* RFC 792: The checksum is the 16-bit ones's complement of the one's complement
 * sum of the ICMP message starting with the ICMP Type. For computing the
 * checksum , the checksum field should be zero. */
uint16_t ping_calc_icmp_checksum(uint16_t *pkt, size_t len)
{
    uint32_t sum = 0;
    uint16_t *pkt_p = pkt;

    for (; len > 1; pkt_p++, len -= 2) {
        sum += *pkt_p;
    }

    if (len == 1) {
        sum += *(uint8_t *)pkt;
    }

    sum = (sum >> 16) + (sum & 0xffff);	/* First fold */
    sum += (sum >> 16); /* Add carry if any */

    return ~sum;
}

double timespec_to_ms(struct timespec ts)
{
    return (ts.tv_sec * 1000.0) + (ts.tv_nsec / 1000000.0);
}

struct timespec ms_to_timespec(int ms)
{
    return (struct timespec) {
        .tv_sec  = (ms / 1000),
        .tv_nsec = (ms % 1000) * 1000000,
    };
}

struct timespec timespec_substract(struct timespec last, struct timespec now)
{
    return (struct timespec) {
        .tv_sec = last.tv_sec - now.tv_sec,
        .tv_nsec = last.tv_nsec - now.tv_nsec,
    };
}

struct timespec timespec_add(struct timespec last, struct timespec now)
{
    return (struct timespec) {
        .tv_sec = last.tv_sec + now.tv_sec,
        .tv_nsec = last.tv_nsec + now.tv_nsec,
    };
}

struct timespec timespec_normalise(struct timespec ts)
{

    while (ts.tv_nsec < 0) {
        ts.tv_sec--;
        ts.tv_nsec += PING_NSEC_PER_SEC;
    }

    while (ts.tv_nsec >= PING_NSEC_PER_SEC) {
        ts.tv_sec++;
        ts.tv_nsec -= PING_NSEC_PER_SEC;
    }

    if (ts.tv_sec < 0) {
        ts.tv_sec = ts.tv_nsec = 0;
    }

    return ts;
}

double nabs (double a)
{
    return (a < 0) ? -a : a;
}

double nsqrt (double a, double prec)
{
    double x0, x1;

    if (a < 0) {
        return 0;
    }
    if (a < prec) {
        return 0;
    }
    x1 = a / 2;
    do {
        x0 = x1;
        x1 = (x0 + a / x0) / 2;
    } while (nabs (x1 - x0) > prec);

    return x1;
}
