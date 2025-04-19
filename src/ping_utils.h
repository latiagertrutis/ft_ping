#ifndef PING_UTILS_H
#define PING_UTILS_H

#include <netinet/in.h>

typedef struct host_s {
    struct sockaddr_in addr;
    char *name;
} host;

host   *ping_get_host(char *hostname);
unsigned char *ping_generate_data(unsigned char * pat, unsigned char *data, size_t len);
uint16_t ping_calc_icmp_checksum(uint16_t *pkt, size_t len);
double timespec_to_ms(struct timespec ts);
struct timespec ms_to_timespec(int ms);
struct timespec timespec_substract(struct timespec last, struct timespec now);
struct timespec timespec_add(struct timespec last, struct timespec now);
struct timespec timespec_normalise(struct timespec ts);
double nabs (double a);
double nsqrt (double a, double prec);
bool seq_check(uint16_t seq, uint8_t *seq_map, size_t len);
void seq_set(uint16_t seq, uint8_t *seq_map, size_t len);
void seq_clr(uint16_t seq, uint8_t *seq_map, size_t len);
#endif
