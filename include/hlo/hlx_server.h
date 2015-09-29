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
#include <signal.h>
#include <stdint.h>

#include <string>
#include <list>
#include <map>

#include "hlo/hlx_common.h"

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
//: Fwd decl's
//: ----------------------------------------------------------------------------
class t_server;
class url_router;

#ifndef URL_PARAM_MAP_T
#define URL_PARAM_MAP_T
typedef std::map <std::string, std::string> url_param_map_t;
#endif

//: ----------------------------------------------------------------------------
//: http_request_handler
//: ----------------------------------------------------------------------------
class http_request_handler
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        http_request_handler(void) {};
        virtual ~http_request_handler(){};

        virtual int32_t do_get(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response) = 0;
        virtual int32_t do_post(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response) = 0;
        virtual int32_t do_put(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response) = 0;
        virtual int32_t do_delete(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response) = 0;
        virtual int32_t do_default(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response) = 0;

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        int32_t get_file(const std::string &a_path, const http_req &a_request, http_resp &ao_response);
        int32_t send_not_found(const http_req &a_request, http_resp &ao_response, const char *a_resp_str);
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        HLX_SERVER_DISALLOW_COPY_AND_ASSIGN(http_request_handler)
};

//: ----------------------------------------------------------------------------
//: default_http_request_handler
//: ----------------------------------------------------------------------------
class default_http_request_handler: public http_request_handler
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        default_http_request_handler(void);
        ~default_http_request_handler();

        int32_t do_get(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response);
        int32_t do_post(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response);
        int32_t do_put(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response);
        int32_t do_delete(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response);
        int32_t do_default(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response);
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        HLX_SERVER_DISALLOW_COPY_AND_ASSIGN(default_http_request_handler)
};

//: ----------------------------------------------------------------------------
//: hlx_server
//: ----------------------------------------------------------------------------
class hlx_server
{
public:
        //: --------------------------------------------------------------------
        //: Types
        //: --------------------------------------------------------------------
        typedef std::list <t_server *> t_server_list_t;
        typedef std::map<uint16_t, uint32_t > status_code_count_map_t;

        // -----------------------------------------------
        // All Stat Aggregation..
        // -----------------------------------------------
        typedef struct t_stat_struct
        {
                // Server stats
                uint64_t m_num_conn_started;
                uint64_t m_num_conn_completed;
                uint64_t m_num_reqs;
                uint64_t m_num_idle_killed;
                uint64_t m_num_errors;
                uint64_t m_num_bytes_read;
                uint64_t m_num_bytes_written;
                uint32_t m_cur_conn_count;
                status_code_count_map_t m_status_code_count_map;

                t_stat_struct():
                        m_num_conn_started(0),
                        m_num_conn_completed(0),
                        m_num_reqs(0),
                        m_num_idle_killed(0),
                        m_num_errors(0),
                        m_num_bytes_read(0),
                        m_num_bytes_written(0),
                        m_cur_conn_count(0),
                        m_status_code_count_map()
                {}
                void clear();
        } t_stat_t;

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
        void set_scheme(scheme_type_t a_scheme);
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

        // Stats
        void get_stats(t_stat_t &ao_all_stats);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        HLX_SERVER_DISALLOW_COPY_AND_ASSIGN(hlx_server)
        int init(void);
        int init_server_list(void);
        void add_to_total_stat_agg(t_stat_t &ao_stat_agg, const t_stat_t &a_add_total_stat);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        bool m_verbose;
        bool m_color;
        bool m_stats;
        uint16_t m_port;
        uint32_t m_num_threads;
        int32_t m_num_parallel;
        scheme_type_t m_scheme;
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
        url_router *m_url_router;
        int32_t m_fd;
        bool m_is_initd;
};


} //namespace ns_hlx {

#endif


