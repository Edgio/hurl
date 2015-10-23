//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    t_hlx.h
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
#ifndef _T_HLX_H
#define _T_HLX_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "nconn_pool.h"
#include "ndebug.h"
#include "evr.h"
#include "http_cb.h"
#include "http_data.h"
#include "obj_pool.h"
#include "t_hlx_conf.h"
#include "hlx/hlx.h"

// signal
#include <signal.h>

//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------
namespace ns_hlx {

typedef obj_pool <http_data_t> http_data_pool_t;
class url_router;

//: ----------------------------------------------------------------------------
//: t_hlx
//: ----------------------------------------------------------------------------
class t_hlx
{
public:
        // -------------------------------------------------
        // Types
        // -------------------------------------------------
        typedef std::list <nconn *> listening_nconn_list_t;

        // -------------------------------------------------
        // const
        // -------------------------------------------------
        // Subreq support
        static const uint32_t sc_subreq_q_conn_idx = 0xFFFFFFFE;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        t_hlx(const t_hlx_conf_t &a_t_hlx_conf);
        ~t_hlx();
        int run(void);
        void *t_run(void *a_nothing);
        void stop(void);
        bool is_running(void) { return !m_stopped; }
        uint32_t get_timeout_s(void) { return m_conf.m_timeout_s;};
        void get_stats_copy(t_stat_t &ao_stat);
        int32_t add_listener(listener *a_listener);
        int32_t add_subreq(subreq &a_subreq);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // Needs to be public for now -to join externally
        pthread_t m_t_run_thread;
        t_hlx_conf_t m_conf;

        // TODO make private???
        summary_info_t m_summary_info;

        // -------------------------------------------------
        // Public Static (class) methods
        // -------------------------------------------------
        static int32_t evr_loop_file_writeable_cb(void *a_data);
        static int32_t evr_loop_file_readable_cb(void *a_data);
        static int32_t evr_loop_file_error_cb(void *a_data);
        static int32_t evr_loop_file_timeout_cb(void *a_data);
        static int32_t evr_loop_timer_cb(void *a_data);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(t_hlx)

        //Helper for pthreads
        static void *t_run_static(void *a_context)
        {
                return reinterpret_cast<t_hlx *>(a_context)->t_run(NULL);
        }

        int32_t cleanup_connection(nconn *a_nconn, evr_timer_event_t *a_timer_obj, http_data_type_t a_type);
        int32_t handle_req(nconn &a_nconn, url_router *a_url_router, http_req &a_req, http_resp &ao_resp);

        // Initialize
        int32_t init(void);

        // Get new client connection
        nconn *get_new_client_conn(int a_fd, scheme_t a_scheme, url_router *a_url_router);
        int32_t config_conn(nconn *a_nconn,
                            url_router *a_url_router,
                            http_data_type_t a_type,
                            bool a_save,
                            bool a_connect_only);
        int32_t request(subreq &a_subreq, nconn *a_nconn = NULL);
        int32_t try_deq_subreq(void);
        void add_stat_to_agg(const req_stat_t &a_req_stat, uint16_t a_status_code);
        int32_t append_summary(nconn *a_nconn, http_resp *a_resp);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        nconn_pool m_nconn_pool;
        nconn_pool m_nconn_proxy_pool;
        sig_atomic_t m_stopped;
        int32_t m_start_time_s;
        evr_loop *m_evr_loop;
        scheme_t m_scheme;
        listening_nconn_list_t m_listening_nconn_list;
        subreq_queue_t m_subreq_queue;

        default_http_request_handler m_default_handler;
        http_data_pool_t m_http_data_pool;
        t_stat_t m_stat;

        // Subrequest support
        int m_subreq_q_fd;
        nconn *m_subreq_q_nconn;

        // is initialized flag
        bool m_is_initd;
};

} //namespace ns_hlx {

#endif // #ifndef _T_HTTP



