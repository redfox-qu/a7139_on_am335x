//*******************************************************************************
// Copyright (C), RHTECH. Co., Ltd.
// Author:          redfox.qu@qq.com
// Date:            2013/12/26
// Version:         1.0.0
// Description:     The 433Mhz module command line interface source file
//
// History:
//      <author>            <time>      <version>   <desc>
//      redfox.qu@qq.com    2013/12/26  1.0.0       create this file
//
// TODO:
//      2013/12/31          add the common and debug
//*******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include "common.h"
#include "rfcli.h"

static int server_msg_qid;
static int client_msg_qid;

/*****************************************************************************
* Function Name  : alrm_handler
* Description    : time out alarm and exit function
* Input          : int
* Output         : None
* Return         : None
*                  - 0:ok
*****************************************************************************/
void alrm_handler(int s)
{
    printf("###Error recv msg time out(%d)!\n", RECV_MSG_TIME_OUT);
    exit(10);
}

/*****************************************************************************
* Function Name  : show_rf433
* Description    : show the rf433 configure
* Input          : rf433_cfg*
* Output         : display
* Return         : None
*****************************************************************************/
void show_rf433(rf433_cfg *cfg)
{
    printf("%s=%d\n", RF433_NVR_NET_ID, RF433_NET_ID(cfg->net_id));
    printf("%s=0x%08x\n", RF433_NVR_LOCAL_ADDR, cfg->local_addr);
}

/*****************************************************************************
* Function Name  : show_socket
* Description    : show the socket configure
* Input          : socket_cfg*
* Output         : display
* Return         : None
*****************************************************************************/
void show_socket(socket_cfg *cfg)
{
    printf("%s=%s\n", RF433_NVR_SRV_IP, inet_ntoa(cfg->server_ip));
    printf("%s=%d\n", RF433_NVR_UDP_PORT, cfg->server_port);
}

/*****************************************************************************
* Function Name  : show_se433_list
* Description    : show the se433 list and data
* Input          : uint32_t*
* Output         : display
* Return         : None
*****************************************************************************/
void show_se433_list(se433_data *se433data)
{
    printf("addr=0x%08x, req_cnt=%d, rsp_cnt=%d, type=0x%02x, " \
            "data=%.3f, vol=0x%02x, batt=0x%02x, flag=0x%02x, whid=0x%02x\n",
            se433data->addr,
            se433data->req_cnt,
            se433data->rsp_cnt,
            se433data->data.type,
            se433data->data.data,
            se433data->data.vol,
            se433data->data.batt,
            se433data->data.flag,
            se433data->data.watchid);
}

/*****************************************************************************
* Function Name  : show_resp
* Description    : show the other message response
* Input          : char*
* Output         : display
* Return         : None
*****************************************************************************/
int show_resp(char *resp)
{
    /* all success response will no message,
     * and all error response will say some word like 'XXX error'.
     * if the verbose message not in the response
     * please see the syslog
     */
    int resp_len = strlen(resp);

    if (resp_len != 0) {
        printf("####Error %s\n", resp);
        return resp_len;
    }

    return 0;
}

