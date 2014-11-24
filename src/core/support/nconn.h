//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn.h
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
#ifndef _NCONN_H
#define _NCONN_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "ndebug.h"
#include "http_parser.h"
#include "host_info.h"
#include "req_stat.h"

// OpenSSL
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <pthread.h>


//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define MAX_READ_BUF (16*1024)
#define MAX_REQ_BUF (2048)


//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class evr_loop;
class parsed_url;

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class nconn
{
public:

        // ---------------------------------------
        // Protocol
        // ---------------------------------------
        typedef enum scheme_enum {

                SCHEME_HTTP = 0,
                SCHEME_HTTPS

        } scheme_t;

        // ---------------------------------------
        // Connection state
        // ---------------------------------------
        typedef enum conn_state
        {
                CONN_STATE_FREE = 0,
                CONN_STATE_CONNECTING,

                // SSL
                CONN_STATE_SSL_CONNECTING,
                CONN_STATE_SSL_CONNECTING_WANT_READ,
                CONN_STATE_SSL_CONNECTING_WANT_WRITE,

                CONN_STATE_CONNECTED,
                CONN_STATE_READING,
                CONN_STATE_DONE
        } conn_state_t;

        nconn(bool a_verbose,
              bool a_color,
              uint32_t a_sock_opt_recv_buf_size,
              uint32_t a_sock_opt_send_buf_size,
              bool a_sock_opt_no_delay,
              bool a_save_response_in_reqlet,
              bool a_collect_stats,
              uint32_t a_timeout_s,
              int64_t a_max_reqs_per_conn = -1,
              void *a_rand_ptr = NULL):
                m_req_buf_len(0),
                m_timer_obj(NULL),
                m_fd(-1),

                // ssl
                m_ssl_ctx(NULL),
                m_ssl(NULL),

                m_state(CONN_STATE_FREE),
                m_stat(),
                m_id(0),
                m_data1(NULL),
                m_save_response_in_reqlet(a_save_response_in_reqlet),
                m_http_parser_settings(),
                m_http_parser(),
                m_server_response_supports_keep_alives(false),
                m_mutex(),
                m_verbose(a_verbose),
                m_color(a_color),
                m_sock_opt_recv_buf_size(a_sock_opt_recv_buf_size),
                m_sock_opt_send_buf_size(a_sock_opt_send_buf_size),
                m_sock_opt_no_delay(a_sock_opt_no_delay),
                m_read_buf_idx(0),
                m_max_reqs_per_conn(a_max_reqs_per_conn),
                m_num_reqs(0),
                m_connect_start_time_us(0),
                m_request_start_time_us(0),
                m_last_connect_time_us(0),
                m_scheme(SCHEME_HTTP),
                m_host("NA"),
                m_collect_stats_flag(a_collect_stats),
                m_timeout_s(a_timeout_s)
        {
                // Set up callbacks...
                m_http_parser_settings.on_message_begin = hp_on_message_begin;
                m_http_parser_settings.on_url = hp_on_url;
                m_http_parser_settings.on_status = hp_on_status;
                m_http_parser_settings.on_header_field = hp_on_header_field;
                m_http_parser_settings.on_header_value = hp_on_header_value;
                m_http_parser_settings.on_headers_complete = hp_on_headers_complete;
                m_http_parser_settings.on_body = hp_on_body;
                m_http_parser_settings.on_message_complete = hp_on_message_complete;

                // Set stats
                if(m_collect_stats_flag)
                {
                        stat_init(m_stat);
                }

                pthread_mutex_init(&m_mutex, NULL);

        };

        void set_host(const std::string &a_host) {m_host = a_host;};
        int32_t run_state_machine(evr_loop *a_evr_loop, const host_info_t &a_host_info);
        int32_t send_request(bool is_reuse = false);
        int32_t read_cb(void);
        int32_t done_cb(void);
        bool can_reuse(void)
        {
                //NDBG_PRINT("CONN[%u] num / max %ld / %ld \n", m_connection_id, m_num_reqs, m_max_reqs_per_conn);
                if(m_server_response_supports_keep_alives &&
                   ((m_max_reqs_per_conn == -1) || (m_num_reqs < m_max_reqs_per_conn)))
                {
                        return true;
                }
                else
                {
                        return false;
                }
        }
        void set_ssl_ctx(SSL_CTX * a_ssl_ctx) { m_ssl_ctx = a_ssl_ctx;};
        void reset_stats(void);
        const req_stat_t &get_stats(void) const { return m_stat;};
        void set_scheme(scheme_t a_scheme) {m_scheme = a_scheme;};
        int32_t take_lock(void)
        {
                return 0;
                //return pthread_mutex_lock(&m_mutex);
        }
        int32_t try_lock(void) {
                return 0;
                //return pthread_mutex_trylock(&m_mutex);
        }
        int32_t give_lock(void) {
                return 0;
                //return pthread_mutex_unlock(&m_mutex);
        }
        conn_state_t get_state(void) { return m_state;}
        int32_t get_fd(void) { return m_fd; }
        void set_id(uint64_t a_id) {m_id = a_id;}
        uint64_t get_id(void) {return m_id;}

        void set_data1(void * a_data) {m_data1 = a_data;}
        void *get_data1(void) {return m_data1;}

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------
        static int hp_on_message_begin(http_parser* a_parser);
        static int hp_on_url(http_parser* a_parser, const char *a_at, size_t a_length);
        static int hp_on_status(http_parser* a_parser, const char *a_at, size_t a_length);
        static int hp_on_header_field(http_parser* a_parser, const char *a_at, size_t a_length);
        static int hp_on_header_value(http_parser* a_parser, const char *a_at, size_t a_length);
        static int hp_on_headers_complete(http_parser* a_parser);
        static int hp_on_body(http_parser* a_parser, const char *a_at, size_t a_length);
        static int hp_on_message_complete(http_parser* a_parser);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        char m_req_buf[MAX_REQ_BUF];
        uint32_t m_req_buf_len;
        void *m_timer_obj;
        int m_fd;


private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(nconn)

        int32_t setup_socket(const host_info_t &a_host_info);
        int32_t ssl_connect_cb(const host_info_t &a_host_info);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        // ssl
        SSL_CTX * m_ssl_ctx;
        SSL *m_ssl;

        conn_state_t m_state;
        req_stat_t m_stat;
        uint64_t m_id;
        void *m_data1;
        bool m_save_response_in_reqlet;

        http_parser_settings m_http_parser_settings;
        http_parser m_http_parser;
        bool m_server_response_supports_keep_alives;
        pthread_mutex_t m_mutex;

        bool m_verbose;
        bool m_color;

        // Socket options
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;

        char m_read_buf[MAX_READ_BUF];
        uint32_t m_read_buf_idx;

        int64_t m_max_reqs_per_conn;
        int64_t m_num_reqs;

        uint64_t m_connect_start_time_us;
        uint64_t m_request_start_time_us;
        uint64_t m_last_connect_time_us;

        scheme_t m_scheme;
        std::string m_host;
        bool m_collect_stats_flag;
        uint32_t m_timeout_s;

};

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
SSL_CTX* nconn_ssl_init(const std::string &a_cipher_list);
void nconn_kill_locks(void);


#endif



