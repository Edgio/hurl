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
#include "parsed_url.h"

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


//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------
typedef enum conn_state
{
        CONN_STATE_FREE = 0,
        CONN_STATE_CONNECTING,
        CONN_STATE_CONNECTED,
        CONN_STATE_SSL_CONNECTING,
        CONN_STATE_SSL_CONNECTED,
        CONN_STATE_READING,
        CONN_STATE_DONE
} conn_state_t;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class nconn
{
public:
        nconn(bool a_verbose,
              bool a_color,
              uint32_t a_sock_opt_recv_buf_size,
              uint32_t a_sock_opt_send_buf_size,
              bool a_sock_opt_no_delay,
              bool a_save_response_in_reqlet,
              int64_t a_max_reqs_per_conn = -1,
              void *a_rand_ptr = NULL):
                m_req_buf_len(0),
                m_timer_obj(NULL),
                m_fd(-1),
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
                m_ssl_ctx(NULL),
                m_rand_ptr(a_rand_ptr),
                m_scheme(URL_SCHEME_HTTP),
                m_host("NA")
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
                stat_init(m_stat);

                pthread_mutex_init(&m_mutex, NULL);

        };

        int32_t do_connect(host_info_t &a_host_info, const std::string &a_host);
        int32_t connect_cb(host_info_t &a_host_info);
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
        void set_scheme(url_scheme_t a_scheme) {m_scheme = a_scheme;};
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

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(nconn)

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
public:
        int m_fd;
private:
        SSL *m_ssl;
        conn_state_t m_state;
        req_stat_t m_stat;
        uint64_t m_id;
        void *m_data1;
        bool m_save_response_in_reqlet;

        // TODO Testing...
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

        SSL_CTX * m_ssl_ctx;
        void *m_rand_ptr;
        url_scheme_t m_scheme;
        std::string m_host;

};

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
SSL_CTX* nconn_ssl_init(const std::string &a_cipher_list);
void nconn_kill_locks(void);


#endif



