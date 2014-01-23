//*******************************************************************************
// Copyright (C), RHTECH. Co., Ltd.
// Author:          redfox.qu@qq.com
// Date:            2013/12/25
// Version:         1.0.0
// Description:     The 433Mhz module common function file
//
// History:
//      <author>            <time>      <version>   <desc>
//      redfox.qu@qq.com    2013/12/25  1.0.0       create this file
//
// TODO:
//      2013/12/31          add the common and debug
//*******************************************************************************

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "list.h"
#include "rf433pkg.h"

#ifdef UNUSED
#elif defined(__GNUC__)
#define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
#define UNUSED(x) /*@unused@*/ x
#else
#define UNUSED(x) x
#endif

#define ARRAY_SIZE(x)           (sizeof(x)/sizeof(x[0]))

#define SERVER_MSG              1234
#define CLIENT_MSG              4321

#define MSG_REQ_SET_RF433       (1<<0)
#define MSG_REQ_SET_SOCK        (1<<1)
#define MSG_REQ_SET_SE433       (1<<2)
#define MSG_REQ_READ_CFG        (1<<3)
#define MSG_REQ_SAVE_CFG        (1<<4)
#define MSG_REQ_GET_CFG         (1<<5)
#define MSG_REQ_GET_SE433L      (1<<6)
#define MSG_REQ_SET_LOG         (1<<8)
#define MSG_RSP_END             (1<<16)
#define MSG_RSP_RF433           (1<<17)
#define MSG_RSP_SOCKET          (1<<18)
#define MSG_RSP_SE433L          (1<<19)

#define RF433_MASK_NET_ID       (1<<0)
#define RF433_MASK_RCV_ADDR     (1<<1)

#define SOCK_MASK_SER_IP        (1<<0)
#define SOCK_MASK_UDP_PORT      (1<<1)

#define SE433_OP_ADD            1
#define SE433_OP_DEL            2

#define MSG_RESP_LEN            256
#define SERVER_PORT_MIN         1024
#define SERVER_PORT_MAX         65500
#define RF433_NETID_BYTE_0      0xaa    /* 0x98 */
#define RF433_NETID_MIN         0x01
#define RF433_NETID_MAX         0xf0
#define RF433_NETID(id)         ((RF433_NETID_BYTE_0<<8)|(id&0xff))
#define RF433_RCVADDR_MIN       0x00000000
#define RF433_RCVADDR_MAX       0x00ffffff
#define SE433_ADDR_MIN          0x01000000
#define SE433_ADDR_MAX          0xfffffffe


enum msg_syslog_level {
    SYS_EMERG = 1,              /* 1 */
    SYS_ALERT,                  /* 2 */
    SYS_CRIT,                   /* 3 */
    SYS_ERR,                    /* 4 */ /* dufault level */
    SYS_WARNING,                /* 5 */
    SYS_NOTICE,                 /* 6 */
    SYS_INFO,                   /* 7 */
    SYS_DEBUG                   /* 8 */
};

#define TRACE(fmt, args...)     sys_printf(SYS_DEBUG, fmt, ##args)

#define NAMESIZE                32
struct collate_st {
    char str[NAMESIZE];
    int val;
};


#define RF433_THREAD_MAX        1
#define RF433_LOG_F             "rf433"
#define RF433_SE433_MAX         32

#define UDP_BUF_SIZE            256
#define RF433_BUF_SIZE          256
#define RELAY_POLL_SEC          5

#define RF433_CFG_SRV_IP        "192.168.1.37"
#define RF433_CFG_UDP_PORT      4300
#define RF433_CFG_LOCAL_ADDR    0x00abcdef
#define RF433_CFG_NET_ID        RF433_NETID(RF433_NETID_MIN)
#define RF433_CFG_RATE          A7139_RATE_10K
#define RF433_NET_ID(id)        ((uint8_t)(id & 0x00ff))
#define RF433_WFREQ(id)         ((uint8_t)(id & 0x0f))

#define RF433_NVR_SRV_IP        "rf433_server_ip"
#define RF433_NVR_UDP_PORT      "rf433_server_port"
#define RF433_NVR_NET_ID        "rf433_net_id"
#define RF433_NVR_LOCAL_ADDR    "rf433_local_addr"

#define RF433_SHOW_SE_ADDR      "rf433_se_addr"
#define RF433_SHOW_SE_DATA      "rf433_se_data"

#define RF433_RF_DEV_NAME       "/dev/a7139-1"


typedef struct {
    uint32_t m_mask;
    uint16_t local_port;
    struct in_addr server_ip;
    uint16_t server_port;
} socket_cfg;

typedef struct {
    uint32_t m_mask;
    uint32_t local_addr;
    uint16_t net_id;
    uint8_t rate;
    uint8_t freq;
} rf433_cfg;

typedef struct {
    uint8_t op;
    uint32_t se433_addr;
} se433_cfg;

typedef struct {
    uint32_t se433_addr;
    uint64_t req_cnt;           /* rf433 to udp */
    uint64_t rsp_cnt;           /* udp to rf433 */
} se433_cnt;

struct msg_st {
    long mkey;
    long mtype;
    union {
        uint32_t opt;
        uint32_t msg_level;
        socket_cfg socket;
        rf433_cfg rf433;
        se433_cfg se433;
        se433_data se433data;
        char resp[MSG_RESP_LEN];
    } u;
};

void set_loglevel(int n);
void sys_printf(int level, const char *format, ...);
int getvalue(char *opt, int *value, int base);
int get_ip(char* ip, struct in_addr *ip_addr);
int get_port(char *str, uint16_t *port);
int get_netid(char *str, uint16_t *netid);
int get_local_addr(char *str, uint32_t *addr);
int get_se433_addr(char *str, uint32_t *addr);
//int get_se433_list(char *str, se433_head *head);
int get_log_level(char *str, int *log_level);

ssize_t safe_write(int fd, const void *buf, size_t count);
ssize_t full_write(int fd, const void *buf, size_t len);

uint8_t crc8(char *data, uint8_t len);

#endif  //__COMMON_H__

