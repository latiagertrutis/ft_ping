#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <poll.h>

#include "ping_utils.h"

#define HELP_STRING \
"Usage: ft_ping [OPTION...] HOST ...\n" \
"Send ICMP ECHO_REQUEST packets to network hosts.\n" \
"\n" \
"Options:\n" \
"  -v                 verbose output\n" \
"  -i <interval>      interval in seconds between ping messages [default 1s]\n" \
"  -c <count>         number of messages to send, 0 is infinity [default 0]\n" \
"  -?                 give this help list\n"

#define PING_DATALEN	(64 - sizeof(struct icmphdr))
#define PING_DEFAULT_INTERVAL 1000      /* Milliseconds */
#define PING_MS_PER_SEC 1000     /* Millisecond precision */
#define PING_MAX_WAIT (10 * PING_MS_PER_SEC)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#ifndef IP_HDRLEN_MAX
/* According to the RFC 791, section 3.1, the header length is 4 bits that
 * encodes the length of the header in 32 bit words. So the maximum header
 * lenght is 0xF (max value of 4 bits) times 4 (<< 2) to translate from
 * 32-bit words to the usual 8-bit words. */
#define IP_HDRLEN_MAX (0xF << 2)
#endif

/* Ping options */
#define OPT_VERBOSE 0x01

typedef struct ping_pkt_s {
    struct icmphdr hdr;
    unsigned char data[PING_DATALEN];
} ping_pkt;

typedef struct ping_s {
    int					 fd;
    bool				 is_dgram;
    int					 id;
    host				*dest;
    struct sockaddr_in	 from;
    ping_pkt			 pkt;
    size_t				 interval;
    size_t				 count;
    size_t				 num_sent;
    size_t				 num_recv;
    int					 options;
} ping;

/* TODO: remove */
void print_bytes_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);													 // Uppercase hex, 2 digits padded with zero
    }
    printf("\n");
}

