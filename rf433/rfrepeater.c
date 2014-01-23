//*******************************************************************************
// Copyright (C), RHTECH. Co., Ltd.
// Author:          redfox.qu@qq.com
// Date:            2013/12/25
// Version:         1.0.0
// Description:     The 433Mhz module wireless to ethernet transmit on am335x
//
// History:
//      <author>            <time>      <version>   <desc>
//      redfox.qu@qq.com    2013/12/25  1.0.0       create this file
//
// TODO:
//      2013/12/31          add the common and debug
//*******************************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include "common.h"
#include "rfrepeater.h"
#include "rf433lib.h"
#include "nvram.h"
#include "list.h"

#define USAGE   " \
rfrepeater - Sub 1G 433Mhz wireless to ethernet transmitter \n \
\n \
Version: 1.0.0 \n \
Usage: rfrepeater [-h] [-v] [-d] \n \
    -h                          Show this USAGE \n \
    -v                          Show version \n \
    -d                          Running in Debug mode \n \
"


extern int loglevel;
extern int debug_mode;
extern int exitflag;

static int server_msg_qid;
static int client_msg_qid;
rf433_instence rf433i;
int spipefd[RF433_THREAD_MAX];

/*****************************************************************************
* Function Name  : default_init
* Description    : init rf433i instence default value
* Input          : void
* Output         : None
* Return         : int
*                  - 0:ok
*****************************************************************************/
int default_init(void)
{
    /* set default configure */
    rf433i.socket.local_port = RF433_CFG_UDP_PORT;
    rf433i.socket.server_ip.s_addr = inet_addr(RF433_CFG_SRV_IP);
    rf433i.socket.server_port = RF433_CFG_UDP_PORT;

    rf433i.rf433.net_id = RF433_CFG_NET_ID;
    rf433i.rf433.local_addr = RF433_CFG_LOCAL_ADDR;
    rf433i.rf433.rate = RF433_CFG_RATE;
    rf433i.rf433.freq = RF433_WFREQ(rf433i.rf433.net_id);

    INIT_LIST_HEAD(&rf433i.se433.list);
    rf433i.se433.num = 0;

    rf433i.tid = -1;
    rf433i.pipe_fd = -1;
    rf433i.sock_fd = -1;
    rf433i.rf433_fd = -1;

    return 0;
}

/*****************************************************************************
* Function Name  : load_config
* Description    : load config from nvram
* Input          : void
* Output         : None
* Return         : int
*                  - 0:ok
*****************************************************************************/
int load_config(void)
{
    char *val;

    /* get server ip address */
    if ((val = nvram_get(NVRAM_TYPE_NVRAM, RF433_NVR_SRV_IP)) != NULL) {
        if (get_ip(val, &rf433i.socket.server_ip) < 0) {
            sys_printf(SYS_ERR, "val %s for %s invalid", val, RF433_NVR_SRV_IP);
        }
    }

    /* get server udp port */
    if ((val = nvram_get(NVRAM_TYPE_NVRAM, RF433_NVR_UDP_PORT)) != NULL) {
        if (get_port(val, &rf433i.socket.server_port) < 0) {
            sys_printf(SYS_ERR, "val %s for %s invalid", val, RF433_NVR_UDP_PORT);
        }
    }

    /* get rf433 wireless net id */
    if ((val = nvram_get(NVRAM_TYPE_NVRAM, RF433_NVR_NET_ID)) != NULL) {
        if (get_netid(val, &rf433i.rf433.net_id) < 0) {
            sys_printf(SYS_ERR, "val %s for %s invalid", val, RF433_NVR_NET_ID);
        }
    }

    /* get rf433 wireless recveive address */
    if ((val = nvram_get(NVRAM_TYPE_NVRAM, RF433_NVR_LOCAL_ADDR)) != NULL) {
        if (get_local_addr(val, &rf433i.rf433.local_addr) < 0) {
            sys_printf(SYS_ERR, "val %s for %s invalid", val, RF433_NVR_LOCAL_ADDR);
        }
    }

    TRACE("%-20s: 0x%04x", "netid", rf433i.rf433.net_id);
    TRACE("%-20s: 0x%08x", "local_addr", rf433i.rf433.local_addr);
    TRACE("%-20s: %s", "server_ip", inet_ntoa(rf433i.socket.server_ip));
    TRACE("%-20s: %d", "server_port", rf433i.socket.server_port);

    return 0;
}

