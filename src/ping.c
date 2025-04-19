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
#include <math.h>
#include <signal.h>

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

#define PING_DATALEN    (64 - sizeof(struct icmphdr))
#define PING_DEFAULT_INTERVAL 1000.0      /* Milliseconds */
#define PING_MS_PER_SEC 1000     /* Millisecond precision */
#define PING_MIN_INTERVAL 0.2
#define PING_MAX_WAIT (10 * PING_MS_PER_SEC)
#define PING_SEQMAP_SIZE 128

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

typedef struct ping_stat_s {
    double tmin;                  /* minimum round trip time */
    double tmax;                  /* maximum round trip time */
    double tsum;                  /* sum of all times, for doing average */
    double tsumsq;                /* sum of all times squared, for std. dev. */
} ping_stat;


typedef struct ping_s {
    int                  fd;
    bool                 is_dgram;
    int                  id;
    host                *dest;
    ping_pkt             pkt;
    ping_stat			 stat;
    uint8_t				 seq_map[PING_SEQMAP_SIZE];
    size_t               interval;
    size_t               count;
    size_t               num_sent;
    size_t               num_recv;
    size_t               num_dup;
    int                  options;
} ping;

static ping *ping_init(int ident)
{
    int              fd;
    struct protoent *proto;
    ping            *p   = NULL;
    int              one = 1;
    bool is_dgram = false;

    proto = getprotobyname("icmp");
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
        fd    = socket(AF_INET, SOCK_DGRAM, proto->p_proto);
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

/*
 * NOTE: The inetutils-2.0 does not take into consideration if the socket is
 * DGRAM or RAW at the moment of assigning the ip header when decoding the
 * message. This makes that if the socket is DGRAM the ip header will be
 * pointing to the start of the icmp header, thus making the ttl and other
 * data being random numbers. This I understand is just made as undefined
 * behavior, and I preferred to set ttl to 0 instead.
 */
static void ping_print_echo(bool dupflag, struct sockaddr_in *from, struct ip *ip,
                            ping_stat *stat, ping_pkt *pkt, int len)
{
    bool timing = false;
    double triptime = 0.0;
    uint8_t ttl = 0;

    if (ip != NULL) {
        ttl = ip->ip_ttl;
        len  = len - (ip->ip_hl << 2);
    }

    if (len - sizeof(struct icmphdr) >= sizeof(struct timespec)) {
        struct timespec now, before;

        timing = true;

        clock_gettime(CLOCK_MONOTONIC, &now);

        /* Copy to avoid missalignement */
        memcpy(&before, pkt->data, sizeof(struct timespec));
        triptime = timespec_to_ms(timespec_substract(now, before));

        /* Update statistics */
        stat->tsum += triptime;
        stat->tsumsq += triptime * triptime;
        if (triptime < stat->tmin) {
            stat->tmin = triptime;
        }
        if (triptime > stat->tmax) {
            stat->tmax = triptime;
        }
    }

    printf ("%d bytes from %s: icmp_seq=%u", len,
            inet_ntoa (*(struct in_addr*) &from->sin_addr.s_addr),
            ntohs (pkt->hdr.un.echo.sequence));
    printf (" ttl=%d", ttl);

    if (timing) {
        printf (" time=%.3f ms", triptime);
    }

    if (dupflag) {
        printf (" (DUP!)");
    }

    printf ("\n");
}

static void ping_print_stat(ping *p)
{
    fflush (stdout);
    printf ("--- %s ping statistics ---\n", p->dest->name);
    printf ("%zu packets transmitted, ", p->num_sent);
    printf ("%zu packets received, ", p->num_recv);

    /* TODO: handle duplicates */
    /* if (ping->ping_num_rept) */
    /*     printf ("+%zu duplicates, ", ping->ping_num_rept); */
    if (p->num_sent) {
        if (p->num_recv > p->num_sent) {
            printf ("-- somebody is printing forged packets!");
        }
        else {
            printf ("%d%% packet loss",
                    (int) (((p->num_sent - p->num_recv) * 100) / p->num_sent));
        }
    }
    printf ("\n");

    if (p->num_recv && ARRAY_SIZE(p->pkt.data) >= sizeof(struct timespec)) {
        double total = p->num_recv + p->num_dup;
        double avg = p->stat.tsum / total;
        double vari = p->stat.tsumsq / total - avg * avg;

        printf ("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
                p->stat.tmin, avg, p->stat.tmax, nsqrt (vari, 0.0005));
    }
}


/* Initializes ping packet with a icmp echo message */
static void ping_create_package(ping *p)
{
    ping_pkt *pkt = &p->pkt;

    /* Reset the package */
    memset(pkt, 0, sizeof(ping_pkt));

    pkt->hdr.type = ICMP_ECHO;
    pkt->hdr.un.echo.id = htons(p->id);
    pkt->hdr.un.echo.sequence = htons(p->num_sent);
    ping_generate_data(NULL, pkt->data, ARRAY_SIZE(pkt->data));
    /* Last since all data must be set unless checksum which must be all 0 */
    pkt->hdr.checksum = ping_calc_icmp_checksum((uint16_t*)pkt, sizeof(ping_pkt));
}

static bool ping_validate_icmp_pkg(ping *p, uint8_t* data, size_t len, ping_pkt** pkt)
{
    size_t hlen = 0;
    uint16_t chksum;

    if (!p->is_dgram) {
        /* Translate 32-bit words to 8-bit (RFC791, 3.1) */
        hlen = ((struct ip*)data)->ip_hl << 2;
    }

    /* Received bytes should at least be equal to the packet size */
    if (len < hlen + sizeof(ping_pkt)) {
        return false;
    }

    *pkt = (ping_pkt*)(data + hlen);

    /* Validate checksum */
    chksum = (*pkt)->hdr.checksum;
    (*pkt)->hdr.checksum = 0;
    (*pkt)->hdr.checksum = ping_calc_icmp_checksum((uint16_t*)*pkt, len - hlen);
    if ((*pkt)->hdr.checksum != chksum) {
        return false;
    }

    return true;
}

static ssize_t ping_send(ping *p)
{
    ping_pkt *pkt = &p->pkt;
    ssize_t bytes = 0;

    seq_clr(p->num_sent, p->seq_map, ARRAY_SIZE(p->seq_map));
    ping_create_package(p);

    bytes = sendto(p->fd, pkt, sizeof(ping_pkt), 0,
                   (struct sockaddr*)&p->dest->addr,
                   sizeof(struct sockaddr_in));
    if ( bytes < 0) {
        return -1;
    }

    p->num_sent++;

    return bytes;
}

static ssize_t ping_recv(ping *p)
{
    ssize_t bytes = 0;
    uint8_t recv_buff[IP_HDRLEN_MAX + sizeof(ping_pkt)];
    socklen_t fromlen = sizeof(struct sockaddr_in);
    struct sockaddr_in from;
    ping_pkt *pkt;
    uint16_t seq;
    bool dupflag = false;
    struct ip *ip = NULL;

    bytes = recvfrom(p->fd, recv_buff, ARRAY_SIZE(recv_buff), 0,
                     (struct sockaddr*)&from, &fromlen);
    if (bytes <= 0) {
        /* In case bytes == 0 peer closed connection, which should not happen */
        return -1;
    }

    if (!ping_validate_icmp_pkg(p, recv_buff, bytes, &pkt)) {
        goto exit_badmsg;
    }

    /* Validate the type of message */
    if (pkt->hdr.type != ICMP_ECHOREPLY) {
        goto exit_badmsg;
    }

    /* Validate identity (only raw mode) */
    if (!p->is_dgram && (ntohs(pkt->hdr.un.echo.id) != p->id)) {
        goto exit_badmsg;
    }

    /* Validate sequence number */
    seq = ntohs(pkt->hdr.un.echo.sequence);
    if (seq_check(seq, p->seq_map, ARRAY_SIZE(p->seq_map))) {
        p->num_dup++;
        dupflag = true;
    }
    else {
        seq_set(seq, p->seq_map, ARRAY_SIZE(p->seq_map));
        p->num_recv++;
    }

    if (p->is_dgram == false) {
        ip = (struct ip*)recv_buff;
    }
    ping_print_echo(dupflag, &from, ip, &p->stat, pkt, bytes);

    return bytes;

exit_badmsg:
    errno = EBADMSG;
    return -1;
}

volatile bool done = false;

static void ping_sigint_handler(int signal)
{
    done = true;
}

static int ping_run(ping * p, char* host)
{
    int ret = 0;
    ssize_t bytes;
    int wait;
    bool finishing;
    struct pollfd pfd;

    /* Reset statistics */
    memset (&p->stat, 0, sizeof (ping_stat));
    p->stat.tmin = 999999999.0;
    p->num_sent = 0;
    p->num_recv = 0;
    p->num_dup = 0;

    /* Reset the sequence number map */
    memset(p->seq_map, 0, PING_SEQMAP_SIZE);

    /* Get the new host */
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

    signal(SIGINT, ping_sigint_handler);

    finishing = false;
    wait = p->interval;
    while (!done) {
        uint16_t chksum;
        int pret;

        pret = poll(&pfd, 1, wait);
        if (pret < 0) {
            ret = -1;
            break;
        }

        if (pret > 0) {
            if (ping_recv(p) < 0) {
                ret = -1;
                break;
            }

            if (p->count && p->num_recv >= p->count) {
                break;
            }

        }
        else {
            if (p->count == 0 || p->num_sent < p->count) {
                if (ping_send(p) < 0) {
                    ret = -1;
                    break;
                }
            }
            else if (finishing) {
                break;
            }
            else {
                finishing = true;
                wait = PING_MAX_WAIT;
            }
        }
    }

exit_clean:
    ping_print_stat(p);

    free(p->dest->name);
    free(p->dest);

    return ret;
}

int main(int argc, char** argv)
{
    int c;
    bool verbose = false;
    double interval = PING_DEFAULT_INTERVAL;
    size_t count = 0;
    ping *p;
    char *endptr;

    while ((c = getopt(argc, argv, "vi:c:?")) != -1) {
        switch (c) {
        case 'v':
            verbose = true;
            break;

        case 'i':
            interval = strtod(optarg, &endptr);
            if (*endptr != '\0') {
                fprintf(stderr, "invalid value (`%s' near `%s')\n", optarg, endptr);
                exit (EXIT_FAILURE);
            }
            if (interval < PING_MIN_INTERVAL) {
                fprintf (stderr, "option value too small: %s\n", optarg);
                exit (EXIT_FAILURE);
            }
            interval *= PING_MS_PER_SEC;
            break;

        case 'c':
            count = strtoul(optarg, &endptr, 0);
            if (*endptr != '\0') {
                fprintf(stderr, "invalid value (`%s' near `%s')\n", optarg, endptr);
                exit (EXIT_FAILURE);
            }
            if (count == 0) {
                fprintf (stderr, "option value too small: %s\n", optarg);
                exit (EXIT_FAILURE);
            }
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
            if (errno != EINTR) {
                perror("ping_echo");
            }
        }
    }

    close(p->fd);
    free(p);
    return EXIT_SUCCESS;
}
