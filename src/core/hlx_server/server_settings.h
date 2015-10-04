//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    server_settings.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    03/11/2015
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
#ifndef _SERVER_SETTINGS_H
#define _SERVER_SETTINGS_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlo/hlx_server.h"
#include "ndebug.h"
#include "evr.h"
#include "nconn.h"

#include <openssl/ssl.h>

// signal
#include <signal.h>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------


namespace ns_hlx {

typedef std::map<std::string, std::string> header_map_t;

//: ----------------------------------------------------------------------------
//: Settings
//: ----------------------------------------------------------------------------
typedef struct server_settings_struct
{
        bool m_verbose;
        bool m_color;
        bool m_quiet;
        bool m_show_summary;

        // request options
        std::string m_url;
        header_map_t m_header_map;
        std::string m_verb;
        char *m_req_body;
        uint32_t m_req_body_len;

        // run options
        evr_loop_type_t m_evr_loop_type;
        int32_t m_num_parallel;
        uint32_t m_timeout_s;
        int32_t m_run_time_s;
        int32_t m_rate;
        int32_t m_num_end_fetches;
        bool m_connect_only;
        int32_t m_num_reqs_per_conn;
        bool m_save_response;
        bool m_collect_stats;
        bool m_use_persistent_pool;

        // tcp options
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;

        // TLS options
        SSL_CTX* m_ssl_ctx;
        std::string m_ssl_cipher_list;
        std::string m_ssl_options_str;
        long m_ssl_options;
        bool m_ssl_verify;
        bool m_ssl_sni;
        std::string m_ssl_ca_file;
        std::string m_ssl_ca_path;

        nconn::scheme_t m_scheme;
        std::string m_tls_key;
        std::string m_tls_crt;

        // server fd
        int m_fd;

        // ---------------------------------
        // Defaults...
        // ---------------------------------
        server_settings_struct() :
                m_verbose(false),
                m_color(false),
                m_quiet(false),
                m_show_summary(false),
                m_url(),
                m_header_map(),
                m_verb("GET"),
                m_req_body(NULL),
                m_req_body_len(0),
                m_evr_loop_type(EVR_LOOP_EPOLL),
                m_num_parallel(64),

                // TODO Put back to something reasonable like 10
                m_timeout_s(100000),
                m_run_time_s(-1),
                m_rate(-1),
                m_num_end_fetches(-1),
                m_connect_only(false),
                m_num_reqs_per_conn(-1),
                m_save_response(false),
                m_collect_stats(false),
                m_use_persistent_pool(false),
                m_sock_opt_recv_buf_size(0),
                m_sock_opt_send_buf_size(0),
                m_sock_opt_no_delay(false),
                m_ssl_ctx(NULL),
                m_ssl_cipher_list(""),
                m_ssl_options_str(""),
                m_ssl_options(0),
                m_ssl_verify(false),
                m_ssl_sni(false),
                m_ssl_ca_file(""),
                m_ssl_ca_path(""),
                m_scheme(nconn::SCHEME_TCP),
                m_tls_key(""),
                m_tls_crt(""),
                m_fd(-1)
        {}

private:
        DISALLOW_COPY_AND_ASSIGN(server_settings_struct);

} server_settings_struct_t;

} //namespace ns_hlx {

#endif // #ifndef _SERVER_SETTINGS_H
