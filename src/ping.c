#include <netinet/in.h>
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

#define ICMP_ECHO		8
#define PING_HEADER_LEN 8
#define PING_DATALEN	(64 - PING_HEADER_LEN)	/* default data length */

/* Ping options */
#define OPT_VERBOSE 0x01

typedef struct ping_s {
    int				 fd;
    int				 type;
    unsigned short	 ident;
    host			*dest;
    size_t			 datalen;
    char			 options;
} ping;

static ping* ping_init(unsigned short ident) {
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
    p->ident = ident;

    return p;

close_return:
    close(fd);
    return NULL;
}

static int ping_echo(ping * p, char *host) {
    int ret = 0;

    p->type = ICMP_ECHO;
    p->datalen = PING_DATALEN;
    p->dest = ping_get_host(host);
    if (p->dest == NULL) {
        ret = -1;
        goto exit_clean;
    }

    printf ("PING %s (%s): %zu data bytes", p->dest->name,
            inet_ntoa(p->dest->addr.sin_addr), p->datalen);
    if (p->options & OPT_VERBOSE) {
        printf (", id 0x%04x = %u", p->ident, p->ident);
    }
    printf ("\n");

exit_clean:
    free(p->dest);
    return ret;
}

int main(int argc, char** argv) {
    int c;
    bool verbose;
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
    p->options |= verbose ? OPT_VERBOSE : 0x00;

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