static ping* ping_init(int ident) {
    int				 fd;
    struct protoent *proto;
    ping			*p	 = NULL;
    int				 one = 1;
    bool is_dgram = false;

    proto	  = getprotobyname("icmp");
    if (proto == NULL) {
        /* errno is not set by getprotobyname() */
        errno = ENOPROTOOPT;
        return NULL;
    }

    /* Note: If we use SOCK_DGRAM for the socket the kernel will automatically
     * handle IP layer headers, and may also override parts of the ICMP header
     * such as indentifier and sequence. If full control is wanted SOCK_RAW
     * must be used, which will require to give network capabilities to the
     * binary */
    fd = socket(AF_INET, SOCK_RAW, proto->p_proto);
    if (fd < 0) {
        if (errno != EPERM && errno != EACCES) {
            return NULL;
        }

        errno = 0;
        fd	  = socket(AF_INET, SOCK_DGRAM, proto->p_proto);
        if (fd < 0) {
            return NULL;
        }
        is_dgram = true;
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
    p->is_dgram = is_dgram;

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
    pkt->hdr.un.echo.id = htons(p->id);
    pkt->hdr.un.echo.sequence = htons(p->num_sent); //todo: add sequence number
    ping_generate_data(NULL, pkt->data, ARRAY_SIZE(pkt->data));
    /* Last since all data must be set unless checksum which must be all 0 */
    pkt->hdr.checksum = ping_calc_icmp_checksum((uint16_t *)pkt, sizeof(ping_pkt));
}

static bool ping_validate_icmp_pkg(ping *p, uint8_t *data, size_t len, ping_pkt **pkt) {
    size_t hlen = 0;
    uint16_t chksum;

    if (!p->is_dgram) {
        /* Translate 32-bit words to 8-bit (RFC791, 3.1) */
        hlen = ((struct ip *)data)->ip_hl << 2;
    }

    /* Received bytes should at least be equal to the packet size */
    if (len < hlen + sizeof(ping_pkt)) {
        return false;
    }

    *pkt = (ping_pkt *)(data + hlen);

    /* Validate checksum */
    chksum = (*pkt)->hdr.checksum;
    (*pkt)->hdr.checksum = 0;
    (*pkt)->hdr.checksum = ping_calc_icmp_checksum((uint16_t *)*pkt, len - hlen);
    if ((*pkt)->hdr.checksum != chksum) {
        return false;
    }

    return true;
}

static ssize_t ping_send(ping *p) {
    ping_pkt *pkt = &p->pkt;
    ssize_t bytes = 0;

    ping_create_package(p);

    bytes = sendto(p->fd, pkt, sizeof(ping_pkt), 0,
                   (struct sockaddr *)&p->dest->addr,
                   sizeof(struct sockaddr_in));
    if ( bytes < 0) {
        return -1;
    }

    p->num_sent++;

    /* TODO: remove */
    printf("Sent message:\n");
    print_bytes_hex((const uint8_t *)&p->pkt, sizeof(ping_pkt));

    return bytes;
}

static ssize_t ping_recv(ping *p) {
    ssize_t bytes = 0;
    uint8_t recv_buff[IP_HDRLEN_MAX + sizeof(ping_pkt)];
    socklen_t fromlen = sizeof(struct sockaddr_in);
    ping_pkt *pkt;
    uint16_t seq;

    bytes = recvfrom(p->fd, recv_buff, ARRAY_SIZE(recv_buff), 0,
                     (struct sockaddr *)&p->from, &fromlen);
    if (bytes <= 0) {
        /* In case bytes == 0 peer closed connection, which should not happen */
        return -1;
    }

    if (!ping_validate_icmp_pkg(p, recv_buff, bytes, &pkt)) {
        goto exit_badmsg;
    }

    /* Validate identity (only raw mode) */
    if (!p->is_dgram && (ntohs(pkt->hdr.un.echo.id) != p->id)) {
        printf("Error id recv [%X], local [%X]\n", ntohs(pkt->hdr.un.echo.id), p->id);
        goto exit_badmsg;
    }

    /* Validate sequence number, should be one less than the messages transmitted */
    seq = ntohs(pkt->hdr.un.echo.sequence);
    /* TODO: Handle duplicates? */
    if (seq >= p->num_sent) {
        printf("Error sequence [%d]\n", seq);
        goto exit_badmsg;
    }

    p->num_recv++;

    /* TODO: Remove */
    printf("Received message:\n");
    print_bytes_hex((const uint8_t *)recv_buff, bytes);

    return bytes;

exit_badmsg:
    errno = EBADMSG;
    return -1;
}

static int ping_run(ping * p, char *host) {
    int ret = 0;
    ssize_t bytes;
    int wait;
    bool done;
    bool finishing;
    struct pollfd pfd;

    p->dest = ping_get_host(host);
    if (p->dest == NULL) {
        return -1;
    }

    pfd.fd = p->fd;
    pfd.events = POLLIN;

    /* Print the ping data */
    printf ("PING %s (%s): %zu data bytes", p->dest->name,
            inet_ntoa(p->dest->addr.sin_addr), ARRAY_SIZE(p->pkt.data));
    if (p->options & OPT_VERBOSE) {
        printf(", id 0x%04x = %u", p->id, p->id);
    }
    printf ("\n");

    if (ping_send(p) < 0) {
        ret = -1;
        goto exit_clean;
    }

    done = false;
    finishing = false;
    wait = p->interval;
    while (!done) {
        uint16_t chksum;
        int pret;

        pret = poll(&pfd, 1, wait);
        if (pret < 0) {
            ret = -1;
            done = true;
            continue;
        }

        if (pret > 0) {
            if (ping_recv(p) < 0) {
                ret = -1;
                done = true;
                continue;
            }

            if (p->num_recv >= p->count) {
                done = true;
            }

        } else {
            if (p->count == 0 || p->num_sent < p->count) {
                if (ping_send(p) < 0) {
                    ret = -1;
                    done = true;
                    continue;
                }
            } else if (finishing) {
                done = true;
            } else {
                finishing = true;
                wait = PING_MAX_WAIT;
            }
        }
    }

exit_clean:
    free(p->dest->name);
    free(p->dest);
    return ret;
}

int main(int argc, char** argv) {
    int c;
    bool verbose = false;
    size_t interval = PING_DEFAULT_INTERVAL;
    size_t count = 0;
    ping *p;

    while ((c = getopt(argc, argv, "vi:c:?")) != -1) {
        switch (c) {
        case 'v':
            verbose = true;
            break;

        case 'i':
            /* TODO: Use float */
            interval = atol(optarg) * PING_MS_PER_SEC;
            break;

        case 'c':
            count = atol(optarg);
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
    p->interval = interval;
    p->count = count;

    /* Loop through all the hosts */
    for (; optind < argc; optind++) {
        if (ping_run(p, argv[optind]) != 0) {
            perror("ping_echo");
        }
    }

    close(p->fd);
    free(p);
    return EXIT_SUCCESS;
}
