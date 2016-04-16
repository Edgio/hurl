//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    t_conf.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    10/05/2015
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
#ifndef _T_CONF_H
#define _T_CONF_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "evr.h"
#include "hlx/hlx.h"

// signal
#include <signal.h>

//: ----------------------------------------------------------------------------
//: Ext Fwd decl's
//: ----------------------------------------------------------------------------
typedef struct ssl_ctx_st SSL_CTX;

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Virtual Server conf
//: TODO Allow many t_hlx_conf's
//: one per listener
//: ----------------------------------------------------------------------------
typedef struct t_conf
{
        bool m_rqst_resp_logging;
        bool m_rqst_resp_logging_color;
        evr_loop_type_t m_evr_loop_type;
        int32_t m_num_parallel;
        uint32_t m_timeout_ms;
        int32_t m_num_reqs_per_conn;
        bool m_collect_stats;
        uint32_t m_update_stats_ms;
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;
        resp_done_cb_t m_resp_done_cb;
        hlx *m_hlx;

        // TLS Config
        // Server ctx
        SSL_CTX* m_tls_server_ctx;
        std::string m_tls_server_ctx_key;
        std::string m_tls_server_ctx_crt;
        std::string m_tls_server_ctx_cipher_list;
        std::string m_tls_server_ctx_options_str;
        long m_tls_server_ctx_options;

        // client ctx
        SSL_CTX *m_tls_client_ctx;
        std::string m_tls_client_ctx_cipher_list;
        std::string m_tls_client_ctx_options_str;
        long m_tls_client_ctx_options;
        std::string m_tls_client_ctx_ca_file;
        std::string m_tls_client_ctx_ca_path;

        // ---------------------------------
        // Defaults...
        // ---------------------------------
        t_conf():
                m_rqst_resp_logging(false),
                m_rqst_resp_logging_color(false),
#if defined(__linux__)
                m_evr_loop_type(EVR_LOOP_EPOLL),
#elif defined(__FreeBSD__) || defined(__APPLE__)
                m_evr_loop_type(EVR_LOOP_SELECT),
#else
                m_evr_loop_type(EVR_LOOP_SELECT),
#endif
                m_num_parallel(1024),
                m_timeout_ms(10000),
                m_num_reqs_per_conn(-1),
                m_collect_stats(false),
                m_update_stats_ms(0),
                m_sock_opt_recv_buf_size(0),
                m_sock_opt_send_buf_size(0),
                m_sock_opt_no_delay(false),
                m_resp_done_cb(NULL),
                m_hlx(NULL),
                m_tls_server_ctx(NULL),
                m_tls_server_ctx_key(),
                m_tls_server_ctx_crt(),
                m_tls_server_ctx_cipher_list(),
                m_tls_server_ctx_options_str(),
                m_tls_server_ctx_options(0),
                m_tls_client_ctx(NULL),
                m_tls_client_ctx_cipher_list(),
                m_tls_client_ctx_options_str(),
                m_tls_client_ctx_options(0),
                m_tls_client_ctx_ca_file(),
                m_tls_client_ctx_ca_path()
        {}

private:
        // Disallow copy/assign
        t_conf& operator=(const t_conf &);
        t_conf(const t_conf &);
} conf_t;

} //namespace ns_hlx {

#endif // #ifndef _T_CONF_H



