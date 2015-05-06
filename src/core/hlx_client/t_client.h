//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    t_client.h
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
#ifndef _T_CLIENT_H
#define _T_CLIENT_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "reqlet.h"
#include "nconn_ssl.h"
#include "nconn_tcp.h"
#include "hlx_client.h"
#include "ndebug.h"

// signal
#include <signal.h>

#include <list>
#include <unordered_set>
#include <vector>

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
//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::vector<nconn *> nconn_vector_t;
typedef std::list<uint32_t> conn_id_list_t;
typedef std::unordered_set<uint32_t> conn_id_set_t;

//: ----------------------------------------------------------------------------
//: Settings
//: ----------------------------------------------------------------------------
typedef struct settings_struct
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
        t_client_list_t m_t_client_list;
        evr_loop_type_t m_evr_loop_type;
        int32_t m_num_parallel;
        uint32_t m_num_threads;
        uint32_t m_timeout_s;
        int32_t m_run_time_s;
        int32_t m_rate;
        request_mode_t m_request_mode;
        int32_t m_num_end_fetches;
        bool m_connect_only;
        int32_t m_num_reqs_per_conn;
        bool m_save_response;
        bool m_collect_stats;

        // tcp options
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;

        // SSL options
        SSL_CTX* m_ssl_ctx;
        std::string m_ssl_cipher_list;
        std::string m_ssl_options_str;
        long m_ssl_options;
        bool m_ssl_verify;
        bool m_ssl_sni;
        std::string m_ssl_ca_file;
        std::string m_ssl_ca_path;

        // resolver
        resolver *m_resolver;


        // ---------------------------------
        // Defaults...
        // ---------------------------------
        settings_struct() :
                m_verbose(false),
                m_color(false),
                m_quiet(false),
                m_show_summary(false),
                m_url(),
                m_header_map(),
                m_verb("GET"),
                m_req_body(NULL),
                m_req_body_len(0),
                m_t_client_list(),
                m_evr_loop_type(EVR_LOOP_EPOLL),
                m_num_parallel(64),
                m_num_threads(8),
                m_timeout_s(10),
                m_run_time_s(-1),
                m_rate(-1),
                m_request_mode(REQUEST_MODE_ROUND_ROBIN),
                m_num_end_fetches(-1),
                m_connect_only(false),
                m_num_reqs_per_conn(-1),
                m_save_response(false),
                m_collect_stats(false),
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
                m_resolver(NULL)
        {}

private:
        DISALLOW_COPY_AND_ASSIGN(settings_struct);

} settings_struct_t;

//: ----------------------------------------------------------------------------
//: t_client
//: ----------------------------------------------------------------------------
class t_client
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        t_client(const settings_struct_t &a_settings,
                 reqlet_vector_t a_reqlet_vector);

        ~t_client();

        int run(void);
        void *t_run(void *a_nothing);
        void stop(void);
        bool is_running(void) { return !m_stopped; }
        int32_t set_header(const std::string &a_header_key, const std::string &a_header_val);
        void set_ssl_ctx(SSL_CTX * a_ssl_ctx) { m_settings.m_ssl_ctx = a_ssl_ctx;};
        uint32_t get_timeout_s(void) { return m_settings.m_timeout_s;};
        void get_stats_copy(tag_stat_map_t &ao_tag_stat_map);
        void set_end_fetches(int32_t a_num_fetches) { m_num_fetches = a_num_fetches;}
        bool is_done(void) const
        {
                if(m_num_fetches != -1) return (m_num_fetched >= m_num_fetches);
                else return false;
        }
        bool is_pending_done(void) const
        {
                if(m_num_fetches != -1) return ((m_num_fetched + m_num_pending) >= m_num_fetches);
                else return false;
        }

        int32_t append_summary(reqlet *a_reqlet);
        const reqlet_vector_t &get_reqlet_vector(void) {return m_reqlet_vector;};

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // Needs to be public for now -to join externally
        pthread_t m_t_run_thread;
        settings_struct_t m_settings;

        // Reqlets
        uint32_t m_num_reqlets;
        uint32_t m_num_get;
        uint32_t m_num_done;
        uint32_t m_num_resolved;
        uint32_t m_num_error;

        // -----------------------------
        // Summary info
        // -----------------------------
        // Connectivity
        uint32_t m_summary_success;
        uint32_t m_summary_error_addr;
        uint32_t m_summary_error_conn;
        uint32_t m_summary_error_unknown;

        // SSL info
        uint32_t m_summary_ssl_error_self_signed;
        uint32_t m_summary_ssl_error_expired;
        uint32_t m_summary_ssl_error_other;

        summary_map_t m_summary_ssl_protocols;
        summary_map_t m_summary_ssl_ciphers;

        // -------------------------------------------------
        // Static (class) methods
        // -------------------------------------------------
        static void *evr_loop_file_writeable_cb(void *a_data);
        static void *evr_loop_file_readable_cb(void *a_data);
        static void *evr_loop_file_error_cb(void *a_data);
        static void *evr_loop_file_timeout_cb(void *a_data);
        static void *evr_loop_timer_cb(void *a_data);
        static void *evr_loop_timer_completion_cb(void *a_data);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(t_client)

        //Helper for pthreads
        static void *t_run_static(void *a_context)
        {
                return reinterpret_cast<t_client *>(a_context)->t_run(NULL);
        }

        int32_t start_connections(void);
        int32_t cleanup_connection(nconn *a_nconn, bool a_cancel_timer = true);
        int32_t create_request(nconn &ao_conn, reqlet &a_reqlet);
        nconn *create_new_nconn(uint32_t a_id, const reqlet &a_reqlet);
        reqlet *get_reqlet(void);
        reqlet *try_get_resolved(void);
        void limit_rate();

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        // client config

        sig_atomic_t m_stopped;

        nconn_vector_t m_nconn_vector;
        conn_id_list_t m_conn_free_list;
        conn_id_set_t m_conn_used_set;

        int64_t m_num_fetches;
        int64_t m_num_fetched;
        int64_t m_num_pending;

        int32_t m_start_time_s;

        //uint64_t m_unresolved_count;

        // Get evr_loop
        evr_loop *m_evr_loop;

        // For rate limiting
        uint64_t m_rate_delta_us;
        uint64_t m_last_get_req_us;

        // Reqlet vectors
        reqlet_vector_t m_reqlet_vector;
        uint32_t m_reqlet_vector_idx;

        void *m_rand_ptr;

};


} //namespace ns_hlx {

#endif // #ifndef _HLX_CLIENT_H
