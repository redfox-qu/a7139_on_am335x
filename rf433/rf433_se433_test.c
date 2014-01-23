//*******************************************************************************
// Copyright (C), RHTECH. Co., Ltd.
// Author:          redfox.qu@qq.com
// Date:            2013/01/04
// Version:         1.0.0
// Description:     rf433 se433 sensor simulator
//
// History:
//      <author>            <time>      <version>   <desc>
//      redfox.qu@qq.com    2014/01/04  1.0.0       create this file
//      redfox.qu@qq.com    2014/01/06  1.0.1       add rswp433 protocol process
//
// TODO:
//*******************************************************************************

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>
#include "rf433lib.h"
#include "common.h"

#define SE433_ADDR      (SE433_ADDR_MIN+1)
#define RF433_ADDR      (RF433_RCVADDR_MAX)
#define RF433_RATE      A7139_RATE_10K

#define USAGE           "-d <rf433-dev> [-a se433-addr] [-n rf433-netid] [-i kj98-rf433-addr] [-r rf433-rate]"

#define DEBUG
//#undef  DEBUG
#ifdef DEBUG
#define debugf(fmt, args...)        fprintf(stderr, fmt, ##args)
#else
#define debugf(fmt, args...)
#endif

char *se433_devname     = "/dev/rf433-1";
uint32_t se433_addr     = SE433_ADDR;
uint32_t rf433_addr     = RF433_ADDR;
uint16_t se433_netid    = RF433_NETID(RF433_NETID_MIN);
uint8_t  se433_rate     = RF433_RATE;
uint8_t  se433_state    = SE433_STATE_RESET;

struct se_list {
    uint32_t se_addr;
    uint8_t se_type;
    float se_data;
};

struct se_list se433_array[] = {
    {
        .se_addr = 0x55555501,
        .se_type = SE433_TYPE_CH4,
        .se_data = 0.1,
    },
    {
        .se_addr = 0x55555502,
        .se_type = SE433_TYPE_CO,
        .se_data = 10,
    },
    {
        .se_addr = 0x55555503,
        .se_type = SE433_TYPE_CH4,
        .se_data = 0.2,
    },
    {
        .se_addr = 0x55555504,
        .se_type = SE433_TYPE_TEMP,
        .se_data = 10,
    },
    {
        .se_addr = 0x55555505,
        .se_type = SE433_TYPE_TEMP,
        .se_data = 14,
    },
};

void se433_buf_dump(char *buf, int len)
{
    int i;

    for (i = 0; i < len; i++)
    {
        if (i % 16 == 0)
            fprintf(stderr, "\n");
        fprintf(stderr, "%02x ", buf[i]);
    }

    fprintf(stderr, "\n\n");
}

int se433_reset(int rf433_fd)
{
    struct timeval tv;
    int ret;

    debugf("enter se433_reset\n");

#ifndef DEBUG
    /* RESET DELAY ALGORITHM */
    srandom(se433_addr);
    tv.tv_sec = 5 + (random() % 50);
    tv.tv_usec = 0;
#else
    tv.tv_sec = 5;
    tv.tv_usec = 0;
#endif

    debugf("[reset] ALGORITHM delay %d(s)\n", (int)tv.tv_sec);

    ret = select(rf433_fd + 1, NULL, NULL, NULL, &tv);

    /* error or break out */
    if (ret == 0) {
        se433_state = SE433_STATE_REGISTER;
        return 0;
    }

    debugf("leave se433_reset\n");

    /* may be error */
    return ret;
}