/*****************************************************************************
* Function Name  : save_config
* Description    : save config to nvram
* Input          : void
* Output         : None
* Return         : int
*                  - 0:ok
*****************************************************************************/
int save_config(void)
{
    char buff[32] = { 0 };

    nvram_set(NVRAM_TYPE_NVRAM, RF433_NVR_SRV_IP, inet_ntoa(rf433i.socket.server_ip));

    snprintf(buff, 32, "%d", rf433i.socket.server_port);
    nvram_set(NVRAM_TYPE_NVRAM, RF433_NVR_UDP_PORT, buff);

    snprintf(buff, 32, "%d", RF433_NET_ID(rf433i.rf433.net_id));
    nvram_set(NVRAM_TYPE_NVRAM, RF433_NVR_NET_ID, buff);

    snprintf(buff, 32, "%x", rf433i.rf433.local_addr);
    nvram_set(NVRAM_TYPE_NVRAM, RF433_NVR_LOCAL_ADDR, buff);

    nvram_commit(NVRAM_TYPE_NVRAM);

    return 0;
}

/*****************************************************************************
* Function Name  : relay_rf433
* Description    : rswp433 protocol implement and udp data send
* Input          : rf433_instence
* Output         : None
* Return         : int
*                  - 0:ok
*****************************************************************************/
int relay_rf433(rf433_instence *rf433x)
{
    fd_set rset, wset;
    int max_fd, ret;
    int r_433, w_433, r_udp, w_udp, p_proc;
    struct timeval time;
    buffer *rf433_rbuf, *rf433_wbuf, *udp_rbuf, *udp_wbuf;
    se433_list *se433l;
    rswp433_pkg *pkg;

#define MS(rr,wr,ru,wu,p) do { r_433=rr; w_433=wr; r_udp=ru; w_udp=wu; p_proc=p; } while(0)

    rf433_rbuf = rf433_wbuf = udp_rbuf = udp_wbuf = NULL;

    rf433_rbuf = buf_new_max();
    if (rf433_rbuf == NULL) {
        sys_printf(SYS_ERR, "new rf433_rbuf memory error");
        goto out;
    }

    rf433_wbuf = buf_new_max();
    if (rf433_wbuf == NULL) {
        sys_printf(SYS_ERR, "new rf433_wbuf memory error");
        goto out;
    }

    udp_rbuf = buf_new_max();
    if (udp_rbuf == NULL) {
        sys_printf(SYS_ERR, "new udp_rbuf memory error");
        goto out;
    }

    udp_wbuf = buf_new_max();
    if (udp_wbuf == NULL) {
        sys_printf(SYS_ERR, "new udp_wbuf memory error");
        goto out;
    }

    pkg = rswp433_pkg_new();
    if (pkg == NULL) {
        sys_printf(SYS_ERR, "new rswp433_pkg memory error");
        goto out;
    }


    MS(1, 0, 0, 0, 0);

    while (!exitflag) {

        TRACE("MachineState(rf=%d,wf=%d,ru=%d,wu=%d,p=%d)\n", r_433, w_433, r_udp, w_udp, p_proc);

        FD_ZERO(&rset);
        FD_ZERO(&wset);

        /* caculate the delay timer */
        time.tv_sec = RF433_SE433_MAX / (rf433x->se433.num == 0 ? 1 : rf433x->se433.num);
        time.tv_usec = 0;

        /*
         * check pipe read for main process signal
         */
        FD_SET(rf433x->pipe_fd, &rset);
        max_fd = max(0, rf433x->pipe_fd);

        if (r_433) {
            FD_SET(rf433x->rf433_fd, &rset);
        }
        if (w_433) {
            FD_SET(rf433x->rf433_fd, &wset);
        }
        max_fd = max(max_fd, rf433x->rf433_fd);

        if (r_udp) {
            FD_SET(rf433x->sock_fd, &rset);
        }
        if (w_udp) {
            FD_SET(rf433x->sock_fd, &wset);
        }
        max_fd = max(max_fd, rf433x->sock_fd);


        TRACE("select ALGORITHM delay %d(s)\n", (int)time.tv_sec);

        /* monitor all file discription with read and write operate
         * time out is caculate by se433 number
         */
        ret = select(max_fd + 1, &rset, &wset, NULL, &time);

        if (ret == -1) {
            /* signal interrupt the select */
            if (errno == EAGAIN || errno == EINTR)
                continue;

            sys_printf(SYS_ERR, "relay_rf433():select error:%s", strerror(errno));

            /* exit the thread */
            goto out;
        }

        /* select is time out
         * process timeout funcion
         */
        if (ret == 0) {

            TRACE("select time out\n");

            //se433_list_show(&rf433x->se433);

            /* remove offline se433 in the list */
            se433l = se433_find_offline(&rf433x->se433);
            if (se433l != NULL) {
                sys_printf(SYS_WARNING, "se433 0x%08x offline, req_cnt=%d, rsp_cnt=%d",
                        se433l->se433.addr, se433l->se433.req_cnt, se433l->se433.rsp_cnt);
                se433_del(&rf433x->se433, se433l->se433.addr);
            }

            /* request new data for the earliest se433 */
            se433l = se433_find_earliest(&rf433x->se433);
            if (se433l == NULL) {
                sys_printf(SYS_NOTICE, "can not find the earliest se433");
                continue;
            }

            TRACE("request new data for the se433 0x%08x\n", se433l->se433.addr);
            rswp433_data_req(se433l, rf433x, rf433_wbuf);

            /* next data request write to rf433 */
            MS(0, 1, 0, 0, 0);
            continue;
        }

        /* new msg from main process */
        if (FD_ISSET(rf433x->pipe_fd, &rset)) {
            char x;

            sys_printf(SYS_INFO, "got main process write pipe signal");

            while (read(rf433x->pipe_fd, &x, 1) > 0) {}

            /* exit the thread */
            goto out;
        }

        /* read from 433 */
        if (FD_ISSET(rf433x->rf433_fd, &rset)) {

            TRACE("rf433_fd can be read\n");

            ret = buf_read(rf433x->rf433_fd, rf433_rbuf);

            if (ret == -1) {
                sys_printf(SYS_ERR, "read(rf433_fd) error: %s", strerror(errno));

                /* exit the thread */
                goto out;

            } else if (ret == 0) {
                /* maybe error */
                sys_printf(SYS_ERR, "read(rf433_fd) closed");

                /* exit the thread */
                goto out;

            } else {
                sys_printf(SYS_INFO, "read %d bytes from 433", ret);

                /* next process data from rf433 rbuf */
                MS(0, 0, 0, 0, 1);
            }
        }

        /* write to 433 */
        if (FD_ISSET(rf433x->rf433_fd, &wset)) {

            TRACE("rf433_fd can be write\n");

            ret = buf_write(rf433x->rf433_fd, rf433_wbuf);

            if (ret == -1) {
                sys_printf(SYS_ERR, "write() error: %s", strerror(errno));

                /* exit the thread */
                goto out;

            } else {
                sys_printf(SYS_INFO, "write %d bytes to 433", ret);

                /* next read data to rf433 rbuf */
                MS(1, 0, 0, 0, 0);
            }
        }

#if 0
        /**
         * no Requirements data from udp
         */

        /* read from udp */
        if (FD_ISSET(rf433x->sock_fd, &rset)) {

            /*
            len = sizeof(cliaddr);
            ret = recvfrom(rf433x->sock_fd, udp_wbuf, UDP_BUF_SIZE - len_u2r, 0,
                    (struct sockaddr*)&cliaddr, &len);
            */
            ret = buf_read(rf433x->sock_fd, udp_wbuf);

            if (ret == -1) {
                sys_printf(SYS_ERR, "recvfrom(sock_fd) error: %s", strerror(errno));

                /* exit the thread */
                goto out;

            } else if (ret == 0) {
                /* maybe error */
                sys_printf(SYS_ERR, "read(sock_fd) closed");

                /* exit the thread */
                goto out;

            } else {
                sys_printf(SYS_INFO, "read %d bytes from UDP", ret);

                if (loglevel == SYS_DEBUG) {
                    buf_dump(udp_wbuf);
                }

                MS(0, 1, 0, 0, 0);
            }
        }
#endif

        /* write to udp */
        if (FD_ISSET(rf433x->sock_fd, &wset)) {
            int len;
            struct sockaddr_in cliaddr;

            TRACE("sock_fd can be write\n");

            len = sizeof(cliaddr);
            bzero(&cliaddr, len);

            memcpy(&cliaddr.sin_addr, &rf433x->socket.server_ip, sizeof(cliaddr.sin_addr));
            cliaddr.sin_port = htons(rf433x->socket.server_port);
            cliaddr.sin_family = AF_INET;

            ret = sendto(rf433x->sock_fd, buf_data(udp_wbuf), buf_len(udp_wbuf), 0,
                    (struct sockaddr*)&cliaddr, len);
            /*
            ret = buf_write(rf433x->sock_fd, rf433_rbuf);
            */

            if (ret == -1) {
                sys_printf(SYS_ERR, "write(udp_fd) error: %s", strerror(errno));

                /* exit the thread */
                goto out;

            } else {
                sys_printf(SYS_INFO, "write %d bytes to UDP %s:%d", ret,
                        inet_ntoa(rf433x->socket.server_ip), rf433x->socket.server_port);

                /* next read data to rf433 rbuf */
                MS(1, 0, 0, 0, 0);
            }

            buf_clean(udp_wbuf);
        }

        /**
         * some leave packet to process
         */
        if (p_proc) {

            TRACE("some leave packet to process\n");

            /* got a rswp433 packet */
            if (rswp433_pkg_analysis(rf433_rbuf, pkg) == 1) {

                TRACE("got a valid rswp433 pkg\n");

                /* check the rswp433 command */
                switch (pkg->u.content.cmd) {

                    case RSWP433_CMD_REG_REQ:

                        /* check the address */
                        if (pkg->u.content.dest_addr != RF433_RCVADDR_MAX) {

                            sys_printf(SYS_WARNING, "0x%08x is not register addr, drop this packet\n",
                                pkg->u.content.dest_addr);

                            /* next read data to rf433 rbuf */
                            MS(1, 0, 0, 0, 0);
                        }

                        else {

                            /* first register the se433 */
                            if ((se433l = rswp433_reg_req(pkg, rf433x)) != NULL) {

                                /* second, response the reg_ok message to se433 */
                                rswp433_reg_rsp(se433l, rf433x, rf433_wbuf);

                                /* next write response data to rf433 */
                                MS(0, 1, 0, 0, 0);

                            } else {

                                /* register error, next reread the rf433 data */
                                MS(1, 0, 0, 0, 0);
                            }
                        }

                        break;

                    case RSWP433_CMD_DATA_RSP:

                        /* check if my address */
                        if (pkg->u.content.dest_addr != rf433x->rf433.local_addr) {

                            sys_printf(SYS_WARNING, "0x%08x is not my addr, drop this packet\n",
                                pkg->u.content.dest_addr);

                            /* next read data to rf433 rbuf */
                            MS(1, 0, 0, 0, 0);

                        } else {

                            /* got the se433 sensor data */
                            if (rswp433_data_rsp(pkg, rf433x, udp_wbuf) < 0) {
                                MS(1, 0, 0, 0, 0);
                            }

                            /* next write rswp433 protocol data to udp */
                            else {
                                MS(0, 0, 0, 1, 0);
                            }
                        }

                        break;

                    default:
                        TRACE("invalid rswp433 cmd 0x%x, drop the packet\n", pkg->u.content.cmd);
                        MS(1, 0, 0, 0, 0);

                        break;
                }

                buf_clean(rf433_rbuf);
                rswp433_pkg_clr(pkg);

                continue;

            } else {

                TRACE("got a invalid rswp433 pkg\n");

                if (loglevel == SYS_DEBUG) {
                    buf_dump(rf433_rbuf);
                }

                /* not a valid rswp433 packet */
                buf_clean(rf433_rbuf);
                rswp433_pkg_clr(pkg);
                MS(1, 0, 0, 0, 0);

            }

        }
    }

out:
    buf_free(rf433_rbuf);
    buf_free(rf433_wbuf);
    buf_free(udp_rbuf);
    buf_free(udp_wbuf);
    rswp433_pkg_del(pkg);
    return 0;

}


