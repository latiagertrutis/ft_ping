#include <netinet/in.h>
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

typedef struct ping_s {
    int fd;
    int type;
    host *dest;
} ping;

static ping* ping_init() {
    int fd;
    struct protoent *proto;
    ping *p = NULL;
    int one = 1;

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

    p->fd = fd;

    return p;

close_return:
    close(fd);
    return NULL;
}

static int ping_echo(ping * p, char *host) {
    int ret = 0;

    p->type = ICMP_ECHO;
    p->dest = ping_get_host(host);
    if (p->dest == NULL) {
        ret = -1;
        goto exit_clean;
    }

    printf("ADDR NAME IS: %s\n", p->dest->name);

exit_clean:
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

    p = ping_init();
    if (p == NULL) {
        perror("ping_init");
        exit (EXIT_FAILURE);
    }

    for (; optind < argc; optind++) {
        if (ping_echo(p, argv[optind]) != 0) {
            perror("ping_echo");
        }
    }

    close(p->fd);
    free(p);
    return EXIT_SUCCESS;
}
