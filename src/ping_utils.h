#ifndef PING_UTILS_H
#define PING_UTILS_H

#include <netinet/in.h>

typedef struct host_s {
    struct sockaddr_in addr;
    char *name;
} host;

host  * ping_get_host(char *hostname);
unsigned char * ping_generate_data(unsigned char * pat, unsigned char *data, size_t len);

#endif