/*****************************************************************************
* Function Name  : rf433_repeater_job
* Description    : rf433 repeater pthread
* Input          : (rf433_instence*)void*
* Output         : None
* Return         : void *
*                  - 0:ok
*****************************************************************************/
void *rf433_repeater_job(void *p)
{
    rf433_instence *rf433x;

    TRACE("enter rf433_repeater_job");

    rf433x = (rf433_instence*)p;

    while (INST_STAUTS(rf433x->status) == INST_START) {

        rf433x->rf433_fd = open_rf433(RF433_RF_DEV_NAME);
        if (rf433x->rf433_fd == -1) {
            sys_printf(SYS_ERR, "open_rf433 error");
            pthread_exit((void*)1);
        }
        set_rf433_opt(rf433x->rf433_fd, rf433x->rf433.net_id, rf433x->rf433.rate);

        rf433x->sock_fd = open_socket();
        if (rf433x->sock_fd == -1) {
            sys_printf(SYS_ERR, "open_socket error");
            pthread_exit((void*)1);
        }
        set_socket_opt(rf433x->sock_fd, rf433x->socket.local_port,
                rf433x->socket.server_ip, rf433x->socket.server_port);

        relay_rf433(rf433x);

        close(rf433x->rf433_fd);
        close(rf433x->sock_fd);

        rf433x->rf433_fd = -1;
        rf433x->sock_fd = -1;

        /* wait 3 second for while(1) */
        //sleep(3);
    }

    close(rf433x->pipe_fd);
    rf433x->pipe_fd = -1;

    TRACE("leave rf433_repeater_job");

    pthread_exit((void*)0);
}

