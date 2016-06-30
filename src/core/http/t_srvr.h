//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    t_srvr.h
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
#ifndef _T_SRVR_H
#define _T_SRVR_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/resp.h"
#include "hlx/rqst.h"
#include "hlx/subr.h"

#include "nconn_pool.h"
#include "evr.h"
#include "obj_pool.h"
#include "hlx/srvr.h"
#include "hlx/stat.h"
#include "nresolver.h"

#include <queue>

// for sig_atomic_t
#include <signal.h>
#include "cb.h"
#include "clnt_session.h"
#include "ups_srvr_session.h"

//: ----------------------------------------------------------------------------
//: External Fwd decl's
//: ----------------------------------------------------------------------------
typedef struct ssl_ctx_st SSL_CTX;

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------
typedef obj_pool <clnt_session> clnt_session_pool_t;
typedef obj_pool <ups_srvr_session> ups_srvr_session_pool_t;
typedef obj_pool <resp> resp_pool_t;
typedef obj_pool <rqst> rqst_pool_t;
typedef obj_pool <nbq> nbq_pool_t;
class url_router;
struct host_info;
class nbq;

//: ----------------------------------------------------------------------------
//: Virtual Server conf
//: TODO Allow many t_srvr conf's
//: one per listener
//: ----------------------------------------------------------------------------
typedef struct t_conf
{
        bool m_rqst_resp_logging;
        bool m_rqst_resp_logging_color;
        evr_loop_type_t m_evr_loop_type;
        int32_t m_num_parallel;
        uint32_t m_timeout_ms;
        int32_t m_num_reqs_per_conn;
        bool m_collect_stats;
        bool m_count_response_status;
        uint32_t m_update_stats_ms;
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;
        resp_done_cb_t m_resp_done_cb;
        srvr *m_srvr;

        // TLS Config
        // Server ctx
        SSL_CTX* m_tls_server_ctx;
        std::string m_tls_server_ctx_key;
        std::string m_tls_server_ctx_crt;
        std::string m_tls_server_ctx_cipher_list;
        std::string m_tls_server_ctx_options_str;
        long m_tls_server_ctx_options;

        // client ctx
        SSL_CTX *m_tls_client_ctx;
        std::string m_tls_client_ctx_cipher_list;
        std::string m_tls_client_ctx_options_str;
        long m_tls_client_ctx_options;
        std::string m_tls_client_ctx_ca_file;
        std::string m_tls_client_ctx_ca_path;

        // ---------------------------------
        // Defaults...
        // ---------------------------------
        t_conf():
                m_rqst_resp_logging(false),
                m_rqst_resp_logging_color(false),
#if defined(__linux__)
                m_evr_loop_type(EVR_LOOP_EPOLL),
#elif defined(__FreeBSD__) || defined(__APPLE__)
                m_evr_loop_type(EVR_LOOP_SELECT),
#else
                m_evr_loop_type(EVR_LOOP_SELECT),
#endif
                m_num_parallel(1024),
                m_timeout_ms(10000),
                m_num_reqs_per_conn(-1),
                m_collect_stats(false),
                m_count_response_status(false),
                m_update_stats_ms(0),
                m_sock_opt_recv_buf_size(0),
                m_sock_opt_send_buf_size(0),
                m_sock_opt_no_delay(false),
                m_resp_done_cb(NULL),
                m_srvr(NULL),
                m_tls_server_ctx(NULL),
                m_tls_server_ctx_key(),
                m_tls_server_ctx_crt(),
                m_tls_server_ctx_cipher_list(),
                m_tls_server_ctx_options_str(),
                m_tls_server_ctx_options(0),
                m_tls_client_ctx(NULL),
                m_tls_client_ctx_cipher_list(),
                m_tls_client_ctx_options_str(),
                m_tls_client_ctx_options(0),
                m_tls_client_ctx_ca_file(),
                m_tls_client_ctx_ca_path()
        {}

private:
        // Disallow copy/assign
        t_conf& operator=(const t_conf &);
        t_conf(const t_conf &);
} conf_t;