/*****************************************************************************
* Function Name  : send_msg
* Description    : send the message to rfrepeater deamon
* Input          : char*
* Output         : display
* Return         : None
*****************************************************************************/
int send_msg(int type, void *p)
{
    int ret;
    struct msg_st msg;

    msg.mkey = getpid();
    msg.mtype = type;
    switch (type) {
        case MSG_REQ_SET_RF433:
            memcpy(&msg.u.rf433, p, sizeof(rf433_cfg));
            break;

        case MSG_REQ_SET_SOCK:
            memcpy(&msg.u.socket, p, sizeof(socket_cfg));
            break;

        case MSG_REQ_SET_SE433:
            memcpy(&msg.u.se433, p, sizeof(se433_cfg));
            break;

        case MSG_REQ_READ_CFG:
        case MSG_REQ_SAVE_CFG:
        case MSG_REQ_GET_CFG:
        case MSG_REQ_GET_SE433L:
            msg.u.opt = type;
            break;

        case MSG_REQ_SET_LOG:
            msg.u.msg_level = *(int*)p;
            break;

        default:
            printf("\nsend_msg(): msg type %d error!\n\n", type);
            return -1;
            break;
    }

    ret = msgsnd(server_msg_qid, &msg, sizeof(msg) - sizeof(long), 0);
    if (ret == -1) {
        printf("send_msg(): msgsnd() err: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

/*****************************************************************************
* Function Name  : recv_msg
* Description    : recveive the message from rfrepeater deamon
* Input          : void
* Output         : display
* Return         : int
*                  - 0: ok
*****************************************************************************/
int recv_msg(void)
{
    int ret = 0;
    int msg_len;
    struct msg_st msg;

    if (signal(SIGALRM, alrm_handler) == SIG_ERR) {
        printf("signal(SIGALRM) error\n");
        goto out;
    }

    alarm(RECV_MSG_TIME_OUT);

    while (1) {
        msg_len = msgrcv(client_msg_qid, &msg, sizeof(msg)-sizeof(long), getpid(), 0);
        if (msg_len == -1) {
            printf("recv_msg(): msgrcv() err: %s\n", strerror(errno));
            goto out;
        }

        switch (msg.mtype) {
            case MSG_RSP_RF433:
                show_rf433(&msg.u.rf433);
                break;

            case MSG_RSP_SOCKET:
                show_socket(&msg.u.socket);
                break;

            case MSG_RSP_SE433L:
                show_se433_list(&msg.u.se433data);
                break;

            case MSG_RSP_END:
                show_resp(msg.u.resp);
                goto out;
                break;

            default:
                goto out;
        }
    }

out:
    alarm(0);
    return ret;
}


#define USAGE   " \
Usage: rfcli [options] \n \
    -h, --help                  Show this USAGE \n \
    -v, --version               Show version \n \
    -s, --save                  Save config to file \n \
    -r, --read                  Read config from file \n \
    -c, --show                  Show the current config \n \
    -l, --selist                Show se433 sensor list \n \
    -L, --loglevel              choose loglevel \n \
                                1:LOG_EMERG,    2:LOG_ALERT \n \
                                3:LOG_CRIT,     4:LOG_ERR \n \
                                5:LOG_WARING,   6:LOG_NOTICE \n \
                                7:LOG_INFO,     8:LOG_DEBUG \n \
config options: \n \
    -N, --id=netid              433 network id \n \
    -A, --addr=433addr          433 receive address \n \
    -I, --rip=ipaddr            set remote server ip address \n \
    -P, --rport=port            set remote server udp port \n \
\n "

/*****************************************************************************
* Function Name  : main
* Description    : rfcli main function
* Input          : int, char**
* Output         : None
* Return         : int
*                  - 0: ok
*                  - -1: error
*****************************************************************************/
int main(int argc, char **argv)
{
    int opt, log_level, opt_id = 0;
    int ret = 0;
    int msg_retry;
    socket_cfg sock_set;
    rf433_cfg rf433_set;
    se433_cfg se433_set;

    struct option longopts[] = {
        { "help",           0, NULL, 'h' }, //1
        { "version",        0, NULL, 'v' }, //2
        { "save",           0, NULL, 's' }, //3
        { "read",           0, NULL, 'r' }, //4
        { "show",           0, NULL, 'c' }, //5
        { "selist",         0, NULL, 'l' }, //6
        { "loglevel",       1, NULL, 'L' }, //7
        { "id",             1, NULL, 'N' }, //8
        { "addr",           1, NULL, 'A' }, //9
        { "rip",            1, NULL, 'I' }, //10
        { "rport",          1, NULL, 'P' }, //11
        { 0, 0, 0, 0 },
    };

    rf433_set.net_id        = RF433_CFG_NET_ID;
    rf433_set.local_addr    = RF433_CFG_LOCAL_ADDR;
    rf433_set.m_mask        = 0;

    sock_set.server_ip.s_addr = inet_addr(RF433_CFG_SRV_IP);
    sock_set.server_port    = RF433_CFG_UDP_PORT;
    sock_set.m_mask         = 0;

    se433_set.se433_addr    = 0;
    se433_set.op            = 0;

    if (argc == 1) {
        printf(USAGE);
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt_long(argc, argv, "hvsrclL:N:A:I:P:", longopts, NULL)) != -1) {
        switch (opt) {
            case 's':                   // save
                opt_id |= MSG_REQ_SAVE_CFG;
                break;

            case 'r':                   // read
                opt_id |= MSG_REQ_READ_CFG;
                break;

            case 'c':                   // show
                opt_id |= MSG_REQ_GET_CFG;
                break;

            case 'l':                   // se433 list
                opt_id |= MSG_REQ_GET_SE433L;
                break;

            case 'L':                   // loglevel
                if ((get_log_level(optarg, &log_level)) == -1) {
                    fprintf(stderr, "\n invalid value (%s)\n\n", optarg);
                    ret = -1;
                    goto err;
                }
                opt_id |= MSG_REQ_SET_LOG;
                break;

            case 'N':                   // netid
                if ((get_netid(optarg, &rf433_set.net_id)) == -1) {
                    fprintf(stderr, "\n invalid value (%s)\n\n", optarg);
                    ret = -1;
                    goto err;
                }
                rf433_set.m_mask |= RF433_MASK_NET_ID;
                break;

            case 'A':                   // addr
                if ((get_local_addr(optarg, &rf433_set.local_addr)) == -1) {
                    fprintf(stderr, "\n invalid value (%s)\n\n", optarg);
                    ret = -1;
                    goto err;
                }
                rf433_set.m_mask |= RF433_MASK_RCV_ADDR;
                break;
#if 0
            case 'E':                   // seadd
                if ((get_se433_addr(optarg, &se433_set.se433_addr)) == -1) {
                    fprintf(stderr, "\n invalid value (%s)\n\n", optarg);
                    ret = -1;
                    goto err;
                }
                se433_set.op = SE433_OP_ADD;
                break;

            case 'D':                   // sedel
                if ((get_se433_addr(optarg, &se433_set.se433_addr)) == -1) {
                    fprintf(stderr, "\n invalid value (%s)\n\n", optarg);
                    ret = -1;
                    goto err;
                }
                se433_set.op = SE433_OP_DEL;
                break;
#endif
            case 'I':                   // rip
                if ((get_ip(optarg, &sock_set.server_ip)) == -1) {
                    fprintf(stderr, "\n invalid value (%s)\n\n", optarg);
                    ret = -1;
                    goto err;
                }
                sock_set.m_mask |= SOCK_MASK_SER_IP;
                break;

            case 'P':                   // rport
                if ((get_port(optarg, &sock_set.server_port)) == -1) {
                    fprintf(stderr, "\n invalid value (%s)\n\n", optarg);
                    ret = -1;
                    goto err;
                }
                sock_set.m_mask |= SOCK_MASK_UDP_PORT;
                break;

            case 'h':
            case 'v':
                printf(USAGE);
                exit(EXIT_SUCCESS);
                break;

            default:
                fprintf(stderr, "%s: invalid opt (%c)\n\n", argv[0], opt);
                printf(USAGE);
                exit(EXIT_FAILURE);
                break;
        }
    }


    // create msg quere
    for (msg_retry = 0; msg_retry < MSG_RETRY_MAX; msg_retry++) {
        server_msg_qid = msgget((key_t)SERVER_MSG, 0666);
        client_msg_qid = msgget((key_t)CLIENT_MSG, 0666);

        if (server_msg_qid != -1 && client_msg_qid == -1) {
            break;
        }
        usleep(MSG_RETRY_UDELAY);
    }

    if (server_msg_qid == -1) {
        fprintf(stderr, "msgget(server_msg_qid):%s\n", strerror(errno));
        ret = -3;
        goto err;
    }
    if (client_msg_qid == -1) {
        fprintf(stderr, "msgget(client_msg_qid):%s\n", strerror(errno));
        ret = -3;
        goto err;
    }


    if (rf433_set.m_mask) {
        ret = send_msg(MSG_REQ_SET_RF433, &rf433_set);
        if (ret != 0)
            goto err;
        ret = recv_msg();
        if (ret != 0)
            goto err;
    }

    if (sock_set.m_mask) {
        ret = send_msg(MSG_REQ_SET_SOCK, &sock_set);
        if (ret != 0)
            goto err;
        ret = recv_msg();
        if (ret != 0)
            goto err;
    }

    if (se433_set.op) {
        ret = send_msg(MSG_REQ_SET_SE433, &se433_set);
        if (ret != 0)
            goto err;
        ret = recv_msg();
        if (ret != 0)
            goto err;
    }

    if (opt_id & MSG_REQ_READ_CFG) {
        ret = send_msg(MSG_REQ_READ_CFG, NULL);
        if (ret != 0)
            goto err;
        ret = recv_msg();
        if (ret != 0)
            goto err;
    }

    if (opt_id & MSG_REQ_SAVE_CFG) {
        ret = send_msg(MSG_REQ_SAVE_CFG, NULL);
        if (ret != 0)
            goto err;
        ret = recv_msg();
        if (ret != 0)
            goto err;
    }

    if (opt_id & MSG_REQ_GET_CFG) {
        ret = send_msg(MSG_REQ_GET_CFG, NULL);
        if (ret != 0)
            goto err;
        ret = recv_msg();
        if (ret != 0)
            goto err;
    }

    if (opt_id & MSG_REQ_GET_SE433L) {
        ret = send_msg(MSG_REQ_GET_SE433L, NULL);
        if (ret != 0)
            goto err;
        ret = recv_msg();
        if (ret != 0)
            goto err;
    }

    if (opt_id & MSG_REQ_SET_LOG) {
        ret = send_msg(MSG_REQ_SET_LOG, &log_level);
        if (ret != 0)
            goto err;
        ret = recv_msg();
        if (ret != 0)
            goto err;
    }

err:
    exit(ret);
}