/*****************************************************************************
* Function Name  : thread_create
* Description    : create rf433 pthread
* Input          : void
* Output         : None
* Return         : int
*                  - 0:ok
*****************************************************************************/
int thread_create(void)
{
    int err;
    int childpipe[2];

    if (pipe(childpipe) < 0) {
        sys_printf(SYS_ERR, "error creating rf433 daemon pipe");
        return -1;
    }

    spipefd[0] = childpipe[0];
    rf433i.pipe_fd = childpipe[1];

    rf433i.status = INST_START;

    err = pthread_create(&rf433i.tid, NULL, rf433_repeater_job, (void*)&rf433i);

    if (err == -1) {
        sys_printf(SYS_ERR, "rfrepeater thread create faild!");
        return err;
    }

    sys_printf(SYS_INFO, "rfrepeater tid %ld thread has started", (long)rf433i.tid);

    return 0;
}

/*****************************************************************************
* Function Name  : thread_close
* Description    : close rf433 pthread
* Input          : void
* Output         : None
* Return         : int
*                  - 0:ok
*****************************************************************************/
int thread_close(void)
{
    int thread_ret = 0;

    sys_printf(SYS_ALERT, "close the pthread tid is %ld", (long)rf433i.tid);

    rf433i.status &= (~INST_START);

    write(spipefd[0], "0", 1);
    close(spipefd[0]);
    spipefd[0] = -1;

    thread_ret = pthread_join(rf433i.tid, NULL);
    sys_printf(SYS_ALERT, "closed pthread %ld ret is %d",
            (long)rf433i.tid, thread_ret);

    rf433i.tid = 0;
    rf433i.pipe_fd = -1;
    rf433i.sock_fd = -1;
    rf433i.rf433_fd = -1;

    return 0;
}