//: ----------------------------------------------------------------------------
//: t_srvr
//: ----------------------------------------------------------------------------
class t_srvr
{
public:
        // -------------------------------------------------
        // Types
        // -------------------------------------------------
        typedef std::list <nconn *> listening_nconn_list_t;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        t_srvr(const t_conf *a_t_conf);
        ~t_srvr();
        int32_t init(void);
        int run(void);
        void *t_run(void *a_nothing);
        void stop(void);
        bool is_running(void) { return !m_stopped; }
        uint32_t get_timeout_ms(void) { return m_t_conf->m_timeout_ms;};
        srvr *get_srvr(void);
        nbq *get_nbq(void);
        int32_t add_lsnr(lsnr &a_lsnr);
        int32_t subr_add(subr &a_subr);
        int32_t queue_output(clnt_session &a_clnt_session);
        int32_t queue_api_resp(api_resp &a_api_resp, clnt_session &a_clnt_session);
        void add_stat_to_agg(const conn_stat_t &a_conn_stat, uint16_t a_status_code);
        int32_t add_timer(uint32_t a_time_ms, timer_cb_t a_timer_cb, void *a_data, void **ao_timer);
        int32_t cancel_timer(void *a_timer);
        void signal(void);
        int32_t cleanup_clnt_session(clnt_session *a_clnt_session, nconn *a_nconn);
        int32_t cleanup_srvr_session(ups_srvr_session *a_ups_srvr_session, nconn *a_nconn);
        void release_resp(resp *a_resp);
        void release_nbq(nbq *a_nbq);

        int32_t get_stat(t_stat_cntr_t &ao_stat);
        int32_t get_ups_status_code_count_map(status_code_count_map_t &ao_ups_status_code_count_map);

        // TODO Consider removal...
        // Stats helpers
        void bump_num_clnt_reqs(void) { ++m_stat.m_clnt_reqs;}
        void bump_num_clnt_idle_killed(void) { ++m_stat.m_clnt_idle_killed;}
        void bump_num_upsv_idle_killed(void) { ++m_stat.m_upsv_idle_killed;}

        // TODO Move to lsnr readable...
        static int32_t evr_fd_readable_lsnr_cb(void *a_data);

        evr_loop *get_evr_loop(void) {return m_evr_loop;}

        void *dequeue_clnt_session_writeable(void)
        {
                void *l_retval = NULL;
                if(m_clnt_session_writeable_data)
                {
                        l_retval = m_clnt_session_writeable_data;
                        m_clnt_session_writeable_data = NULL;
                }
                return l_retval;
        }

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // Needs to be public for now -to join externally
        pthread_t m_t_run_thread;

        // TODO hide -or prefer getters
        // Orphan q's
        nbq *m_orphan_in_q;
        nbq *m_orphan_out_q;

        // TODO hide -or prefer getters
        t_stat_cntr_t m_stat;
        status_code_count_map_t m_upsv_status_code_count_map;

        // TODO hide -or prefer getters
        nconn_pool m_nconn_pool;
        nconn_pool m_nconn_proxy_pool;
        clnt_session_pool_t m_clnt_session_pool;
        ups_srvr_session_pool_t m_ups_srvr_session_pool;

        // Resolver callback
#ifdef ASYNC_DNS_SUPPORT
        static int32_t adns_resolved_cb(const host_info *a_host_info, void *a_data);
#endif

        // Async stat update
        static int32_t s_stat_update(void *a_ctx, void *a_data);
        void stat_update(void);

private:
        // -------------------------------------------------
        // const
        // -------------------------------------------------
        static const uint32_t S_POOL_ID_UPS_PROXY = 0xDEAD0001;
        static const uint32_t S_POOL_ID_CLIENT    = 0xDEAD0002;

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        t_srvr& operator=(const t_srvr &);
        t_srvr(const t_srvr &);

        //Helper for pthreads
        static void *t_run_static(void *a_context)
        {
                return reinterpret_cast<t_srvr *>(a_context)->t_run(NULL);
        }

        // Get new client connection
        nconn *get_new_client_conn(scheme_t a_scheme, lsnr *a_lsnr);
        clnt_session * get_clnt(lsnr *a_lsnr);
        ups_srvr_session * get_ups_srvr(lsnr *a_lsnr);

        int32_t subr_try_start(subr &a_subr);
        int32_t subr_start(subr &a_subr, ups_srvr_session &a_ups_srvr_session, nconn &a_nconn);
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

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        const t_conf *m_t_conf;
        sig_atomic_t m_stopped;
        int32_t m_start_time_s;
        evr_loop *m_evr_loop;
        scheme_t m_scheme;
        listening_nconn_list_t m_listening_nconn_list;
        subr_list_t m_subr_list;
        uint64_t m_subr_list_size;

        // Pools
        nbq_pool_t m_nbq_pool;
        resp_pool_t m_resp_pool;
        rqst_pool_t m_rqst_pool;

#ifdef ASYNC_DNS_SUPPORT
        nresolver::adns_ctx *m_adns_ctx;
#endif

        // is initialized flag
        bool m_is_initd;

        // Stat (internal)
        t_stat_cntr_t m_stat_copy;
        status_code_count_map_t m_upsv_status_code_count_map_copy;
        pthread_mutex_t m_stat_copy_mutex;

        // Write after read upstream
        void *m_clnt_session_writeable_data;
};

} //namespace ns_hlx {

#endif // #ifndef _T_HTTP