int se433_register(int rf433_fd)
{
    buffer *se433_buf;
    rswp433_pkg *pkg;
    fd_set rset, wset;
    struct timeval tv;
    unsigned long reg_cnt = 0;
    int ret;

    debugf("[register] enter se433_register\n");

    se433_buf = buf_new_max();
    if (se433_buf == NULL) {
        debugf("[register] buf_new_max error\n");
        return -1;
    }

    /* reset the rf433 register addr */
    rf433_addr = RF433_ADDR;

    do {
        /* first write a register request */
        FD_ZERO(&rset);
        FD_ZERO(&wset);
        FD_SET(rf433_fd, &wset);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        debugf("\n[register] write cnt %lu\n", reg_cnt);

        ret = select(rf433_fd + 1, NULL, &wset, NULL, &tv);

        /* error or time out */
        if (ret <= 0) {
            debugf("[register] write error or timeout\n");
            continue;
        }

        if (FD_ISSET(rf433_fd, &wset)) {

            /* send a register request */
            buf_clean(se433_buf);

            pkg = (rswp433_pkg*)buf_data(se433_buf);
            pkg->header.sp[0] = RSWP433_PKG_SP_0;
            pkg->header.sp[1] = RSWP433_PKG_SP_1;
            pkg->header.len = sizeof(rswp433_pkg_content);
            pkg->header.crc1 = crc8((char*)&pkg->header, sizeof(rswp433_pkg_header) - 1);

            pkg->u.content.dest_addr = rf433_addr;
            pkg->u.content.src_addr = se433_addr;
            pkg->u.content.cmd = RSWP433_CMD_REG_REQ;
            pkg->u.content.crc2 = crc8((char*)&pkg->u.content, sizeof(rswp433_pkg_content) - 1);

            buf_incrlen(se433_buf, sizeof(rswp433_pkg));

            debugf("[register] to netid=0x%04x dest=0x%08x, src=0x%08x, cmd=0x%02x\n",
                    se433_netid, rf433_addr, se433_addr, RSWP433_CMD_REG_REQ);

            buf_dump(se433_buf);

            buf_write(rf433_fd, se433_buf);

            reg_cnt++;
        }

        /* second read a register response */
        FD_ZERO(&rset);
        FD_ZERO(&wset);
        FD_SET(rf433_fd, &rset);

        /* REGISTER DELAY ALGORITHM */
#ifndef DEBUG
        srandom(time(NULL));
        tv.tv_sec = 7 * (reg_cnt < 30 ? reg_cnt : 30) + (random() % 50);
        tv.tv_usec = 0;
#else
        /* test */
        tv.tv_sec = 5;
        tv.tv_usec = 0;
#endif

        debugf("[register] ALGORITHM delay %d(s)\n", (int)tv.tv_sec);

        ret = select(rf433_fd + 1, &rset, NULL, NULL, &tv);

        /* error or time out */
        if (ret <= 0) {
            debugf("[register] read error or timeout\n");
            continue;
        }

        if (FD_ISSET(rf433_fd, &rset)) {

            /* receive a register response */
            buf_clean(se433_buf);

            ret = buf_read(rf433_fd, se433_buf);

            if (ret <= 0) {
                debugf("[register] read error return %d\n", ret);
                continue;
            }

            debugf("[register] read %d bytes from 433\n", ret);
            /* se433_buf_dump(se433_buf); */

            pkg = rswp433_pkg_new();
            if (pkg == NULL) {
                debugf("[register] rswp433_pkg_new error\n");
                goto out;
            }

            if (rswp433_pkg_analysis(se433_buf, pkg) == 1) {

                if (pkg->u.content.cmd != RSWP433_CMD_REG_RSP) {
                    debugf("[register] invalid cmd 0x%x on state %d\n", pkg->u.content.cmd, se433_state);
                    rswp433_pkg_del(pkg);
                    continue;
                }

                /* ok, my register request is passed,
                 * so change to poll state
                 */
                rf433_addr = pkg->u.content.src_addr;
                se433_state = SE433_STATE_POLL;

                debugf("[register] got a valid pkg, cmd=0x%x\n", pkg->u.content.cmd);
                debugf("[register] se433_state=%d\n", se433_state);
                debugf("[register] rf433_addr=0x%x\n", rf433_addr);
            }
            else {
                debugf("[register] got a invalid pkg, continue register. \n");
                buf_dump(se433_buf);
                buf_clean(se433_buf);
            }

            rswp433_pkg_del(pkg);
        }
    } while (se433_state == SE433_STATE_REGISTER);

out:
    buf_free(se433_buf);

    debugf("[register] leave se433_register\n");

    return ret;
}