/*****************************************************************************
* Function Name  : msg_to_read_config
* Description    : receive message to read config
* Input          : void
* Output         : None
* Return         : int
*                  - 0:ok
*****************************************************************************/
int msg_to_read_config(void)
{
    return load_config();
}

/*****************************************************************************
* Function Name  : msg_to_save_config
* Description    : receive message to save config
* Input          : void
* Output         : None
* Return         : int
*                  - 0:ok
*****************************************************************************/
int msg_to_save_config(void)
{
    return save_config();
}

/*****************************************************************************
* Function Name  : msg_to_get_config
* Description    : receive message to get current config
* Input          : message client id
* Output         : None
* Return         : int
*                  - 0:ok
*****************************************************************************/
int msg_to_get_config(long client_pid)
{
    struct msg_st rf433_msg, sock_msg;

    /* send rf433 config */
    rf433_msg.mkey = client_pid;
    rf433_msg.mtype = MSG_RSP_RF433;
    memcpy(&rf433_msg.u.rf433, &rf433i.rf433, sizeof(rf433_cfg));
    msgsnd(client_msg_qid, &rf433_msg, sizeof(rf433_msg) - sizeof(long), 0);

    /* send socket config */
    sock_msg.mkey = client_pid;
    sock_msg.mtype = MSG_RSP_SOCKET;
    memcpy(&sock_msg.u.socket, &rf433i.socket, sizeof(socket_cfg));
    msgsnd(client_msg_qid, &sock_msg, sizeof(sock_msg) - sizeof(long), 0);

    return 0;
}

