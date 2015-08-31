//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn_tcp.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    02/07/2014
//:
//:   Licensed under the Apache License, Version 2.0 (the "License");
//:   you may not use this file except in compliance with the License.
//:   You may obtain a copy of the License at
//:
//:       http://www.apache.org/licenses/LICENSE-2.0
//:
//:   Unless required by applicable law or agreed to in writing, software
//:   distributed under the License is distributed on an "AS IS" BASIS,
//:   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//:   See the License for the specific language governing permissions and
//:   limitations under the License.
//:
//: ----------------------------------------------------------------------------
#ifndef _NCONN_TCP_H
#define _NCONN_TCP_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "nconn.h"
#include "ndebug.h"
#include "http_cb.h"

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------


namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class nconn_tcp: public nconn
{
public:
        // ---------------------------------------
        // Options
        // ---------------------------------------
        typedef enum tcp_opt_enum
        {
                OPT_TCP_REQ_BUF = 0,
                OPT_TCP_REQ_BUF_LEN = 1,
                OPT_TCP_GLOBAL_REQ_BUF = 2,
                OPT_TCP_GLOBAL_REQ_BUF_LEN = 3,
                OPT_TCP_RECV_BUF_SIZE = 4,
                OPT_TCP_SEND_BUF_SIZE = 5,
                OPT_TCP_NO_DELAY = 6,

                OPT_TCP_SENTINEL = 999
        } tcp_opt_t;

        nconn_tcp(bool a_verbose,
                  bool a_color,
                  int64_t a_max_reqs_per_conn = -1,
                  bool a_save_response_in_reqlet = false,
                  bool a_collect_stats = false,
                  bool a_connect_only = false):
                          nconn(a_verbose,
                                a_color,
                                a_max_reqs_per_conn,
                                a_save_response_in_reqlet,
                                a_collect_stats,
                                a_connect_only),
                          m_tcp_state(TCP_STATE_FREE),
                          m_fd(-1),
                          m_http_parser_settings(),
                          m_http_parser(),
                          m_sock_opt_recv_buf_size(),
                          m_sock_opt_send_buf_size(),
                          m_sock_opt_no_delay(false),
                          m_timeout_s(10),
                          m_req_buf(),
                          m_req_buf_len(0),
                          m_read_buf(),
                          m_read_buf_idx(0),
                          s_req_buf(),
                          s_req_buf_len()

        {
                m_scheme = SCHEME_TCP;

                // Set up callbacks...
                m_http_parser_settings.on_status = hp_on_status;
                m_http_parser_settings.on_message_complete = hp_on_message_complete;

                if(a_save_response_in_reqlet)
                {
                        m_http_parser_settings.on_message_begin = hp_on_message_begin;
                        m_http_parser_settings.on_url = hp_on_url;
                        m_http_parser_settings.on_header_field = hp_on_header_field;
                        m_http_parser_settings.on_header_value = hp_on_header_value;
                        m_http_parser_settings.on_headers_complete = hp_on_headers_complete;
                        m_http_parser_settings.on_body = hp_on_body;
                }
        };

        // Destructor
        ~nconn_tcp()
        {
        };

        int32_t run_state_machine(evr_loop *a_evr_loop, const host_info_t &a_host_info);
        int32_t send_request(bool is_reuse = false);
        int32_t cleanup(void);
        int32_t set_opt(uint32_t a_opt, const void *a_buf, uint32_t a_len);
        int32_t get_opt(uint32_t a_opt, void **a_buf, uint32_t *a_len);

        bool is_done(void) { return (m_tcp_state == TCP_STATE_DONE);}
        bool is_free(void) { return (m_tcp_state == TCP_STATE_FREE);}
        void set_state_done(void) { m_tcp_state = TCP_STATE_DONE; };

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------
        static const uint32_t m_max_req_buf = 16*1024;
        static const uint32_t m_max_read_buf = 16*1024;

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------

private:

        // ---------------------------------------
        // Connection state
        // ---------------------------------------
        typedef enum tcp_conn_state
        {
                TCP_STATE_FREE = 0,
                TCP_STATE_CONNECTING,
                TCP_STATE_CONNECTED,
                TCP_STATE_READING,
                TCP_STATE_DONE
        } tcp_conn_state_t;

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(nconn_tcp)

        int32_t receive_response(void);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        tcp_conn_state_t m_tcp_state;

        // -------------------------------------------------
        // Protectected methods
        // -------------------------------------------------
protected:
        int32_t setup_socket(const host_info_t &a_host_info);

        // -------------------------------------------------
        // Protectected members
        // -------------------------------------------------
protected:
        int m_fd;

        http_parser_settings m_http_parser_settings;
        http_parser m_http_parser;

        // Socket options
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;
        uint32_t m_timeout_s;

        char m_req_buf[m_max_req_buf];
        uint32_t m_req_buf_len;
        char m_read_buf[m_max_read_buf];
        uint32_t m_read_buf_idx;

        // -------------------------------------------------
        // Private static members
        // -------------------------------------------------
        char *s_req_buf;
        uint32_t s_req_buf_len;

};

} //namespace ns_hlx {

#endif



