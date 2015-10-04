//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    t_server.h
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
#ifndef _T_SERVER_H
#define _T_SERVER_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlo/hlx_server.h"
#include "nconn_pool.h"
#include "server_settings.h"
#include "ndebug.h"
#include "evr.h"
#include "http_cb.h"
#include "obj_pool.h"

// signal
#include <signal.h>

// TODO multi-thread support
#if 0
// TODO Need extern here???
extern "C" {
#include <pthread_workqueue.h>
}
#endif

//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------
namespace ns_hlx {

// TODO multi-thread support
#if 0
struct work_struct;
typedef work_struct work_t;
#endif

typedef std::queue <nconn *> out_q_t;

//: ----------------------------------------------------------------------------
//: t_server
//: ----------------------------------------------------------------------------
class t_server
{
public:
        // -------------------------------------------------
        // Public const
        // -------------------------------------------------
        // TODO multi-thread support
#if 0
        static const uint32_t sc_work_q_conn_idx = 0xFFFFFFFF;
#endif

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        t_server(const server_settings_struct_t &a_settings, url_router *a_url_router);
        ~t_server();
        int run(void);
        void *t_run(void *a_nothing);
        void stop(void);
        bool is_running(void) { return !m_stopped; }
        uint32_t get_timeout_s(void) { return m_settings.m_timeout_s;};
        void get_stats_copy(hlx_server::t_stat_t &ao_stat);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // Needs to be public for now -to join externally
        pthread_t m_t_run_thread;
        server_settings_struct_t m_settings;
        url_router *m_url_router;
        out_q_t m_out_q;

        // -------------------------------------------------
        // Public Static (class) methods
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
        DISALLOW_COPY_AND_ASSIGN(t_server)

        //Helper for pthreads
        static void *t_run_static(void *a_context)
        {
                return reinterpret_cast<t_server *>(a_context)->t_run(NULL);
        }

        int32_t cleanup_connection(nconn *a_nconn, evr_timer_event_t *a_timer_obj);

        // TODO multi-thread support
#if 0
        // Work Queue
        int32_t add_work(nconn *a_nconn, http_req *a_req);
        static void do_work(void *a_work);
#endif
        int32_t handle_req(nconn *a_nconn, http_req *a_req);

        // Initialize
        int32_t init(void);

        // Get new client connection
        nconn *get_new_client_conn(int a_fd);
        int32_t reset_conn_input_q(http_data_t *a_data, nconn *a_nconn);
        int32_t config_conn(nconn *a_nconn);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        nconn_pool m_nconn_pool;
        sig_atomic_t m_stopped;
        int32_t m_start_time_s;
        evr_loop *m_evr_loop;
        nconn::scheme_t m_scheme;
        nconn *m_listening_nconn;
        nconn *m_out_q_nconn;
        int m_out_q_fd;
        default_http_request_handler m_default_handler;
        http_data_pool_t m_http_data_pool;
        hlx_server::t_stat_t m_stat;

        // TODO multi-thread support
#if 0
        pthread_workqueue_t m_work_q;
        pthread_workqueue_attr_t m_work_q_attr;
#endif

        bool m_is_initd;

};

} //namespace ns_hlx {

#endif // #ifndef _HLX_CLIENT_H
