#include <netdb.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ping_utils.h"

#define PING_NSEC_PER_SEC 1000000000

host * ping_get_host(char *hostname) {
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

unsigned char * ping_generate_data(unsigned char * pat, unsigned char *data, size_t len) {
    size_t i = 0;

    if (len == 0 || data == NULL) {
        return NULL;
    }

    if (pat != NULL) {
        unsigned char * data_p;

        for (data_p = data; data_p < data + len; data_p++) {
            *data_p = pat[i];
            if (++i >= len) {
                i = 0;
            }
        }
    } else {
        for (i = 0; i < len; i++) {
            data[i] = i;
        }
    }

    return data;
}

/* RFC 792: The checksum is the 16-bit ones's complement of the one's complement
 * sum of the ICMP message starting with the ICMP Type. For computing the
 * checksum , the checksum field should be zero. */
uint16_t ping_calc_icmp_checksum(uint16_t *pkt, size_t len) {
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

    printf("Calculated: %X\n", ~sum);

    return ~sum;
}

int timespec_to_ms(struct timespec ts) {
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

struct timespec ms_to_timespec(int ms) {
    return (struct timespec) {
        .tv_sec  = (ms / 1000),
        .tv_nsec = (ms % 1000) * 1000000,
    };
}

struct timespec timespec_substract(struct timespec last, struct timespec now) {
    return (struct timespec) {
        .tv_sec = last.tv_sec - now.tv_sec,
        .tv_nsec = last.tv_nsec - now.tv_nsec,
    };
}

struct timespec timespec_add(struct timespec last, struct timespec now) {
    return (struct timespec) {
        .tv_sec = last.tv_sec + now.tv_sec,
        .tv_nsec = last.tv_nsec + now.tv_nsec,
    };
}

struct timespec timespec_normalise(struct timespec ts) {

    while(ts.tv_nsec < 0) {
        ts.tv_sec--;
        ts.tv_nsec += PING_NSEC_PER_SEC;
    }

    while(ts.tv_nsec >= PING_NSEC_PER_SEC) {
        ts.tv_sec++;
        ts.tv_nsec -= PING_NSEC_PER_SEC;
    }

    if(ts.tv_sec < 0) {
        ts.tv_sec = ts.tv_nsec = 0;
    }

    return ts;
}
