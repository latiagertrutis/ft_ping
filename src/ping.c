#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/socket.h>
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
    int			 fd;
    int			 type;
    int			 ident;
    host		*dest;
    ping_pkt	 pkt;
    int			 options;
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
    p->ident = ident & 0xFFFF;

    return p;

close_return:
    close(fd);
    return NULL;
}

static int ping_echo(ping * p, char *host) {
    int ret = 0;

    /* Initialize message specific elements */
    p->type = ICMP_ECHO;
    /* p->data; */
    p->dest = ping_get_host(host);
    if (p->dest == NULL) {
        return -1;
    }

    /* Print the ping data */
    printf ("PING %s (%s): %zu data bytes", p->dest->name,
            inet_ntoa(p->dest->addr.sin_addr), ARRAY_SIZE(p->pkt.data));
    if (p->options & OPT_VERBOSE) {
        printf(", id 0x%04x = %u", p->ident, p->ident);
    }
    printf ("\n");

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
