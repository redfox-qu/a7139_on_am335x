#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "common.h"
#include "rf433lib.h"

#define USAGE           "-o <rw|wr|r|w|c|dump> [-n netid] [-w wfreq] [-r rate]"
#define DEV             "/dev/a7139-1"

uint16_t se433_netid    = 0;
uint8_t  se433_wfreq    = 0;
uint8_t  se433_rate     = 0;

int main(int argc, char *argv[])
{
    int fd;
    int ret, opt, i;
    char buf[64];
    char *op = NULL;
    struct timespec tdely;
    unsigned long r_cnt = 0;
    unsigned long w_cnt = 0;

    if (argc < 2) {
        fprintf(stderr, "%s: %s\n", argv[0], USAGE);
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt(argc, argv, "o:n:w:r:")) != -1) {
        switch (opt) {
            case 'o':
                op = optarg;
                break;

            case 'n':
                se433_netid = atoi(optarg);
                break;

            case 'w':
                se433_wfreq = atoi(optarg);
                break;

            case 'r':
                se433_rate = atoi(optarg);
                break;

            default: /* '?' */
                fprintf(stderr, "%s: %s\n", argv[0], USAGE);
                exit(EXIT_FAILURE);
        }
    }

    fd = open(DEV, O_RDWR);
    if (fd < 0) {
        printf("open %s error: %s\n", DEV, strerror(errno));
        exit(-1);
    }

    if (se433_netid) {
        printf("set %s netid 0x%x\n", DEV, se433_netid);
        ret = rf433_set_netid(fd, se433_netid);
        if (ret < 0) {
            fprintf(stderr, "rf433_set_netid error: %d, %s\n", ret, strerror(errno));
        }

        se433_netid = 0;
        ret = rf433_get_netid(fd, &se433_netid);
        if (ret < 0) {
            fprintf(stderr, "rf433_get_netid error: %d, %s\n", ret, strerror(errno));
        }
        printf("get %s netid is 0x%x\n", DEV, se433_netid);
    }

    if (se433_wfreq) {
        printf("set %s wfreq %d, %s\n", DEV, se433_wfreq, rf433_get_freq_str(se433_wfreq));
        rf433_set_wfreq(fd, se433_wfreq);
        if (ret < 0) {
            fprintf(stderr, "se433_rate error: %d\n", ret);
        }

        se433_wfreq = 0;
        ret = rf433_get_wfreq(fd, &se433_wfreq);
        if (ret < 0) {
            fprintf(stderr, "rf433_get_wfreq error: %d\n", ret);
        }
        printf("get %s wfreq %d, %s\n", DEV, se433_wfreq, rf433_get_freq_str(se433_wfreq));
    }

    if (se433_rate) {
        printf("set %s rate %d, %s\n", DEV, se433_rate, rf433_get_rate_str(se433_rate));
        rf433_set_rate(fd, se433_rate);
        if (ret < 0) {
            fprintf(stderr, "rf433_set_rate error: %d\n", ret);
        }

        se433_rate = 0;
        ret = rf433_get_rate(fd, &se433_rate);
        if (ret < 0) {
            fprintf(stderr, "rf433_get_rate error: %d\n", ret);
        }
        printf("get %s rate %d, %s\n", DEV, se433_rate, rf433_get_rate_str(se433_rate));
    }

    if (op == NULL) {
        fprintf(stderr, "-o operation must be set!\n");
        exit(-1);
    }


    if (strcmp(op, "rw") == 0) {

        while (1) {
            ret = read(fd, buf, 64);
            if (ret <= 0) {
                printf("read error %s\n", strerror(errno));
                exit(-1);
            }
            r_cnt++;
#if 0
            for (i = 0; i < ret; i++) {
                printf("%02x ", buf[i]);
                if ((i+1) % 16 == 0) {
                    printf("\n");
                }
            }
#endif
            ret = write(fd, buf, 64);
            if (ret <= 0) {
                printf("write error %s\n", strerror(errno));
                exit(-1);
            }
            w_cnt++;

            printf("r_cnt=%lu, w_cnt=%lu\n", r_cnt, w_cnt);
        }
    }

    else if (strcmp(op, "wr") == 0){
        while (1) {
            ret = write(fd, buf, 64);
            if (ret <= 0) {
                printf("read error %s\n", strerror(errno));
                exit(-1);
            }
            w_cnt++;

            ret = read(fd, buf, 64);
            if (ret <= 0) {
                printf("write error %s\n", strerror(errno));
                exit(-1);
            }
            r_cnt++;

            printf("w_cnt=%lu, r_cnt=%lu\n", w_cnt, r_cnt);
        }
    }

    else if (strcmp(op, "r") == 0) {
        while (1) {
            ret = read(fd, buf, 64);
            if (ret <= 0) {
                printf("write error %s\n", strerror(errno));
                exit(-1);
            }
            r_cnt++;

            printf("r_cnt=%lu\n", r_cnt);

            for (i = 0; i < ret; i++) {
                printf("%02x ", buf[i]);
                if ((i+1) % 16 == 0) {
                    printf("\n");
                }
            }
        }
    }

    else if (strcmp(op, "w") == 0) {

        tdely.tv_sec = 0;
        tdely.tv_nsec = 100 * 1000 * 1000;  // delay 100ms;

        while (1) {
            memset(buf, (char)(w_cnt%256), sizeof(buf));
            for (i = 0; i < 64; i++) {
                printf("%02x ", buf[i]);
                if ((i+1) % 16 == 0) {
                    printf("\n");
                }
            }

            ret = write(fd, buf, 64);
            if (ret <= 0) {
                printf("read error %s\n", strerror(errno));
                exit(-1);
            }
            w_cnt++;

            nanosleep(&tdely, NULL);

            printf("w_cnt=%lu\n", w_cnt);
        }
    }

    else if (strcmp(op, "dump") == 0) {
        se433_netid = 0;
        ret = rf433_get_netid(fd, &se433_netid);
        if (ret < 0) {
            fprintf(stderr, "rf433_get_netid error: %d, %s\n", ret, strerror(errno));
        }
        printf("get %s netid is 0x%x\n", DEV, se433_netid);

        se433_wfreq = 0;
        ret = rf433_get_wfreq(fd, &se433_wfreq);
        if (ret < 0) {
            fprintf(stderr, "rf433_get_wfreq error: %d, %s\n", ret, strerror(errno));
        }
        printf("get %s wfreq is %d\n", DEV, se433_wfreq);

        se433_rate = 0;
        ret = rf433_get_rate(fd, &se433_rate);
        if (ret < 0) {
            fprintf(stderr, "rf433_get_rate error: %d, %s\n", ret, strerror(errno));
        }
        printf("get %s rate is %d\n", DEV, se433_rate);
    }


    close(fd);
    exit(0);
}

