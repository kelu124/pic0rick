//!
//! \file       lwipopts.h
//! \brief      lwIP configuration for RP2350 DAQ web server
//!

#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// ---- Platform ----
#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0
#define SYS_LIGHTWEIGHT_PROT        0

// ---- Memory ----
#define MEM_LIBC_MALLOC             0
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_ARP_QUEUE          10
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    (16 * 1024)

// ---- Pbuf ----
#define PBUF_POOL_SIZE              24

// ---- TCP ----
#define LWIP_TCP                    1
#define TCP_MSS                     1460
#define TCP_WND                     (8 * TCP_MSS)
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define MEMP_NUM_TCP_PCB            8
#define MEMP_NUM_TCP_PCB_LISTEN     2

// ---- DHCP ----
#define LWIP_DHCP                   1

// ---- DNS (optional, useful for mDNS later) ----
#define LWIP_DNS                    1

// ---- ICMP (ping) ----
#define LWIP_ICMP                   1

// ---- Misc ----
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_HTTPD                  0         // we use our own server

// ---- Timers ----
#define LWIP_TIMERS                 1

// ---- Debug (uncomment for troubleshooting) ----
// #define LWIP_DEBUG                1
// #define TCP_DEBUG                 LWIP_DBG_ON
// #define HTTPD_DEBUG               LWIP_DBG_ON

#endif // _LWIPOPTS_H
