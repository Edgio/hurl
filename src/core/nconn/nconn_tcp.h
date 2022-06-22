//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _NCONN_TCP_H
#define _NCONN_TCP_H
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include "nconn.h"
// ---------------------------------------------------------
// KTLS changes requires a #define for TCP_ULP
// only defined in /usr/include/linux/tcp.h
// (propbably from kernel version >= 4.13 when KTLS support was first added).
// since KTLS is a linux only feature thus far, only
// required define is repeated here
// for code portability's sake
// based on:
// https://stackoverflow.com/questions/34121434/difference-in-netinet-tcp-h-vs-linux-tcp-h
// ---------------------------------------------------------
#ifdef KTLS_SUPPORT
  #ifdef __linux__
    #ifndef TCP_ULP
      #define TCP_ULP 31
    #endif
  #endif
#endif
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! \details: TODO
//! ----------------------------------------------------------------------------
class nconn_tcp: public nconn
{
public:
        // -------------------------------------------------
        // Connection state
        // -------------------------------------------------
        typedef enum tcp_conn_state
        {
                TCP_STATE_NONE,
                TCP_STATE_LISTENING,
                TCP_STATE_ACCEPTING,
                TCP_STATE_CONNECTING,
                TCP_STATE_CONNECTED,
                TCP_STATE_READING,
                TCP_STATE_WRITING,
                TCP_STATE_DONE
        } tcp_conn_state_t;
        // -------------------------------------------------
        // Options
        // -------------------------------------------------
        typedef enum tcp_opt_enum
        {
                OPT_TCP_RECV_BUF_SIZE = 0,
                OPT_TCP_SEND_BUF_SIZE = 1,
                OPT_TCP_NO_DELAY = 2,
                OPT_TCP_NO_LINGER = 10,
                OPT_TCP_FD = 100,
                OPT_TCP_SENTINEL = 999
        } tcp_opt_t;
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        nconn_tcp():
          nconn(),
          m_fd(-1),
          m_sock_opt_recv_buf_size(),
          m_sock_opt_send_buf_size(),
          m_sock_opt_no_delay(false),
          m_sock_opt_no_linger(false),
          m_tcp_state(TCP_STATE_NONE)
        {
                m_scheme = SCHEME_TCP;
        };
        ~nconn_tcp() {};
        int32_t set_opt(uint32_t a_opt, const void *a_buf, uint32_t a_len);
        int32_t get_opt(uint32_t a_opt, void **a_buf, uint32_t *a_len);
        bool is_listening(void) {return (m_tcp_state == TCP_STATE_LISTENING);};
        bool is_connecting(void) {return (m_tcp_state == TCP_STATE_CONNECTING);};
        bool is_accepting(void) {return (m_tcp_state == TCP_STATE_ACCEPTING);};
        // -------------------------------------------------
        // virtual methods
        // -------------------------------------------------
        int32_t ncset_listening(int32_t a_val);
        int32_t ncset_listening_nb(int32_t a_val);
        int32_t ncset_accepting(int a_fd);
        int32_t ncset_connected(void);
        int32_t ncsetup();
        int32_t ncread(char *a_buf, uint32_t a_buf_len);
        int32_t ncwrite(char *a_buf, uint32_t a_buf_len);
        int32_t ncaccept();
        int32_t ncconnect();
        int32_t nccleanup();
        // -------------------------------------------------
        // Protected members
        // -------------------------------------------------
        int m_fd;
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;
        bool m_sock_opt_no_linger;
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        nconn_tcp& operator=(const nconn_tcp &);
        nconn_tcp(const nconn_tcp &);
        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        tcp_conn_state_t m_tcp_state;
};
} //namespace ns_hurl {
#endif