/*****************************************************************************
* Function Name  : msg_to_get_se433list
* Description    : receive message to get se433 list data
* Input          : message client id
* Output         : None
* Return         : int
*                  - 0:ok
*****************************************************************************/
int msg_to_get_se433list(long client_pid)
{
    struct msg_st se433_msg;
    se433_list *se433l;

    /* send sensor 433 address and count */
    list_for_each_entry(se433l, &rf433i.se433.list, list) {
        se433_msg.mkey = client_pid;
        se433_msg.mtype = MSG_RSP_SE433L;
        memcpy(&se433_msg.u.se433data, &se433l->se433, sizeof(se433_data));
        msgsnd(client_msg_qid, &se433_msg, sizeof(se433_msg) - sizeof(long), 0);
    }

    return 0;
}

/*****************************************************************************
* Function Name  : msg_to_resp
* Description    : response message to client
* Input          : client_id, string format
* Output         : None
* Return         : int
*                  -  0:ok
*                  - -1:error
*****************************************************************************/
int msg_to_resp(long client_pid, char *str, ...)
{
    struct msg_st resp_msg;
    char tmpline[MSG_RESP_LEN];
    va_list args;

    memset(tmpline, 0, MSG_RESP_LEN);

    va_start(args, str);
    vsnprintf(tmpline, MSG_RESP_LEN - 1, str, args);
    va_end(args);

    sys_printf(SYS_DEBUG, "response for the message");

    resp_msg.mkey = client_pid;
    resp_msg.mtype = MSG_RSP_END;

    memset(resp_msg.u.resp, 0, MSG_RESP_LEN);
    strncpy(resp_msg.u.resp, tmpline, MSG_RESP_LEN-1);

    if (msgsnd(client_msg_qid, &resp_msg, sizeof(resp_msg) - sizeof(long), 0) == -1) {
        return -1;
    }

    sys_printf(SYS_DEBUG, "send the response ok");

    return 0;
}

/*****************************************************************************
* Function Name  : msg_to_rf433
* Description    : receive message to set rf433 config
* Input          : rf433_cfg*
* Output         : None
* Return         : int
*                  -  0:ok
*****************************************************************************/
int msg_to_rf433(rf433_cfg *rf433)
{
    if (rf433->m_mask & RF433_MASK_NET_ID) {
        rf433i.rf433.net_id = rf433->net_id;
    }

    if (rf433->m_mask & RF433_MASK_RCV_ADDR) {
        rf433i.rf433.local_addr = rf433->local_addr;
    }

    return 0;
}

