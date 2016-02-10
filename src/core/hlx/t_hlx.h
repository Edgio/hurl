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
#include "cb.h"
#include "hconn.h"
#include "obj_pool.h"
#include "t_conf.h"
#include "hlx/hlx.h"
#include "hlx/stat.h"
#include "nresolver.h"

#include <queue>

// for sig_atomic_t
#include <signal.h>

//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------
namespace ns_hlx {

typedef obj_pool <hconn> hconn_pool_t;
typedef obj_pool <resp> resp_pool_t;
typedef obj_pool <rqst> rqst_pool_t;
typedef obj_pool <nbq> nbq_pool_t;
class url_router;
struct host_info;

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
        // Public methods
        // -------------------------------------------------
        t_hlx(const t_conf *a_t_conf);
        ~t_hlx();
        int32_t init(void);
        int run(void);
        void *t_run(void *a_nothing);
        void stop(void);
        bool is_running(void) { return !m_stopped; }
        uint32_t get_timeout_ms(void) { return m_t_conf->m_timeout_ms;};
        hlx *get_hlx(void) { if(!m_t_conf) return NULL;  return m_t_conf->m_hlx;}
        const t_stat_t &get_stat(void) { return m_stat;}
        int32_t add_lsnr(lsnr &a_lsnr);
        int32_t subr_add(subr &a_subr);
        int32_t queue_output(hconn &a_hconn);
        int32_t queue_api_resp(api_resp &a_api_resp, hconn &a_hconn);
        void add_stat_to_agg(const req_stat_t &a_req_stat, uint16_t a_status_code);
        int32_t add_timer(uint32_t a_time_ms, timer_cb_t a_timer_cb, void *a_data, void **ao_timer);
        int32_t cancel_timer(void *a_timer);
        void signal(void);
        int32_t cleanup_hconn(hconn &a_hconn);
        void release_resp(resp *a_resp);
        void release_nbq(nbq *a_nbq);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // Needs to be public for now -to join externally
        pthread_t m_t_run_thread;
        // TODO make private
        t_stat_t m_stat;

        // -------------------------------------------------
        // Public Static (class) methods
        // -------------------------------------------------
        static int32_t evr_file_writeable_cb(void *a_data);
        static int32_t evr_file_readable_cb(void *a_data);
        static int32_t evr_file_error_cb(void *a_data);
        static int32_t evr_file_timeout_cb(void *a_ctx, void *a_data);
        static int32_t evr_timer_cb(void *a_ctx, void *a_data);

        // Resolver callback
#ifdef ASYNC_DNS_SUPPORT
        static int32_t async_dns_resolved_cb(const host_info *a_host_info, void *a_data);
#endif

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

        // Get new client connection
        nconn *get_new_client_conn(int a_fd, scheme_t a_scheme, url_router *a_url_router);
        int32_t config_conn(nconn &a_nconn,
                            url_router *a_url_router,
                            hconn_type_t a_type,
                            bool a_save,
                            bool a_connect_only);

        hconn * get_hconn(url_router *a_url_router,
                            hconn_type_t a_type,
                            bool a_save);
        int32_t subr_try_start(subr &a_subr);
        int32_t subr_start(subr &a_subr, hconn &a_hconn, nconn &a_nconn);
        int32_t subr_try_deq(void);

        inline void subr_dequeue(void)
        {
                m_subr_list.pop_front();
                --m_subr_list_size;
        }
        inline void subr_enqueue(subr &a_subr)
        {
                m_subr_list.push_back(&a_subr);
                ++m_subr_list_size;
        }
        inline uint64_t subr_queue_size(void)
        {
                return m_subr_list_size;
        }

        int32_t handle_listen_ev(hconn &a_hconn, nconn &a_nconn);
#ifdef ASYNC_DNS_SUPPORT
        int32_t async_dns_init(void);
        int32_t async_dns_handle_ev(void);
        int32_t async_dns_lookup(const std::string &a_host, uint16_t a_port, subr *a_subr);
#endif

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        const t_conf *m_t_conf;
        nconn_pool m_nconn_pool;
        nconn_pool m_nconn_proxy_pool;
        sig_atomic_t m_stopped;
        int32_t m_start_time_s;
        evr_loop *m_evr_loop;
        scheme_t m_scheme;
        listening_nconn_list_t m_listening_nconn_list;
        subr_list_t m_subr_list;
        uint64_t m_subr_list_size;

        hconn_pool_t m_hconn_pool;
        resp_pool_t m_resp_pool;
        rqst_pool_t m_rqst_pool;
        nbq_pool_t m_nbq_pool;

#ifdef ASYNC_DNS_SUPPORT
        bool m_async_dns_is_initd;
        void *m_async_dns_ctx;
        int m_async_dns_fd;
        nconn *m_async_dns_nconn;
        evr_timer_event_struct *m_async_dns_timer_obj;
        nresolver::lookup_job_q_t m_lookup_job_q;
        nresolver::lookup_job_pq_t m_lookup_job_pq;
#endif

        // is initialized flag
        bool m_is_initd;

};

} //namespace ns_hlx {

#endif // #ifndef _T_HTTP



