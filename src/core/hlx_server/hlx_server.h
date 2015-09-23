//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlx_server.h
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
#ifndef _HLX_SERVER_H
#define _HLX_SERVER_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <pthread.h>
#include <signal.h>

#include <string>
#include <list>
#include <vector>
#include <map>
#include <stdint.h>
#include "http_request_handler.h"
#include "nbq.h"
#include "nconn.h"

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define HLX_SERVER_STATUS_OK 0
#define HLX_SERVER_STATUS_ERROR -1

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define HLX_SERVER_DISALLOW_ASSIGN(class_name)\
    class_name& operator=(const class_name &);
#define HLX_SERVER_DISALLOW_COPY(class_name)\
    class_name(const class_name &);
#define HLX_SERVER_DISALLOW_COPY_AND_ASSIGN(class_name)\
    HLX_SERVER_DISALLOW_COPY(class_name)\
    HLX_SERVER_DISALLOW_ASSIGN(class_name)

//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------
struct ssl_ctx_st;
typedef ssl_ctx_st SSL_CTX;

namespace ns_hlx {
//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
class t_server;
typedef std::list <t_server *> t_server_list_t;

//: ----------------------------------------------------------------------------
//: hlx_server
//: ----------------------------------------------------------------------------
class hlx_server
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        hlx_server();
        ~hlx_server();

        // General
        void set_stats(bool a_val);
        void set_verbose(bool a_val);
        void set_color(bool a_val);

        // Settings
        void set_port(uint16_t a_port);
        void set_num_threads(uint32_t a_num_threads);
        void set_num_parallel(uint32_t a_num_parallel);
        void set_start_time_ms(uint64_t a_start_time_ms) {m_start_time_ms = a_start_time_ms;}

        // TLS
        void set_scheme(nconn::scheme_t a_scheme);
        void set_ssl_cipher_list(const std::string &a_cipher_list);
        void set_ssl_ca_path(const std::string &a_ssl_ca_path);
        void set_ssl_ca_file(const std::string &a_ssl_ca_file);
        int set_ssl_options(const std::string &a_ssl_options_str);
        int set_ssl_options(long a_ssl_options);
        void set_tls_key(const std::string &a_tls_key) {m_tls_key = a_tls_key;}
        void set_tls_crt(const std::string &a_tls_crt) {m_tls_crt = a_tls_crt;}

        // Running...
        int32_t run(void);
        int32_t stop(void);
        int32_t wait_till_stopped(void);
        bool is_running(void);
        int32_t add_endpoint(const std::string &a_endpoint, const http_request_handler *a_handler);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        HLX_SERVER_DISALLOW_COPY_AND_ASSIGN(hlx_server)
        int init(void);
        int init_server_list(void);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        bool m_verbose;
        bool m_color;
        bool m_stats;
        uint16_t m_port;
        uint32_t m_num_threads;
        int32_t m_num_parallel;
        nconn::scheme_t m_scheme;
        SSL_CTX* m_ssl_ctx;
        std::string m_tls_key;
        std::string m_tls_crt;
        std::string m_ssl_cipher_list;
        std::string m_ssl_options_str;
        long m_ssl_options;
        std::string m_ssl_ca_file;
        std::string m_ssl_ca_path;
        uint64_t m_start_time_ms;
        t_server_list_t m_t_server_list;
        int m_evr_loop_type;
        url_router m_url_router;
        int32_t m_fd;
        bool m_is_initd;
};


} //namespace ns_hlx {

#endif