/*****************************************************************************
* Function Name  : msg_to_socket
* Description    : receive message to set socket config
* Input          : socket_cfg*
* Output         : None
* Return         : int
*                  -  0:ok
*****************************************************************************/
int msg_to_socket(socket_cfg *socket)
{
    if (socket->m_mask & SOCK_MASK_SER_IP) {
        rf433i.socket.server_ip = socket->server_ip;
    }

    if (socket->m_mask & SOCK_MASK_UDP_PORT) {
        rf433i.socket.server_port = socket->server_port;
    }

    return 0;
}

/*****************************************************************************
* Function Name  : msg_to_se433
* Description    : receive message to set se433 config
* Input          : se433_cfg*
* Output         : None
* Return         : int
*                  -  0:ok
*****************************************************************************/
int msg_to_se433(se433_cfg *se433)
{
    switch (se433->op) {
        case SE433_OP_ADD:
            return (se433_add(&rf433i.se433, se433->se433_addr) ? 0 : -1);
            break;

        case SE433_OP_DEL:
            return se433_del(&rf433i.se433, se433->se433_addr);
            break;

        default:
            sys_printf(SYS_ERR, "se433 op %d unsupport", se433->op);
            return -1;
    }

    return 0;
}

/*****************************************************************************
* Function Name  : msg_to_log_level
* Description    : receive message to set loglevel
* Input          : loglevel
* Output         : None
* Return         : int
*                  -  0:ok
*****************************************************************************/
int msg_to_log_level(uint32_t log)
{
    set_loglevel(log);
    return 0;
}

/*****************************************************************************
* Function Name  : sigintterm_handler
* Description    : catch ctrl-c or sigterm
* Input          : None
* Output         : None
* Return         : void
*****************************************************************************/
static void sigintterm_handler(int UNUSED(unused)) {

	exitflag = 1;
    sys_printf(SYS_EMERG, "rfrepeater daemon shutdown");
    //remove(LOCKFILE);
    exit(0);
}

/*****************************************************************************
* Function Name  : sigsegv_handler
* Description    : catch any segvs
* Input          : None
* Output         : None
* Return         : void
*****************************************************************************/
static void sigsegv_handler(int UNUSED(unused)) {
	sys_printf(SYS_EMERG, "Aiee, segfault! You should probably report "
			"this as a bug to the developer\n");
	exit(EXIT_FAILURE);
}

