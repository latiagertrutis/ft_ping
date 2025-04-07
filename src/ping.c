#include <asm-generic/errno-base.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>

#include "ping_utils.h"

#define HELP_STRING \
"Usage: ft_ping [OPTION...] HOST ...\n" \
"Send ICMP ECHO_REQUEST packets to network hosts.\n" \
"\n" \
"Options:\n" \
"  -v                 verbose output\n" \
"  -?                 give this help list\n"

#define PING_DATALEN	(64 - sizeof(struct icmphdr))

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* Ping options */
#define OPT_VERBOSE 0x01

typedef struct ping_pkt_s {
    struct icmphdr hdr;
    unsigned char data[PING_DATALEN];
} ping_pkt;

typedef struct ping_s {
    int					 fd;
    int					 id;
    host				*dest;
    struct sockaddr_in	 from;
    ping_pkt			 pkt;
    int					 options;
} ping;

static ping* ping_init(int ident) {
    int				 fd;
    struct protoent *proto;
    ping			*p	 = NULL;
    int				 one = 1;

    proto = getprotobyname("icmp");
    if (proto == NULL) {
        /* errno is not set by getprotobyname() */
        errno = ENOPROTOOPT;
        return NULL;
    }

    fd = socket(AF_INET, SOCK_DGRAM, proto->p_proto);
    if (fd < 0) {
        return NULL;
    }

    /* Ping may need to talk with a broadcast address */
    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one)) != 0) {
        goto close_return;
    }

    p = malloc(sizeof(ping));
    if (p == NULL) {
        goto close_return;
    }
    memset(p, 0, sizeof(ping));

    p->fd = fd;
    p->id = ident & 0xFFFF;

    return p;

close_return:
    close(fd);
    return NULL;
}

/* Initializes ping packet with a icmp echo message */
static void ping_create_package(ping *p) {
    ping_pkt *pkt = &p->pkt;

    /* Reset the package */
    memset(pkt, 0, sizeof(ping_pkt));

    pkt->hdr.type = ICMP_ECHO;
    pkt->hdr.un.echo.id = p->id;
    pkt->hdr.un.echo.sequence = 0;
    ping_generate_data(NULL, pkt->data, ARRAY_SIZE(pkt->data));
    /* Last since all data must be set unless checksum which must be all 0 */
    pkt->hdr.checksum = ping_calc_icmp_checksum((uint16_t *)pkt, sizeof(ping_pkt));
}

static int ping_echo(ping * p, char *host) {
    int ret = 0;
    ssize_t bytes;
    bool done;

    ping_create_package(p);
    p->dest = ping_get_host(host);
    if (p->dest == NULL) {
        return -1;
    }

    /* Print the ping data */
    printf ("PING %s (%s): %zu data bytes", p->dest->name,
            inet_ntoa(p->dest->addr.sin_addr), ARRAY_SIZE(p->pkt.data));
    if (p->options & OPT_VERBOSE) {
        printf(", id 0x%04x = %u", p->id, p->id);
    }
    printf ("\n");

    done = false;
    do {
        socklen_t fromlen = sizeof(struct sockaddr_in);

        if (sendto(p->fd, &p->pkt, sizeof(ping_pkt), 0,
                   (struct sockaddr *)&p->dest->addr,
                   sizeof(struct sockaddr_in)) < 0) {
            ret = -1;
            done = true;
            continue;
        }

        bytes = recvfrom(p->fd, &p->pkt, sizeof(ping_pkt), 0,
                         (struct sockaddr *)&p->from, &fromlen);
        if (bytes <= 0) {
            /* In case bytes == 0 peer closed connection, which should not happen */
            ret = -1;
            done = true;
            continue;
        }

        /* Received bytes should at least be equal to the packet size */
        if (bytes < sizeof(ping_pkt)) {
            errno = ERANGE;
            ret = -1;
            done = true;
            continue;
        }

        printf("Received message of size %ld, a ping packet is size %ld\n", bytes, sizeof(ping_pkt));
        done = true; // TODO: fix me
        /* TODO: continue here, with the DGRAM socket we will not receive the IP header as opposed to RAW socket. Next steps: validate received message and finish the loop. */

    } while (!done);

exit_clean:
    free(p->dest->name);
    free(p->dest);
    return ret;
}

int main(int argc, char** argv) {
    int c;
    bool verbose = false;
    ping *p;

    while ((c = getopt(argc, argv, "v?")) != -1) {
        switch (c) {
        case 'v':
            verbose = true;
            break;

        case '?':
            printf(HELP_STRING);
            exit(EXIT_SUCCESS);
        }
    }

    /* Initialize ping structure */
    p = ping_init(getpid());
    if (p == NULL) {
        perror("ping_init");
        exit (EXIT_FAILURE);
    }

    /* Write the options into the ping structure */
    if (verbose) {
        p->options |= OPT_VERBOSE;
    }

    /* Loop through all the hosts */
    for (; optind < argc; optind++) {
        if (ping_echo(p, argv[optind]) != 0) {
            perror("ping_echo");
        }
    }

    close(p->fd);
    free(p);
    return EXIT_SUCCESS;
}