int se433_poll(int rf433_fd)
{
    buffer *se433_buf;
    rswp433_pkg *rpkg, *wpkg;
    fd_set rset, wset;
    struct timeval tv;
    uint8_t se_type = SE433_TYPE_CH4;
    uint8_t se_vol = 0x33;
    uint8_t se_batt = 0x64;
    uint8_t se_flag = 0x00;
    uint8_t se_whid = 0x00;
    float se_data = 0.1;
    unsigned long poll_cnt = 0;
    int ret;

    debugf("[poll] enter se433_poll\n");

    se433_buf = buf_new_max();
    if (se433_buf == NULL) {
        debugf("[poll] buf_new_max error\n");
        return -1;
    }

    rpkg = rswp433_pkg_new();
    if (rpkg == NULL) {
        debugf("[poll] rswp433_pkg_new error\n");
        return -1;
    }

    do {
        /* first read a poll request */
        FD_ZERO(&rset);
        FD_ZERO(&wset);
        FD_SET(rf433_fd, &rset);

        /* POLL OFFLINE DELAY ALGORITHM */
        tv.tv_sec = RF433_SE433_MAX * RSWP433_OFFLINE_CNT;
        tv.tv_usec = 0;

        debugf("[poll] ALGORITHM delay %d(s)\n", (int)tv.tv_sec);

        ret = select(rf433_fd + 1, &rset, NULL, NULL, &tv);

        /* error */
        if (ret < 0) {
            debugf("[poll] read error\n");
            continue;
        }

        /* time out */
        else if (ret == 0) {
            se433_state = SE433_STATE_RESET;
            debugf("[poll] time out se433_state=%d\n", se433_state);
            goto out;
        }

        if (FD_ISSET(rf433_fd, &rset)) {

            /* receive a poll request */
            buf_clean(se433_buf);

            ret = buf_read(rf433_fd, se433_buf);

            if (ret <= 0) {
                debugf("[poll] read return error %d\n", ret);
                continue;
            }

            debugf("[poll] read %d bytes from 433\n", ret);
            /* se433_buf_dump(se433_buf); */


            if (rswp433_pkg_analysis(se433_buf, rpkg) == 1) {
                if (rpkg->u.content.cmd != RSWP433_CMD_DATA_REQ) {
                    debugf("[poll] invalid cmd 0x%x on state %d\n", rpkg->u.content.cmd, se433_state);
                    rswp433_pkg_del(rpkg);
                    continue;
                }

                if (rpkg->u.content.src_addr != rf433_addr) {
                    debugf("[poll] invalid host kj98-f addr 0x%x on state %d\n",
                            rpkg->u.content.src_addr, se433_state);
                    rswp433_pkg_del(rpkg);
                    continue;
                }

                /* ok, got a data poll request,
                 * so prepare data to response
                 */
                buf_clean(se433_buf);

                debugf("[poll] got a valid rswp433 pkg, cmd=0x%x, poll_cnt=%lu\n",
                        rpkg->u.content.cmd, poll_cnt++);

            } else {
                debugf("[poll] invalid rswp433_pkg on state %d\n", se433_state);
                buf_dump(se433_buf);
                buf_clean(se433_buf);
                continue;
            }
        }

        /* next write a data response */
        FD_ZERO(&rset);
        FD_ZERO(&wset);
        FD_SET(rf433_fd, &wset);

        /* POLL OFFLINE DELAY ALGORITHM */
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        debugf("[poll] write delay %d(s)\n", (int)tv.tv_sec);

        ret = select(rf433_fd + 1, NULL, &wset, NULL, &tv);

        /* error */
        if (ret <= 0) {
            debugf("[poll] write error or timeout\n");
            continue;
        }

        if (FD_ISSET(rf433_fd, &wset)) {

            /* send a register request */
            buf_clean(se433_buf);
            srandom(time(NULL));

            wpkg = (rswp433_pkg*)buf_data(se433_buf);
            wpkg->header.sp[0] = RSWP433_PKG_SP_0;
            wpkg->header.sp[1] = RSWP433_PKG_SP_1;
            wpkg->header.len = sizeof(rswp433_pkg_data_content);
            wpkg->header.crc1 = crc8((char*)&wpkg->header, sizeof(rswp433_pkg_header) - 1);

            wpkg->u.data_content.dest_addr = rf433_addr;
            wpkg->u.data_content.src_addr = se433_addr;
            wpkg->u.data_content.cmd = RSWP433_CMD_DATA_RSP;
            wpkg->u.data_content.data.type = se_type;
            wpkg->u.data_content.data.data = se_data += (((float)(random() % 5) - 2) / 100);
            wpkg->u.data_content.data.vol = se_vol;
            wpkg->u.data_content.data.batt = se_batt;
            wpkg->u.data_content.data.flag = se_flag;
            wpkg->u.data_content.data.watchid = se_whid;
            wpkg->u.data_content.crc2 = crc8((char*)&wpkg->u.data_content, sizeof(rswp433_pkg_data_content) - 1);

            buf_incrlen(se433_buf, sizeof(rswp433_pkg));

            debugf("[poll] response dest_addr=0x%08x, src_addr=0x%08x, data=%.2f, crc1=0x%02x, crc2=0x%02x\n",
                    wpkg->u.content.dest_addr,
                    wpkg->u.content.src_addr,
                    wpkg->u.data_content.data.data,
                    wpkg->header.crc1,
                    wpkg->u.content.crc2);

            buf_write(rf433_fd, se433_buf);
        }
    } while (se433_state == SE433_STATE_POLL);

out:
    buf_free(se433_buf);
    rswp433_pkg_del(rpkg);

    return ret;
}