/*****************************************************************************
* Function Name  : main
* Description    : rfrepeater main function
* Input          : argc, argv
* Output         : None
* Return         : int
*                  -  0:ok
*****************************************************************************/
int main(int argc,char* argv[])
{
    int i, ret;
    struct msg_st msg;

    for (i = 1; i < (unsigned int)argc; i++)
    {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'd':
                    debug_mode = 1;
                    break;
                case 'v':
                case 'h':
                    fprintf(stderr, USAGE);
                    exit(EXIT_FAILURE);
                    break;
                default:
                    fprintf(stderr, "Unknown argument %s\n", argv[i]);
                    exit(EXIT_FAILURE);
                    break;
            }
        }
    }


    if (debug_mode == 1) {
        set_loglevel(SYS_DEBUG);
    } else {
        set_loglevel(SYS_ERR);
        openlog(RF433_LOG_F, 0, LOG_DAEMON);
    }

    /* set up cleanup handler */
    if (signal(SIGINT, sigintterm_handler) == SIG_ERR ||
            signal(SIGTERM, sigintterm_handler) == SIG_ERR ||
            signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        fprintf(stderr, "signal()1 error\n");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGSEGV, sigsegv_handler) == SIG_ERR) {
        fprintf(stderr, "signal()2 error\n");
        exit(EXIT_FAILURE);
    }

    if (!debug_mode) {
        if (daemon(0, 0) < 0) {
            fprintf(stderr, "Failed to daemonize: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    sys_printf(SYS_ALERT, "rfrepeater daemon start");
    //atexit(exit_job);
    //make_pid_file();

    default_init();

    server_msg_qid = msgget((key_t)SERVER_MSG, IPC_CREAT|0666);
    if (server_msg_qid == -1) {
        sys_printf(SYS_ERR, "msgget(server_msg_qid):%s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    client_msg_qid = msgget((key_t)CLIENT_MSG, IPC_CREAT|0666);
    if (client_msg_qid == -1) {
        sys_printf(SYS_ERR, "msgget(client_msg_qid):%s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (load_config()) {
        sys_printf(SYS_INFO, "load_config() error, will use default config");
    }

    if (thread_create()) {
        sys_printf(SYS_ERR, "thread_create() error");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        ret = msgrcv(server_msg_qid, &msg, sizeof(msg)-sizeof(long), 0, 0);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }
            sys_printf(SYS_ERR, "main():msgrcv():%d:%s", errno, strerror(errno));
            break;
        }

        switch (msg.mtype) {

            case MSG_REQ_SET_RF433:
                sys_printf(SYS_DEBUG, "MSG_REQ_SET_RF433");
                thread_close();
                if (msg_to_rf433(&msg.u.rf433) == -1) {
                    msg_to_resp(msg.mkey, "msg_to_rf433 error");
                    //break;
                }
                if (thread_create()) {
                    msg_to_resp(msg.mkey, "restart thread error");
                    break;
                }
                msg_to_resp(msg.mkey, "");
                break;

            case MSG_REQ_SET_SOCK:
                sys_printf(SYS_DEBUG, "MSG_REQ_SET_SOCK");
                thread_close();
                if (msg_to_socket(&msg.u.socket) == -1) {
                    msg_to_resp(msg.mkey, "msg_to_socket error");
                    //break;
                }
                if (thread_create()) {
                    msg_to_resp(msg.mkey, "restart thread error");
                    break;
                }
                msg_to_resp(msg.mkey, "");
                break;

            case MSG_REQ_READ_CFG:
                sys_printf(SYS_DEBUG, "MSG_REQ_READ_CFG");
                thread_close();
                if (msg_to_read_config() == -1) {
                    msg_to_resp(msg.mkey, "msg_to_read_config error");
                    sys_printf(SYS_ALERT, "read config error, use default config");
                    default_init();
                }
                if (thread_create()) {
                    msg_to_resp(msg.mkey, "restart thread error");
                    break;
                }
                msg_to_resp(msg.mkey, "");
                break;

            case MSG_REQ_SAVE_CFG:
                sys_printf(SYS_DEBUG, "MSG_REQ_SAVE_CFG");
                if (msg_to_save_config() == -1) {
                    msg_to_resp(msg.mkey, "msg_to_save_config error");
                    break;
                }
                msg_to_resp(msg.mkey, "");
                break;

            case MSG_REQ_GET_CFG:
                sys_printf(SYS_DEBUG, "MSG_REQ_GET_CFG");
                if (msg_to_get_config(msg.mkey) == -1) {
                    msg_to_resp(msg.mkey, "msg_to_get_config error");
                    break;
                }
                msg_to_resp(msg.mkey, "");
                break;

            case MSG_REQ_GET_SE433L:
                sys_printf(SYS_DEBUG, "MSG_RSP_SE433L");
                if (msg_to_get_se433list(msg.mkey) == -1) {
                    msg_to_resp(msg.mkey, "msg_to_get_stat error");
                    break;
                }
                msg_to_resp(msg.mkey, "");
                break;

            case MSG_REQ_SET_SE433:
                sys_printf(SYS_DEBUG, "MSG_REQ_SET_SE433");
                if (msg_to_se433(&msg.u.se433) == -1) {
                    msg_to_resp(msg.mkey, "msg_to_se433 error");
                }
                msg_to_resp(msg.mkey, "");
                break;

            case MSG_REQ_SET_LOG:
                sys_printf(SYS_DEBUG, "MSG_REQ_SET_LOG");
                if (msg_to_log_level(msg.u.msg_level) == -1) {
                    msg_to_resp(msg.mkey, "msg_to_log_level error");
                    break;
                }
                msg_to_resp(msg.mkey, "");
                break;

            default:
                sys_printf(SYS_DEBUG, "Unknown MSG command %ld", msg.mtype);
                msg_to_resp(msg.mkey, "");
                break;
        }
    }

    sys_printf(SYS_INFO, "rf433 daemon terminaled!");

    exit(0);
}

