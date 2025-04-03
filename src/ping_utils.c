#include <netdb.h>
#include <stdlib.h>
#include <string.h>

#include "ping_utils.h"

struct gen_data_s {
    unsigned char *data;
    size_t len;
};

static struct gen_data_s data = {NULL, 0};

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

unsigned char * ping_generate_data(unsigned char * pat, size_t len) {
    size_t i = 0;

    if (len == 0) {
        return NULL;
    }

    if (data.data == NULL || len != data.len) {
        free(data.data);

        data.data = malloc(sizeof(unsigned char) * len);
        if (data.data == NULL) {
            return NULL;
        }
        data.len = len;
    } else {
        return data.data;
    }

    if (pat != NULL) {
        unsigned char * data_p;

        for (data_p = data.data; data_p < data.data + len; data_p++) {
            *data_p = pat[i];
            if (++i >= len) {
                i = 0;
            }
        }

    } else {
        for (i = 0; i < len; i++) {
            data.data[i] = i;
        }
    }

    return data.data;
}

void ping_clean_data() {
    free(data.data);
    data = (struct gen_data_s) {
        NULL, 0
    };
}
