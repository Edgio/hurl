//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    t_http.h
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
#ifndef _T_HTTP
#define _T_HTTP

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "nconn_pool.h"
#include "ndebug.h"
#include "evr.h"
#include "http_cb.h"
#include "obj_pool.h"
#include "hlo/hlx.h"
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
class url_router;

//: ----------------------------------------------------------------------------
//: Virtual Server conf
//: TODO Allow many vsconf's
//: one per listener
//: ----------------------------------------------------------------------------
typedef struct vsconf
{
        bool m_verbose;
        bool m_color;
        bool m_quiet;
        bool m_show_summary;
        evr_loop_type_t m_evr_loop_type;
        int32_t m_num_parallel;
        uint32_t m_timeout_s;
        int32_t m_num_reqs_per_conn;
        bool m_collect_stats;
        bool m_use_persistent_pool;
        bool m_stop_on_empty;
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;
        SSL_CTX* m_ssl_server_ctx;
        std::string m_ssl_cipher_list;
        std::string m_ssl_options_str;
        long m_ssl_options;
        bool m_ssl_verify;
        bool m_ssl_sni;
        std::string m_ssl_ca_file;
        std::string m_ssl_ca_path;
        std::string m_tls_key;
        std::string m_tls_crt;
        SSL_CTX* m_ssl_client_ctx;
        resolver *m_resolver;

        // ---------------------------------
        // Defaults...
        // ---------------------------------
        vsconf() :
                m_verbose(false),
                m_color(false),
                m_quiet(false),
                m_show_summary(false),
                m_evr_loop_type(EVR_LOOP_EPOLL),
                m_num_parallel(1024),
                m_timeout_s(10),
                m_num_reqs_per_conn(-1),
                m_collect_stats(false),
                m_use_persistent_pool(false),
                m_stop_on_empty(false),
                m_sock_opt_recv_buf_size(0),
                m_sock_opt_send_buf_size(0),
                m_sock_opt_no_delay(false),
                m_ssl_server_ctx(NULL),
                m_ssl_cipher_list(""),
                m_ssl_options_str(""),
                m_ssl_options(0),
                m_ssl_verify(false),
                m_ssl_sni(false),
                m_ssl_ca_file(""),
                m_ssl_ca_path(""),
                m_tls_key(""),
                m_tls_crt(""),
                m_ssl_client_ctx(NULL),
                m_resolver(NULL)
        {}

private:
        DISALLOW_COPY_AND_ASSIGN(vsconf);

} vsconf_t;

//: ----------------------------------------------------------------------------
//: t_http
//: ----------------------------------------------------------------------------
class t_http
{
public:
        // -------------------------------------------------
        // Types
        // -------------------------------------------------
        typedef std::list <nconn *> listening_nconn_list_t;

        // -------------------------------------------------
        // const
        // -------------------------------------------------
        // TODO multi-thread support
#if 0
        static const uint32_t sc_work_q_conn_idx = 0xFFFFFFFF;
#endif
        // Subreq support
        static const uint32_t sc_subreq_q_conn_idx = 0xFFFFFFFE;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        t_http(const vsconf_t &a_vsconf);
        ~t_http();
        int run(void);
        void *t_run(void *a_nothing);
        void stop(void);
        bool is_running(void) { return !m_stopped; }
        uint32_t get_timeout_s(void) { return m_vsconf.m_timeout_s;};
        void get_stats_copy(t_stat_t &ao_stat);
        int32_t add_listener(listener *a_listener);
        int32_t add_subreq(subreq *a_subreq);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // Needs to be public for now -to join externally
        pthread_t m_t_run_thread;
        vsconf_t m_vsconf;
        // TODO multi-thread support
#if 0
        out_q_t m_out_q;
#endif
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
        DISALLOW_COPY_AND_ASSIGN(t_http)

        //Helper for pthreads
        static void *t_run_static(void *a_context)
        {
                return reinterpret_cast<t_http *>(a_context)->t_run(NULL);
        }

        int32_t cleanup_connection(nconn *a_nconn, evr_timer_event_t *a_timer_obj, http_data_type_t a_type);

        // TODO multi-thread support
#if 0
        // Work Queue
        int32_t add_work(nconn *a_nconn, http_req *a_req);
        static void do_work(void *a_work);
#endif
        int32_t handle_req(nconn *a_nconn, url_router *a_url_router, http_req *a_req);

        // Initialize
        int32_t init(void);

        // Get new client connection
        nconn *get_new_client_conn(int a_fd, scheme_t a_scheme, url_router *a_url_router);
        int32_t config_conn(nconn *a_nconn,
                            url_router *a_url_router,
                            http_data_type_t a_type,
                            bool a_save,
                            bool a_connect_only);
        int32_t request(subreq *a_subreq, nconn *a_nconn = NULL);
        int32_t try_deq_subreq(void);
        void add_stat_to_agg(const req_stat_t &a_req_stat, uint16_t a_status_code);
        int32_t append_summary(http_resp *a_resp);

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

        // ---------------------------------------
        // TODO multi-thread support
        // ---------------------------------------
#if 0
        int m_out_q_fd;
        nconn *m_out_q_nconn;
        pthread_workqueue_t m_work_q;
        pthread_workqueue_attr_t m_work_q_attr;
#endif

        // ---------------------------------------
        // Subrequest support
        // ---------------------------------------
        int m_subreq_q_fd;
        nconn *m_subreq_q_nconn;

        bool m_is_initd;

};

} //namespace ns_hlx {

#endif // #ifndef _T_HTTP