int main(int argc, char *argv[])
{
    int rf433_fd, opt;

    if (argc < 2) {
        fprintf(stderr, "%s: %s\n", argv[0], USAGE);
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt(argc, argv, "d:a:n:i:r:")) != -1) {
        switch (opt) {
            case 'd':
                se433_devname = optarg;
                break;

            case 'a':
                se433_addr = atoi(optarg);
                break;

            case 'n':
                se433_netid = atoi(optarg);
                break;

            case 'i':
                rf433_addr = atoi(optarg);
                break;

            case 'r':
                se433_rate = atoi(optarg);
                break;

            default: /* '?' */
                fprintf(stderr, "%s: %s\n", argv[0], USAGE);
                exit(EXIT_FAILURE);
        }
    }
#if 0
    if (optind >= argc) {
        fprintf(stderr, "Expected argument after options\n");
        exit(EXIT_FAILURE);
    }
#endif

    set_loglevel(SYS_DEBUG);

    rf433_fd = open(se433_devname, O_RDWR);
    if (rf433_fd == -1) {
        fprintf(stderr, "open %s error, %s\n", se433_devname, strerror(errno));
        exit(EXIT_FAILURE);
    }

    debugf("%s opend\n", se433_devname);

#if 1
    if (set_rf433_opt(rf433_fd, se433_netid, se433_rate) < 0) {
        fprintf(stderr, "set rf433 config error\n");
        exit(EXIT_FAILURE);
    }
    debugf("set netid=0x%x, rate=%d\n", se433_netid, se433_rate);
#endif

    /* main loop */
    while (1) {

        se433_reset(rf433_fd);

        se433_register(rf433_fd);

        se433_poll(rf433_fd);

    }

    exit(0);
}


