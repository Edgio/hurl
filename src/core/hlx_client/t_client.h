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
#include "nconn_pool.h"
#include "settings.h"
#include "ndebug.h"
#include "http_rx.h"

// signal
#include <signal.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::vector<http_rx *> http_rx_vector_t;

//: ----------------------------------------------------------------------------
//: t_client
//: ----------------------------------------------------------------------------
class t_client
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        t_client(const settings_struct_t &a_settings);
        ~t_client();

        int run(void);
        void *t_run(void *a_nothing);
        void stop(void);
        bool is_running(void) { return !m_stopped; };

        int32_t set_http_requests(http_rx_vector_t a_http_rx_vector);

        int32_t set_header(const std::string &a_header_key, const std::string &a_header_val);
        void clear_headers(void) { m_settings.m_header_map.clear(); };
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
                if(m_num_fetches != -1) return ((m_num_fetched + m_nconn_pool.num_in_use()) >= m_num_fetches);
                else return false;
        }

        int32_t append_summary(http_rx *a_http_rx);
        const http_rx_vector_t &get_rx_vector(void) {return m_http_rx_vector;};

        void reset(void);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // Needs to be public for now -to join externally
        pthread_t m_t_run_thread;
        settings_struct_t m_settings;

        // Reqlets
        uint32_t m_num_resolved;
        uint32_t m_num_done;
        uint32_t m_num_error;

        // Summary info
        summary_info_t m_summary_info;

        // -------------------------------------------------
        // Static (class) methods
        // -------------------------------------------------
        static int32_t evr_loop_file_writeable_cb(void *a_data);
        static int32_t evr_loop_file_readable_cb(void *a_data);
        static int32_t evr_loop_file_error_cb(void *a_data);
        static int32_t evr_loop_file_timeout_cb(void *a_data);
        static int32_t evr_loop_timer_cb(void *a_data);
        static int32_t evr_loop_timer_completion_cb(void *a_data);

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

        int32_t request(http_rx *a_http_rx, nconn *a_nconn = NULL);
        int32_t start_connections(void);
        int32_t cleanup_connection(nconn *a_nconn, bool a_cancel_timer = true, int32_t a_status = 0);
        int32_t create_request(nbq &a_q, http_rx &a_http_rx);
        http_rx *get_rx(void);
        void limit_rate();

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        // client config
        nconn_pool m_nconn_pool;

        sig_atomic_t m_stopped;
        int64_t m_num_fetches;
        int64_t m_num_fetched;
        int32_t m_start_time_s;

        //uint64_t m_unresolved_count;

        // Get evr_loop
        evr_loop *m_evr_loop;

        // For rate limiting
        uint64_t m_rate_delta_us;
        uint64_t m_last_get_req_us;

        // Reqlet vectors
        http_rx_vector_t m_http_rx_vector;
        uint32_t m_http_rx_vector_idx;

        void *m_rand_ptr;

};


} //namespace ns_hlx {

#endif // #ifndef _T_CLIENT_H
