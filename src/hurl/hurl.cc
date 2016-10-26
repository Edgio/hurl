//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    main.cc
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

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "tinymt64.h"
#include "nlookup.h"
#include "nconn.h"
#include "nconn_tcp.h"
#include "nconn_tls.h"
#include "ndebug.h"
#include "cb.h"
#include "obj_pool.h"

#include "hlx/stat.h"
#include "hlx/api_resp.h"
#include "hlx/subr.h"
#include "hlx/time_util.h"
#include "hlx/trace.h"
#include "hlx/status.h"
#include "hlx/string_util.h"
#include "hlx/atomic.h"
#include "hlx/evr.h"
#include "hlx/resp.h"
#include "hlx/nbq.h"
#include "hlx/time_util.h"

#include <string.h>

// getrlimit
#include <sys/time.h>
#include <sys/resource.h>

// Mach time support clock_get_time
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

// signal
#include <signal.h>

#include <list>
#include <algorithm>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h> // For getopt_long
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <stdint.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>

// Profiler
#ifdef ENABLE_PROFILER
#include <gperftools/profiler.h>
#endif

// For inet_pton
#include <arpa/inet.h>

// Bind
#include <sys/types.h>
#include <sys/socket.h>

// Get resource limits
#include <sys/resource.h>

// Json output
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define LOCAL_ADDR_V4_MIN 0x7f000001
#define LOCAL_ADDR_V4_MAX 0x7ffffffe

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define UNUSED(x) ( (void)(x) )

#define CHECK_FOR_NULL_ERROR_DEBUG(_data) \
        do {\
                if(!_data) {\
                        NDBG_PRINT("Error.\n");\
                        return HLX_STATUS_ERROR;\
                }\
        } while(0);

#define CHECK_FOR_NULL_ERROR(_data) \
        do {\
                if(!_data) {\
                        return HLX_STATUS_ERROR;\
                }\
        } while(0);

#define T_HLX_SET_NCONN_OPT(_conn, _opt, _buf, _len) \
        do { \
                int _status = 0; \
                _status = _conn.set_opt((_opt), (_buf), (_len)); \
                if (_status != nconn::NC_STATUS_OK) { \
                        TRC_ERROR("set_opt %d.  Status: %d.\n", \
                                   _opt, _status); \
                        m_nconn_proxy_pool.release(&_conn); \
                        return HLX_STATUS_ERROR;\
                } \
        } while(0)

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------
// ---------------------------------------
// Wildcard support
// ---------------------------------------
typedef enum path_order_enum {

        EXPLODED_PATH_ORDER_SEQUENTIAL = 0,
        EXPLODED_PATH_ORDER_RANDOM

} path_order_t;

// ---------------------------------------
// Results scheme
// ---------------------------------------
typedef enum results_scheme_enum {

        RESULTS_SCHEME_STD = 0,
        RESULTS_SCHEME_HTTP_LOAD,
        RESULTS_SCHEME_HTTP_LOAD_LINE,
        RESULTS_SCHEME_JSON

} results_scheme_t;

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef struct range_struct {
        uint32_t m_start;
        uint32_t m_end;
} range_t;

typedef std::list <std::string> header_str_list_t;
typedef std::vector <std::string> path_substr_vector_t;
typedef std::vector <std::string> path_vector_t;
typedef std::map<std::string, std::string> header_map_t;
typedef std::vector <range_t> range_vector_t;
class t_hurl;
typedef std::list <t_hurl *> t_hurl_list_t;

//: ----------------------------------------------------------------------------
//: Globals
//: ----------------------------------------------------------------------------
static bool g_test_finished = false;
static bool g_verbose = false;
static bool g_color = false;
static uint64_t g_rate_delta_us = 0;
static uint32_t g_num_threads = 1;
static int64_t g_num_to_request = -1;
static int64_t g_reqs_per_conn = -1;
static std::string g_cipher_str_list;
static t_hurl_list_t g_t_hurl_list;
static bool g_stats = true;
static bool g_quiet = false;
static bool g_show_response_codes = false;
static bool g_show_per_interval = true;
static bool g_multipath = false;
static uint32_t g_interval_ms = 500;
static uint32_t g_num_parallel = 100;
static uint64_t g_start_time_ms = 0;
static int32_t g_run_time_ms = -1;

// -----------------------------------------------
// Path vector support
// -----------------------------------------------
static tinymt64_t *g_rand_ptr = NULL;
static path_vector_t g_path_vector;
static std::string g_path;
static uint32_t g_path_vector_last_idx = 0;
static path_order_t g_path_order = EXPLODED_PATH_ORDER_RANDOM;
static pthread_mutex_t g_path_vector_mutex;
static pthread_mutex_t g_completion_mutex;

//: ----------------------------------------------------------------------------
//: Prototypes
//: ----------------------------------------------------------------------------
void get_stat(ns_hlx::t_stat_cntr_t &ao_total,
              ns_hlx::t_stat_calc_t &ao_total_calc,
              ns_hlx::t_stat_cntr_list_t &ao_thread);
void display_results_line_desc(void);
void display_results_line(void);
void display_responses_line_desc(void);
void display_responses_line(void);

void get_results(double a_elapsed_time,
                 std::string &ao_results);

void get_results_http_load(double a_elapsed_time,
                           std::string &ao_results, bool a_one_line_flag = false);

void get_results_json(double a_elapsed_time,
                      std::string &ao_results);

int32_t read_file(const char *a_file, char **a_buf, uint32_t *a_len);
static int32_t s_create_request(ns_hlx::subr &a_subr, ns_hlx::nbq &a_nbq);

//: ----------------------------------------------------------------------------
//: \details: sighandler
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void sig_handler(int signo)
{
        if (signo == SIGINT)
        {
                // Kill program
                g_test_finished = true;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ns_hlx::uint64_atomic_t g_cur_subr_uid = 0;
static inline uint64_t get_next_subr_uuid(void)
{
        return ++g_cur_subr_uid;
}

//: ----------------------------------------------------------------------------
//: session
//: ----------------------------------------------------------------------------
class session {

public:
        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        ns_hlx::nconn *m_nconn;
        t_hurl *m_t_hurl;
        ns_hlx::evr_timer_t *m_timer_obj;
        ns_hlx::resp *m_resp;
        ns_hlx::nbq *m_in_q;
        ns_hlx::nbq *m_out_q;
        ns_hlx::subr *m_subr;
        uint64_t m_idx;
        // -------------------------------------------------
        // Public Static (class) methods
        // -------------------------------------------------
        static int32_t evr_fd_readable_cb(void *a_data) {return run_state_machine(a_data, ns_hlx::EVR_MODE_READ);}
        static int32_t evr_fd_writeable_cb(void *a_data){return run_state_machine(a_data, ns_hlx::EVR_MODE_WRITE);}
        static int32_t evr_fd_error_cb(void *a_data) {return run_state_machine(a_data, ns_hlx::EVR_MODE_ERROR);}
        static int32_t evr_fd_timeout_cb(void *a_ctx, void *a_data){return run_state_machine(a_data, ns_hlx::EVR_MODE_TIMEOUT);}
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        session(void):
                m_nconn(NULL),
                m_t_hurl(NULL),
                m_timer_obj(NULL),
                m_resp(NULL),
                m_in_q(NULL),
                m_out_q(NULL),
                m_subr(NULL),
                m_idx(0)
#if 0
        ,
                m_last_active_ms(0),
                m_timeout_ms(10000)
#endif
        {}
        int32_t cancel_timer(void *a_timer);
        int32_t teardown(ns_hlx::http_status_t a_status);
        int32_t subr_error(ns_hlx::http_status_t a_status);
        uint64_t get_idx(void) {return m_idx;}
        void set_idx(uint64_t a_idx) {m_idx = a_idx;}
#if 0
        void clear(void);
        uint32_t get_timeout_ms(void);
        void set_timeout_ms(uint32_t a_t_ms);
        uint64_t get_last_active_ms(void);
        void set_last_active_ms(uint64_t a_time_ms);
#endif
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        session& operator=(const session &);
        session(const session &);
        bool subr_complete(void);
        void subr_log_status(uint16_t a_status = 0);

        // -------------------------------------------------
        // Private Static (class) methods
        // -------------------------------------------------
        static int32_t run_state_machine(void *a_data, ns_hlx::evr_mode_t a_conn_mode);
        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
#if 0
        uint64_t m_last_active_ms;
        uint32_t m_timeout_ms;
#endif
};

//: ----------------------------------------------------------------------------
//: t_hurl
//: ----------------------------------------------------------------------------
class t_hurl
{
public:
        // -------------------------------------------------
        // Types
        // -------------------------------------------------
        typedef std::list <ns_hlx::nconn *> idle_nconn_list_t;
        typedef ns_hlx::obj_pool <session> session_pool_t;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        t_hurl(ns_hlx::subr a_subr,
               uint32_t a_max_parallel,
               int32_t a_num_to_request):
               m_stopped(true),
               m_t_run_thread(),
               m_idle_nconn_list(),
               m_session_pool(),
               m_stat(),
               m_status_code_count_map(),
               m_num_in_progress(0),
               m_orphan_in_q(NULL),
               m_orphan_out_q(NULL),
               m_subr(a_subr),
               m_evr_loop(NULL),
               m_is_initd(false),
               m_num_parallel_max(a_max_parallel),
               m_num_to_request(a_num_to_request)
        {
                m_orphan_in_q = new ns_hlx::nbq(4096);
                m_orphan_out_q = new ns_hlx::nbq(4096);
        }
        ~t_hurl()
        {
                if(m_orphan_in_q)
                {
                        delete m_orphan_in_q;
                        m_orphan_in_q = NULL;
                }
                if(m_orphan_out_q)
                {
                        delete m_orphan_out_q;
                        m_orphan_out_q = NULL;
                }
        }
        int32_t init(void)
        {
                if(m_is_initd) return HLX_STATUS_OK;
                // TODO -make loop configurable
#if defined(__linux__)
                m_evr_loop = new ns_hlx::evr_loop(ns_hlx::EVR_LOOP_EPOLL, 512);
#elif defined(__FreeBSD__) || defined(__APPLE__)
                m_evr_loop = new ns_hlx::evr_loop(ns_hlx::EVR_LOOP_SELECT, 512);
#else
                m_evr_loop = new ns_hlx::evr_loop(ns_hlx::EVR_LOOP_SELECT, 512);
#endif
                if(!m_evr_loop)
                {
                        TRC_ERROR("m_evr_loop == NULL");
                        return HLX_STATUS_ERROR;
                }
                m_is_initd = true;
                m_subr.set_data(this);
                return HLX_STATUS_OK;
        }
        int run(void) {
                int32_t l_pthread_error = 0;
                l_pthread_error = pthread_create(&m_t_run_thread, NULL, t_run_static, this);
                if (l_pthread_error != 0) {
                        return HLX_STATUS_ERROR;
                }
                return HLX_STATUS_OK;
        };
        void *t_run(void *a_nothing);
        int32_t subr_try_start(void);
        void stop(void) {
                m_stopped = true;
                m_evr_loop->signal();
        }
        bool is_running(void) { return !m_stopped; }
        int32_t cancel_timer(void *a_timer) {
                if(!m_evr_loop) return HLX_STATUS_ERROR;
                if(!a_timer) return HLX_STATUS_OK;
                ns_hlx::evr_timer_t *l_t = static_cast<ns_hlx::evr_timer_t *>(a_timer);
                return m_evr_loop->cancel_timer(l_t);
        }
        int32_t cleanup_session(session *a_ses, ns_hlx::nconn *a_nconn);
        void add_stat_to_agg(const ns_hlx::conn_stat_t &a_conn_stat, uint16_t a_status_code);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        sig_atomic_t m_stopped;
        pthread_t m_t_run_thread;
        idle_nconn_list_t m_idle_nconn_list;
        session_pool_t m_session_pool;
        ns_hlx::t_stat_cntr_t m_stat;
        ns_hlx::status_code_count_map_t m_status_code_count_map;
        uint32_t m_num_in_progress;
        ns_hlx::nbq *m_orphan_in_q;
        ns_hlx::nbq *m_orphan_out_q;
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        t_hurl& operator=(const t_hurl &);
        t_hurl(const t_hurl &);

        //Helper for pthreads
        static void *t_run_static(void *a_context)
        {
                return reinterpret_cast<t_hurl *>(a_context)->t_run(NULL);
        }
        ns_hlx::nconn *create_new_nconn(void);
        int32_t subr_start(void);
        int32_t subr_dequeue(void);
        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        ns_hlx::subr m_subr;
        ns_hlx::evr_loop *m_evr_loop;
        bool m_is_initd;
        uint32_t m_num_parallel_max;
        int32_t m_num_to_request;

};

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void session::subr_log_status(uint16_t a_status)
{
        if(!m_t_hurl)
        {
                return;
        }
        // TODO log here???
        //++(m_t_hurl->m_stat.m_upsv_resp);
        uint16_t l_status;
        if(m_resp)
        {
                l_status = m_resp->get_status();
        }
        else if(a_status)
        {
                l_status = a_status;
        }
        else
        {
                l_status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
        }
        if((l_status >= 100) && (l_status < 200)) {/* TODO log 1xx's? */}
        else if((l_status >= 200) && (l_status < 300)){++m_t_hurl->m_stat.m_upsv_resp_status_2xx;}
        else if((l_status >= 300) && (l_status < 400)){++m_t_hurl->m_stat.m_upsv_resp_status_3xx;}
        else if((l_status >= 400) && (l_status < 500)){++m_t_hurl->m_stat.m_upsv_resp_status_4xx;}
        else if((l_status >= 500) && (l_status < 600)){++m_t_hurl->m_stat.m_upsv_resp_status_5xx;}
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool session::subr_complete(void)
{
        subr_log_status(0);
        if(!m_resp || !m_subr || !m_nconn)
        {
                return true;
        }
        bool l_complete = false;
#if 0
        m_subr->set_end_time_ms(ns_hlx::get_time_ms());
#endif
        // Get vars -completion -can delete subr object
        bool l_connect_only = m_subr->get_connect_only();
        ns_hlx::subr::completion_cb_t l_completion_cb = m_subr->get_completion_cb();
        // Call completion handler
        if(l_completion_cb)
        {
                l_completion_cb(*m_subr, *m_nconn, *m_resp);
        }
        // Connect only
        if(l_connect_only)
        {
                l_complete = true;
        }
        return l_complete;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t session::subr_error(ns_hlx::http_status_t a_status)
{
        subr_log_status(a_status);
        if(!m_subr)
        {
                return HLX_STATUS_ERROR;
        }
#if 0
        m_subr->set_end_time_ms(ns_hlx::get_time_ms());
        if(m_nconn && m_nconn->get_collect_stats_flag())
        {
                m_nconn->set_stat_tt_completion_us(ns_hlx::get_delta_time_us(m_nconn->get_connect_start_time_us()));
                m_nconn->reset_stats();
        }
#endif
        ns_hlx::subr::error_cb_t l_error_cb = m_subr->get_error_cb();
        if(l_error_cb)
        {
                const char *l_err_str = NULL;
                if(m_nconn)
                {
                        l_err_str = m_nconn->get_last_error().c_str();
                }
                l_error_cb(*(m_subr), m_nconn, a_status, l_err_str);
                // TODO Check status...
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t session::cancel_timer(void *a_timer)
{
        if(!m_t_hurl)
        {
            return HLX_STATUS_ERROR;
        }
        int32_t l_s;
        l_s = m_t_hurl->cancel_timer(a_timer);
        if(l_s != HLX_STATUS_OK)
        {
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t session::teardown(ns_hlx::http_status_t a_status)
{
        if(a_status != ns_hlx::HTTP_STATUS_OK)
        {
                if(m_resp)
                {
                        m_resp->set_status(a_status);
                }
                int32_t l_s;
                l_s = subr_error(a_status);
                if(l_s != HLX_STATUS_OK)
                {
                        TRC_ERROR("performing subr_error\n");
                }
        }
        if(m_t_hurl)
        {
                int32_t l_s;
                l_s = m_t_hurl->cleanup_session(this, m_nconn);
                if(l_s != HLX_STATUS_OK)
                {
                        return HLX_STATUS_ERROR;
                }
        }
        return HLX_STATUS_OK;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t session::run_state_machine(void *a_data, ns_hlx::evr_mode_t a_conn_mode)
{
        //NDBG_PRINT("%sRUN%s a_conn_mode: %d a_data: %p\n", ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF, a_conn_mode, a_data);
        //CHECK_FOR_NULL_ERROR(a_data);
        // TODO -return OK for a_data == NULL
        if(!a_data)
        {
                return HLX_STATUS_OK;
        }
        ns_hlx::nconn* l_nconn = static_cast<ns_hlx::nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_ctx());
        t_hurl *l_t_hurl = static_cast<t_hurl *>(l_nconn->get_ctx());
        session *l_ses = static_cast<session *>(l_nconn->get_data());

        // -------------------------------------------------
        // ERROR
        // -------------------------------------------------
        if(a_conn_mode == ns_hlx::EVR_MODE_ERROR)
        {
                // ignore errors for free connections
                if(l_nconn->is_free())
                {
                        TRC_ERROR("call back for free connection\n");
                        return HLX_STATUS_OK;
                }
                if(l_ses)
                {
                        l_ses->cancel_timer(l_ses->m_timer_obj);
                        // TODO Check status
                        l_ses->m_timer_obj = NULL;
                        return l_ses->teardown(ns_hlx::HTTP_STATUS_BAD_GATEWAY);
                }
                else
                {
                        if(l_t_hurl)
                        {
                                return l_t_hurl->cleanup_session(NULL, l_nconn);
                        }
                        else
                        {
                                // TODO log error???
                                return HLX_STATUS_ERROR;
                        }
                }
        }
        // -------------------------------------------------
        // TIMEOUT
        // -------------------------------------------------
        else if(a_conn_mode == ns_hlx::EVR_MODE_TIMEOUT)
        {
                // ignore timeout for free connections
                if(l_nconn->is_free())
                {
                        TRC_ERROR("call back for free connection\n");
                        return HLX_STATUS_OK;
                }
                // calc time since last active
                if(l_ses && l_t_hurl)
                {
                        // ---------------------------------
                        // timeout
                        // ---------------------------------
#if 0
                        uint64_t l_ct_ms = ns_hlx::get_time_ms();
                        if(((uint32_t)(l_ct_ms - l_uss->get_last_active_ms())) >= l_uss->get_timeout_ms())
                        {
                                ++(l_t_srvr->m_stat.m_upsv_errors);
                                ++(l_t_srvr->m_stat.m_upsv_idle_killed);
                                return l_uss->teardown(HTTP_STATUS_GATEWAY_TIMEOUT);
                        }
                        // ---------------------------------
                        // active -create new timer with
                        // delta time
                        // ---------------------------------
                        else
                        {
                                uint64_t l_d_time = (uint32_t)(l_uss->get_timeout_ms() - (l_ct_ms - l_uss->get_last_active_ms()));
                                l_t_srvr->add_timer(l_d_time,
                                                    ups_srvr_session::evr_fd_timeout_cb,
                                                    l_nconn,
                                                    (void **)(&(l_uss->m_timer_obj)));
                                // TODO check status
                                return HLX_STATUS_OK;
                        }
#endif
                }
                else
                {
                        TRC_ERROR("a_conn_mode[%d] ups_srvr_session[%p] || t_srvr[%p] == NULL\n",
                                        a_conn_mode,
                                        l_ses,
                                        l_t_hurl);
                        return HLX_STATUS_ERROR;
                }
        }
        else if(a_conn_mode == ns_hlx::EVR_MODE_READ)
        {
                // ignore readable for free connections
                if(l_nconn->is_free())
                {
                        TRC_ERROR("call back for free connection\n");
                        return HLX_STATUS_OK;
                }
        }
        // -------------------------------------------------
        // TODO unknown conn mode???
        // -------------------------------------------------
        else if((a_conn_mode != ns_hlx::EVR_MODE_READ) &&
                (a_conn_mode != ns_hlx::EVR_MODE_WRITE))
        {
                TRC_ERROR("unknown a_conn_mode: %d\n", a_conn_mode);
                return HLX_STATUS_OK;
        }

#if 0
        // set last active
        if(l_ses)
        {
                l_uss->set_last_active_ms(ns_hlx::get_time_ms());
        }
#endif

        // -------------------------------------------------
        // in/out q's
        // -------------------------------------------------
        ns_hlx::nbq *l_in_q = NULL;
        ns_hlx::nbq *l_out_q = NULL;
        if(l_ses && l_ses->m_in_q)
        {
                l_in_q = l_ses->m_in_q;
        }
        else
        {
                l_in_q = l_t_hurl->m_orphan_in_q;
        }
        if(l_ses && l_ses->m_out_q)
        {
                l_out_q = l_ses->m_out_q;
        }
        else
        {
                l_out_q = l_t_hurl->m_orphan_out_q;
        }

        // -------------------------------------------------
        // conn loop
        // -------------------------------------------------
        bool l_idle = false;
        int32_t l_s = HLX_STATUS_OK;
        do {
                uint32_t l_read = 0;
                uint32_t l_written = 0;
                l_s = l_nconn->nc_run_state_machine(a_conn_mode, l_in_q, l_read, l_out_q, l_written);
                //NDBG_PRINT("l_nconn->nc_run_state_machine(%d): status: %d read: %u l_written: %u\n", a_conn_mode, l_s, l_read, l_written);
                if(l_t_hurl)
                {
                        l_t_hurl->m_stat.m_upsv_bytes_read += l_read;
                        l_t_hurl->m_stat.m_upsv_bytes_written += l_written;
                        if(!g_verbose && (l_written > 0))
                        {
                                l_in_q->reset_write();
                        }
                }
                if(!l_ses ||
                   !l_ses->m_subr ||
                   (l_s == ns_hlx::nconn::NC_STATUS_EOF) ||
                   (l_s == ns_hlx::nconn::NC_STATUS_ERROR) ||
                   l_nconn->is_done())
                {
                        goto check_conn_status;
                }
                // -----------------------------------------
                // READABLE
                // -----------------------------------------
                if(a_conn_mode == ns_hlx::EVR_MODE_READ)
                {
                        // -----------------------------------------
                        // Handle completion
                        // -----------------------------------------
                        if(l_ses->m_resp &&
                           l_ses->m_resp->m_complete)
                        {
                                // Cancel timer
                                l_nconn->bump_num_requested();
                                l_ses->cancel_timer(l_ses->m_timer_obj);
                                // TODO Check status
                                l_ses->m_timer_obj = NULL;

                                // Decrement in progress
                                if(l_t_hurl->m_num_in_progress)
                                {
                                        --l_t_hurl->m_num_in_progress;
                                        ++l_t_hurl->m_stat.m_upsv_resp;
                                }

                                if(g_verbose)
                                {
                                        if(g_color) TRC_OUTPUT("%s", ANSI_COLOR_FG_CYAN);
                                        l_ses->m_resp->show();
                                        if(g_color) TRC_OUTPUT("%s", ANSI_COLOR_OFF);
                                }

                                // Get request time
                                if(g_stats)
                                {
                                        l_nconn->set_stat_tt_completion_us(ns_hlx::get_delta_time_us(l_nconn->get_connect_start_time_us()));

                                }
                                if(l_ses->m_resp && l_t_hurl)
                                {
                                        l_t_hurl->add_stat_to_agg(l_nconn->get_stats(), l_ses->m_resp->get_status());
                                }
                                bool l_hmsg_keep_alive = false;
                                if(l_ses->m_resp)
                                {
                                        l_hmsg_keep_alive = l_ses->m_resp->m_supports_keep_alives;
                                }
                                bool l_nconn_can_reuse = l_nconn->can_reuse();
                                bool l_keepalive = l_ses->m_subr->get_keepalive();
                                bool l_complete = l_ses->subr_complete();
                                if(l_complete ||
                                  (!l_nconn_can_reuse ||
                                   !l_keepalive ||
                                   !l_hmsg_keep_alive))
                                {
                                        l_s = ns_hlx::nconn::NC_STATUS_EOF;
                                        goto check_conn_status;
                                }
                                // Give back rqst + in q
                                if(l_t_hurl)
                                {
                                        l_ses->m_nconn = NULL;
                                        l_t_hurl->m_session_pool.release(l_ses);
                                        l_ses = NULL;
                                        l_in_q = l_t_hurl->m_orphan_in_q;
                                        l_out_q = l_t_hurl->m_orphan_out_q;
                                }
                                l_idle = true;
                                //NDBG_PRINT("l_idle: %d\n", l_idle);
                                goto idle_check;
                        }
                }
                // -----------------------------------------
                // STATUS_OK
                // -----------------------------------------
                else if(l_s == ns_hlx::nconn::NC_STATUS_OK)
                {
                        l_s = ns_hlx::nconn::NC_STATUS_BREAK;
                        //NDBG_PRINT("goto check_conn_status\n");
                        goto check_conn_status;
                }

check_conn_status:
                //NDBG_PRINT("goto check_conn_status\n");
                if(l_nconn->is_free())
                {
                        return HLX_STATUS_OK;
                }
                if(!l_ses)
                {
                        TRC_ERROR("no ups_srvr_session associated with nconn mode: %d\n", a_conn_mode);
                        if(l_t_hurl)
                        {
                                return l_t_hurl->cleanup_session(NULL, l_nconn);
                        }
                        else
                        {
                                // TODO log error???
                                return HLX_STATUS_ERROR;
                        }
                }
                switch(l_s)
                {
                case ns_hlx::nconn::NC_STATUS_BREAK:
                {
                        //NDBG_PRINT("GOTO DONE!\n");
                        goto done;
                }
                case ns_hlx::nconn::NC_STATUS_EOF:
                {
                        // Connect only && done --early exit...
                        if(l_ses->m_subr &&
                           l_ses->m_subr->get_connect_only())
                        {
                                if(l_ses->m_resp)
                                {
                                        l_ses->m_resp->set_status(ns_hlx::HTTP_STATUS_OK);
                                }
                                if(l_t_hurl)
                                {
                                        l_t_hurl->add_stat_to_agg(l_nconn->get_stats(), HTTP_STATUS_OK);
                                }
                        }
                        if(l_ses->m_subr)
                        {
                                l_ses->subr_complete();
                        }
                        return l_ses->teardown(ns_hlx::HTTP_STATUS_OK);
                }
                case ns_hlx::nconn::NC_STATUS_ERROR:
                {
                        if(l_t_hurl)
                        {
                                ++(l_t_hurl->m_stat.m_upsv_errors);
                        }
                        return l_ses->teardown(ns_hlx::HTTP_STATUS_BAD_GATEWAY);
                }
                default:
                {
                        break;
                }
                }
                if(l_nconn->is_done())
                {
                        return l_ses->teardown(ns_hlx::HTTP_STATUS_OK);
                }
        } while((l_s != ns_hlx::nconn::NC_STATUS_AGAIN) &&
                (l_t_hurl->is_running()));

idle_check:
        //NDBG_PRINT("%sIDLE CONNECTION CHECK%s: l_idle: %d\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, l_idle);
        if(l_idle)
        {
                if(l_t_hurl)
                {
                        l_t_hurl->m_idle_nconn_list.push_back(l_nconn);
                }
                l_nconn->set_data(NULL);
                // TODO start new subr???
                return HLX_STATUS_OK;
        }
done:
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_hurl::t_run(void *a_nothing)
{
        int32_t l_s;
        l_s = init();
        if(l_s != HLX_STATUS_OK)
        {
                NDBG_PRINT("Error performing init.\n");
                return NULL;
        }
        m_stopped = false;
        m_stat.clear();
        l_s = subr_dequeue();
        // TODO check return status???
        // -------------------------------------------------
        // Run server
        // -------------------------------------------------
        while((!m_stopped) &&
              ((m_num_to_request < 0) || ((uint32_t)m_num_to_request > m_stat.m_upsv_resp)))
        {
                //NDBG_PRINT("Running.\n");
                l_s = m_evr_loop->run();
                if(l_s != HLX_STATUS_OK)
                {
                        // TODO log run failure???
                }
                // Subrequests
                l_s = subr_dequeue();
                if(l_s != HLX_STATUS_OK)
                {
                        //NDBG_PRINT("Error performing subr_try_deq.\n");
                        //return NULL;
                }
        }
        //NDBG_PRINT("Stopped...\n");
        m_stopped = true;
        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hurl::subr_dequeue(void)
{
        uint32_t l_dq = 0;
        while(!g_test_finished &&
              !m_stopped &&
              (m_num_in_progress < m_num_parallel_max) &&
              ((m_num_to_request < 0) || ((uint32_t)m_num_to_request > m_stat.m_upsv_reqs)))
        {
                int32_t l_s;
                l_s = subr_start();
                if(l_s != HLX_STATUS_OK)
                {
                        // Log error
                        NDBG_PRINT("%sERROR DEQUEUEING%s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF);
                }
                else
                {
                      ++m_num_in_progress;
                }
                ++l_dq;
        }
        //NDBG_PRINT("%sDEQUEUEd%s: %u progress/max %u/%u\n",
        //                ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF,
        //                l_dq,
        //                m_num_in_progress, m_num_parallel_max);
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ns_hlx::nconn *t_hurl::create_new_nconn(void)
{
        ns_hlx::nconn *l_nconn = NULL;
        if(m_subr.get_scheme() == ns_hlx::SCHEME_TLS)
        {
                l_nconn = new ns_hlx::nconn_tls();
        }
        else if(m_subr.get_scheme() == ns_hlx::SCHEME_TCP)
        {
                l_nconn = new ns_hlx::nconn_tcp();
        }
        else
        {
                return NULL;
        }
        l_nconn->set_ctx(this);
        l_nconn->set_num_reqs_per_conn(g_reqs_per_conn);
        l_nconn->set_collect_stats(false);
        l_nconn->set_connect_only(false);
        l_nconn->set_evr_loop(m_evr_loop);
        l_nconn->setup_evr_fd(session::evr_fd_readable_cb,
                              session::evr_fd_writeable_cb,
                              session::evr_fd_error_cb);
        l_nconn->set_host_info(m_subr.get_host_info());
        return l_nconn;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hurl::subr_start(void)
{
        //NDBG_PRINT("%ssubr label%s: %s --HOST: %s\n",
        //                ANSI_COLOR_FG_RED, ANSI_COLOR_OFF,
        //                m_subr.get_label().c_str(), m_subr.get_host().c_str());
        int32_t l_s;
        // Try get idle from proxy pool
        ns_hlx::nconn *l_nconn = NULL;
        // try get from idle list

        if(m_idle_nconn_list.empty() ||
           (m_idle_nconn_list.front() == NULL))
        {
                l_nconn = create_new_nconn();
                //NDBG_PRINT("%sCREATING NEW CONNECTION%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
        }
        else
        {
                l_nconn = m_idle_nconn_list.front();
                m_idle_nconn_list.pop_front();
        }
        if(!l_nconn)
        {
                // TODO fatal???
                return HLX_STATUS_ERROR;
        }
        // Reset stats
        if(g_stats)
        {
                l_nconn->set_collect_stats(g_stats);
                l_nconn->reset_stats();
        }
        // ---------------------------------------
        // setup session
        // ---------------------------------------
        session *l_ses = NULL;
        bool l_ses_reused = false;
        l_ses = m_session_pool.get_free();
        if(!l_ses)
        {
                l_ses = new session();
                m_session_pool.add(l_ses);
        }
        else
        {
                l_ses_reused = true;
                //NDBG_PRINT("%sREUSE_SESSION%s\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF);
        }
        //NDBG_PRINT("Adding http_data: %p.\n", l_clnt_session);
        l_ses->m_t_hurl = this;
        l_ses->m_timer_obj = NULL;

        // Setup clnt_session
        l_ses->m_nconn = l_nconn;
        l_nconn->set_data(l_ses);
        l_ses->m_subr = &m_subr;

        // ---------------------------------------
        // setup resp
        // ---------------------------------------
        //NDBG_PRINT("TID[%lu]: %sSTART%s: Host: %s l_nconn: %p\n",
        //                pthread_self(),
        //                ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF,
        //                a_subr.get_label().c_str(),
        //                &a_nconn);
        if(!l_ses->m_resp)
        {
                l_ses->m_resp = new ns_hlx::resp();
        }
        l_ses->m_resp->init(g_verbose);
        l_ses->m_resp->m_http_parser->data = l_ses->m_resp;
        l_nconn->set_read_cb(ns_hlx::http_parse);
        l_nconn->set_read_cb_data(l_ses->m_resp);
        l_ses->m_resp->m_expect_resp_body_flag = m_subr.get_expect_resp_body_flag();

        // setup q's
        if(!l_ses->m_in_q)
        {
                l_ses->m_in_q = new ns_hlx::nbq(8*1024);
                l_ses->m_resp->set_q(l_ses->m_in_q);
        }
        else
        {
                l_ses->m_in_q->reset_write();
        }
        if(!l_ses->m_out_q)
        {
                l_ses->m_out_q = new ns_hlx::nbq(8*1024);
        }
        else
        {
                if(l_ses_reused && !g_multipath)
                {
                        l_ses->m_out_q->reset_read();
                }
                else
                {
                        l_ses->m_out_q->reset_write();
                }
        }
        // create request
        if(!l_ses_reused || g_multipath)
        {
                l_s = s_create_request(m_subr, *(l_ses->m_out_q));
                if(HLX_STATUS_OK != l_s)
                {
                        return session::evr_fd_error_cb(l_nconn);
                }
        }

        // stats
        ++m_stat.m_upsv_reqs;

        if(g_stats)
        {
                m_subr.set_start_time_ms(ns_hlx::get_time_ms());
                l_nconn->set_request_start_time_us(ns_hlx::get_time_us());
        }
#if 0
        l_uss->set_last_active_ms(ns_hlx::get_time_ms());
        l_uss->set_timeout_ms(a_subr.get_timeout_ms());
#endif

        // ---------------------------------------
        // idle timer
        // ---------------------------------------
        // TODO ???
#if 0
        l_s = add_timer(l_uss->get_timeout_ms(),
                             ups_srvr_session::evr_fd_timeout_cb,
                             l_nconn,
                             (void **)&(l_uss->m_timer_obj));
        if(l_s != HLX_STATUS_OK)
        {
                return ups_srvr_session::evr_fd_error_cb(l_nconn);
        }
#endif

        // ---------------------------------------
        // Display data from out q
        // ---------------------------------------
        if(g_verbose)
        {
                if(g_color) TRC_OUTPUT("%s", ANSI_COLOR_FG_YELLOW);
                l_ses->m_out_q->print();
                if(g_color) TRC_OUTPUT("%s", ANSI_COLOR_OFF);
        }
        // ---------------------------------------
        // start writing request
        // ---------------------------------------
        return session::evr_fd_writeable_cb(l_nconn);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hurl::cleanup_session(session *a_ses, ns_hlx::nconn *a_nconn)
{
        if(a_ses)
        {
#if 0
                // Cancel last timer
                cancel_timer(a_uss->m_timer_obj);
                a_uss->m_timer_obj = NULL;
#endif
                if(a_ses->m_t_hurl)
                {
                        a_ses->m_nconn = NULL;
                        a_ses->m_t_hurl->m_session_pool.release(a_ses);
                }
        }
        if(a_nconn)
        {
                delete a_nconn;
                // TODO Log error???
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_hurl::add_stat_to_agg(const ns_hlx::conn_stat_t &a_conn_stat, uint16_t a_status_code)
{
        update_stat(m_stat.m_upsv_stat_us_connect, a_conn_stat.m_tt_connect_us);
        update_stat(m_stat.m_upsv_stat_us_first_response, a_conn_stat.m_tt_first_read_us);
        update_stat(m_stat.m_upsv_stat_us_end_to_end, a_conn_stat.m_tt_completion_us);
        ++m_status_code_count_map[a_status_code];
}

//: ----------------------------------------------------------------------------
//: \details: Completion callback
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t s_completion_cb(ns_hlx::subr &a_subr,
                               ns_hlx::nconn &a_nconn,
                               ns_hlx::resp &a_resp)
{
        //pthread_mutex_lock(&g_completion_mutex);
        //++g_num_completed;
        //pthread_mutex_unlock(&g_completion_mutex);
#if 0
        if((g_num_to_request != -1) && (g_num_completed >= (uint64_t)g_num_to_request))
        {
                g_test_finished = true;
                return 0;
        }
#endif
        if(g_rate_delta_us && !g_test_finished)
        {
                usleep(g_rate_delta_us*g_num_threads);
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#if 0
static int32_t s_pre_connect_cb(int a_fd)
{
        pthread_mutex_lock(&g_addrx_mutex);
        struct sockaddr_in l_c_addr;
        bzero((char *) &l_c_addr, sizeof(l_c_addr));
        l_c_addr.sin_family = AF_INET;
        l_c_addr.sin_addr.s_addr = htonl(g_addrx_addr_ipv4);
        l_c_addr.sin_port = htons(0);
        int32_t l_s;
        errno = 0;
        l_s = bind(a_fd, (struct sockaddr *) &l_c_addr, sizeof(l_c_addr));
        if(l_s != 0)
        {
                printf("%s.%s.%d: Error performing bind. Reason: %s.\n",
                       __FILE__,__FUNCTION__,__LINE__,strerror(errno));
                pthread_mutex_unlock(&g_addrx_mutex);
                return HLX_STATUS_ERROR;
        }
        ++g_addrx_addr_ipv4;
        if(g_addrx_addr_ipv4 >= g_addrx_addr_ipv4_max)
        {
                g_addrx_addr_ipv4 = LOCAL_ADDR_V4_MIN;
        }
        pthread_mutex_unlock(&g_addrx_mutex);
        return HLX_STATUS_OK;
}
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
// Assumes expr in form [<val>-<val>] where val can be either int or hex
static int32_t convert_exp_to_range(const std::string &a_range_exp, range_t &ao_range)
{

        char l_expr[64];
        strncpy(l_expr, a_range_exp.c_str(), 64);

        uint32_t l_start;
        uint32_t l_end;
        int32_t l_s;

        l_s = sscanf(l_expr, "[%u-%u]", &l_start, &l_end);
        if(2 == l_s) goto success;

        // Else lets try hex...
        l_s = sscanf(l_expr, "[%x-%x]", &l_start, &l_end);
        if(2 == l_s) goto success;

        return HLX_STATUS_ERROR;

success:
        // Check range...
        if(l_start > l_end)
        {
                printf("HLX_STATUS_ERROR: Bad range start[%u] > end[%u]\n", l_start, l_end);
                return HLX_STATUS_ERROR;
        }

        // Successfully matched we outie
        ao_range.m_start = l_start;
        ao_range.m_end = l_end;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t parse_path(const char *a_path,
                   path_substr_vector_t &ao_substr_vector,
                   range_vector_t &ao_range_vector)
{

        //NDBG_PRINT("a_path: %s\n", a_path);

        // If has range '[' char assume it's explodable path
        std::string l_path(a_path);
        size_t l_range_start_pos = 0;
        size_t l_range_end_pos = 0;
        size_t l_cur_str_pos = 0;

        while((l_range_start_pos = l_path.find("[", l_cur_str_pos)) != std::string::npos)
        {
                if((l_range_end_pos = l_path.find("]", l_range_start_pos)) == std::string::npos)
                {
                        printf("HLX_STATUS_ERROR: Bad range for path: %s at pos: %zu\n", a_path, l_range_start_pos);
                        return HLX_STATUS_ERROR;
                }

                // Push path back...
                std::string l_substr = l_path.substr(l_cur_str_pos, l_range_start_pos - l_cur_str_pos);
                //NDBG_PRINT("l_substr: %s from %lu -- %lu\n", l_substr.c_str(), l_cur_str_pos, l_range_start_pos);
                ao_substr_vector.push_back(l_substr);

                // Convert to range
                std::string l_range_exp = l_path.substr(l_range_start_pos, l_range_end_pos - l_range_start_pos + 1);

                std::string::iterator end_pos = std::remove(l_range_exp.begin(), l_range_exp.end(), ' ');
                l_range_exp.erase(end_pos, l_range_exp.end());

                //NDBG_PRINT("l_range_exp: %s\n", l_range_exp.c_str());

                range_t l_range;
                int l_s = HLX_STATUS_OK;
                l_s = convert_exp_to_range(l_range_exp, l_range);
                if(HLX_STATUS_OK != l_s)
                {
                        printf("HLX_STATUS_ERROR: performing convert_exp_to_range(%s)\n", l_range_exp.c_str());
                        return HLX_STATUS_ERROR;
                }

                ao_range_vector.push_back(l_range);

                l_cur_str_pos = l_range_end_pos + 1;

                // Searching next at:
                //NDBG_PRINT("Searching next at: %lu -- result: %lu\n", l_cur_str_pos, l_path.find("[", l_cur_str_pos));

        }

        // Get trailing string
        std::string l_substr = l_path.substr(l_cur_str_pos, l_path.length() - l_cur_str_pos);
        //NDBG_PRINT("l_substr: %s from %lu -- %lu\n", l_substr.c_str(), l_cur_str_pos, l_path.length());
        ao_substr_vector.push_back(l_substr);

#if 0
        // -------------------------------------------
        // Explode the lists
        // -------------------------------------------
        for(path_substr_vector_t::iterator i_substr = ao_substr_vector.begin();
                        i_substr != ao_substr_vector.end();
                        ++i_substr)
        {
                printf("SUBSTR: %s\n", i_substr->c_str());
        }

        for(range_vector_t::iterator i_range = ao_range_vector.begin();
                        i_range != ao_range_vector.end();
                        ++i_range)
        {
                printf("RANGE: %u -- %u\n", i_range->m_start, i_range->m_end);
        }
#endif
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t path_exploder(std::string a_path_part,
                      const path_substr_vector_t &a_substr_vector,
                      uint32_t a_substr_idx,
                      const range_vector_t &a_range_vector,
                      uint32_t a_range_idx)
{
        //a_path_part
        if(a_substr_idx >= a_substr_vector.size())
        {
                g_path_vector.push_back(a_path_part);
                return HLX_STATUS_OK;
        }

        a_path_part.append(a_substr_vector[a_substr_idx]);
        ++a_substr_idx;

        if(a_range_idx >= a_range_vector.size())
        {
                g_path_vector.push_back(a_path_part);
                return HLX_STATUS_OK;
        }

        range_t l_range = a_range_vector[a_range_idx];
        ++a_range_idx;
        for(uint32_t i_r = l_range.m_start; i_r <= l_range.m_end; ++i_r)
        {
                char l_num_str[32];
                sprintf(l_num_str, "%d", i_r);
                std::string l_path = a_path_part;
                l_path.append(l_num_str);
                path_exploder(l_path, a_substr_vector, a_substr_idx,a_range_vector,a_range_idx);
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t add_option(const char *a_key, const char *a_val)
{
        std::string l_key(a_key);
        std::string l_val(a_val);
        //NDBG_PRINT("%s: %s\n",a_key, a_val);
        //: ------------------------------------------------
        //:
        //: ------------------------------------------------
        if (l_key == "order")
        {
                if (l_val == "sequential")
                {
                        g_path_order = EXPLODED_PATH_ORDER_SEQUENTIAL;
                }
                else if (l_val == "random")
                {
                        g_path_order = EXPLODED_PATH_ORDER_RANDOM;
                }
                else
                {
                        printf("HLX_STATUS_ERROR: Bad value[%s] for key[%s]\n", l_val.c_str(), l_key.c_str());
                        return HLX_STATUS_ERROR;
                }
        }
        //: ------------------------------------------------
        //:
        //: ------------------------------------------------
        //else if (l_key == "tag")
        //{
        //        //NDBG_PRINT("HEADER: %s: %s\n", l_val.substr(0,l_pos).c_str(), l_val.substr(l_pos+1,l_val.length()).c_str());
        //        m_label = l_val;
        //}
#if 0
        //: ------------------------------------------------
        //:
        //: ------------------------------------------------
        else if (l_key == "sampling")
        {
                if (l_val == "oneshot") m_run_only_once_flag = true;
                else if (l_val == "reuse") m_run_only_once_flag = false;
                else
                {
                        NDBG_PRINT("HLX_STATUS_ERROR: Bad value[%s] for key[%s]\n", l_val.c_str(), l_key.c_str());
                        return HLX_STATUS_ERROR;
                }
        }
#endif
        else
        {
                printf("HLX_STATUS_ERROR: Unrecognized key[%s]\n", l_key.c_str());
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define SPECIAL_EFX_OPT_SEPARATOR ";"
#define SPECIAL_EFX_KV_SEPARATOR "="
int32_t special_effects_parse(std::string &a_path)
{
        //printf("SPECIAL_FX_PARSE: path: %s\n", a_path.c_str());
        // -------------------------------------------
        // 1. Break by separator ";"
        // 2. Check for exploded path
        // 3. For each bit after path
        //        Split by Key "=" Value
        // -------------------------------------------
        // Bail out if no path
        if(a_path.empty())
        {
                return HLX_STATUS_OK;
        }

        // -------------------------------------------
        // TODO This seems hacky...
        // -------------------------------------------
        // strtok is NOT thread-safe but not sure it matters here...
        char l_path[2048];
        char *l_save_ptr;
        strncpy(l_path, a_path.c_str(), 2048);
        char *l_p = strtok_r(l_path, SPECIAL_EFX_OPT_SEPARATOR, &l_save_ptr);

        if( a_path.front() != *SPECIAL_EFX_OPT_SEPARATOR )
        {
                // Rule out special cases that m_path only contains options
                //
                a_path = l_p;

                int32_t l_s;
                path_substr_vector_t l_path_substr_vector;
                range_vector_t l_range_vector;
                if(l_p)
                {
                        l_s = parse_path(l_p, l_path_substr_vector, l_range_vector);
                        if(l_s != HLX_STATUS_OK)
                        {
                                printf("HLX_STATUS_ERROR: Performing parse_path(%s)\n", l_p);
                                return HLX_STATUS_ERROR;
                        }
                }

                // If empty path explode
                if(l_range_vector.size())
                {
                        l_s = path_exploder(std::string(""), l_path_substr_vector, 0, l_range_vector, 0);
                        if(l_s != HLX_STATUS_OK)
                        {
                                printf("HLX_STATUS_ERROR: Performing explode_path(%s)\n", l_p);
                                return HLX_STATUS_ERROR;
                        }
                        // DEBUG show paths
                        //printf(" -- Displaying Paths --\n");
                        //uint32_t i_path_cnt = 0;
                        //for(path_vector_t::iterator i_path = g_path_vector.begin();
                        //              i_path != g_path_vector.end();
                        //              ++i_path, ++i_path_cnt)
                        //{
                        //      printf(": [%6d]: %s\n", i_path_cnt, i_path->c_str());
                        //}
                }
                else
                {
                        g_path_vector.push_back(a_path);
                }

                l_p = strtok_r(NULL, SPECIAL_EFX_OPT_SEPARATOR, &l_save_ptr);
        }
        else
        {
                g_path.clear();
        }

        // Options...
        while (l_p)
        {
                if(l_p)
                {
                        char l_options[1024];
                        char *l_options_save_ptr;
                        strncpy(l_options, l_p, 1024);
                        //printf("Options: %s\n", l_options);
                        char *l_k = strtok_r(l_options, SPECIAL_EFX_KV_SEPARATOR, &l_options_save_ptr);
                        char *l_v = strtok_r(NULL, SPECIAL_EFX_KV_SEPARATOR, &l_options_save_ptr);
                        int32_t l_add_option_status;
                        l_add_option_status = add_option(l_k, l_v);
                        if(l_add_option_status != HLX_STATUS_OK)
                        {
                                printf("HLX_STATUS_ERROR: Performing add_option(%s,%s)\n", l_k,l_v);
                                // Nuttin doing???

                        }
                        //printf("Key: %s\n", l_k);
                        //printf("Val: %s\n", l_v);
                }
                l_p = strtok_r(NULL, SPECIAL_EFX_OPT_SEPARATOR, &l_save_ptr);
        }
        //printf("a_path: %s\n", a_path.c_str());
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &get_path(void *a_rand)
{
        // TODO -make this threadsafe -mutex per???
        if(g_path_vector.size())
        {
                // Rollover..
                if(g_path_order == EXPLODED_PATH_ORDER_SEQUENTIAL)
                {
                        pthread_mutex_lock(&g_path_vector_mutex);
                        if(g_path_vector_last_idx >= g_path_vector.size())
                        {
                                g_path_vector_last_idx = 0;
                        }
                        uint32_t l_last_idx = g_path_vector_last_idx;
                        ++g_path_vector_last_idx;
                        pthread_mutex_unlock(&g_path_vector_mutex);
                        return g_path_vector[l_last_idx];
                }
                else
                {
                        uint32_t l_rand_idx = 0;
                        if(a_rand)
                        {
                                tinymt64_t *l_rand_ptr = (tinymt64_t*)a_rand;
                                l_rand_idx = (uint32_t)(tinymt64_generate_uint64(l_rand_ptr) % g_path_vector.size());
                        }
                        else
                        {
                                l_rand_idx = (random() * g_path_vector.size()) / RAND_MAX;
                        }
                        return g_path_vector[l_rand_idx];
                }
        }
        else
        {
                return g_path;
        }
}

//: ----------------------------------------------------------------------------
//: \details: create request callback
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t s_create_request(ns_hlx::subr &a_subr, ns_hlx::nbq &a_nbq)
{
        // TODO grab from path...
        std::string l_path_ref = get_path(g_rand_ptr);

        char l_buf[2048];
        int32_t l_len = 0;
        if(l_path_ref.empty())
        {
                l_path_ref = "/";
        }
        if(!(a_subr.get_query().empty()))
        {
                l_path_ref += "?";
                l_path_ref += a_subr.get_query();
        }
        //NDBG_PRINT("HOST: %s PATH: %s\n", a_reqlet.m_url.m_host.c_str(), l_path_ref.c_str());
        l_len = snprintf(l_buf, sizeof(l_buf),
                        "%s %.500s HTTP/1.1", a_subr.get_verb().c_str(), l_path_ref.c_str());

        ns_hlx::nbq_write_request_line(a_nbq, l_buf, l_len);

        // -------------------------------------------
        // Add repo headers
        // -------------------------------------------
        bool l_specd_host = false;

        // Loop over reqlet map
        for(ns_hlx::kv_map_list_t::const_iterator i_hl = a_subr.get_headers().begin();
            i_hl != a_subr.get_headers().end();
            ++i_hl)
        {
                if(i_hl->first.empty() || i_hl->second.empty())
                {
                        continue;
                }
                for(ns_hlx::str_list_t::const_iterator i_v = i_hl->second.begin();
                    i_v != i_hl->second.end();
                    ++i_v)
                {
                        ns_hlx::nbq_write_header(a_nbq, i_hl->first.c_str(), i_hl->first.length(), i_v->c_str(), i_v->length());
                        if (strcasecmp(i_hl->first.c_str(), "host") == 0)
                        {
                                l_specd_host = true;
                        }
                }
        }

        // -------------------------------------------
        // Default Host if unspecified
        // -------------------------------------------
        if (!l_specd_host)
        {
                ns_hlx::nbq_write_header(a_nbq, "Host", strlen("Host"),
                                         a_subr.get_host().c_str(), a_subr.get_host().length());
        }

        // -------------------------------------------
        // body
        // -------------------------------------------
        if(a_subr.get_body_data() && a_subr.get_body_len())
        {
                //NDBG_PRINT("Write: buf: %p len: %d\n", l_buf, l_len);
                ns_hlx::nbq_write_body(a_nbq, a_subr.get_body_data(), a_subr.get_body_len());
        }
        else
        {
                ns_hlx::nbq_write_body(a_nbq, NULL, 0);
        }

        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: Command
//: TODO Refactor
//: ----------------------------------------------------------------------------
#define NB_ENABLE  1
#define NB_DISABLE 0

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int kbhit()
{
        struct timeval l_tv;
        fd_set l_fds;
        l_tv.tv_sec = 0;
        l_tv.tv_usec = 0;
        FD_ZERO(&l_fds);
        FD_SET(STDIN_FILENO, &l_fds);
        //STDIN_FILENO is 0
        select(STDIN_FILENO + 1, &l_fds, NULL, NULL, &l_tv);
        return FD_ISSET(STDIN_FILENO, &l_fds);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nonblock(int state)
{
        struct termios ttystate;
        //get the terminal state
        tcgetattr(STDIN_FILENO, &ttystate);

        if (state == NB_ENABLE)
        {
                //turn off canonical mode
                ttystate.c_lflag &= ~ICANON;
                //minimum of number input read.
                ttystate.c_cc[VMIN] = 1;
        } else if (state == NB_DISABLE)
        {
                //turn on canonical mode
                ttystate.c_lflag |= ICANON;
        }
        //set the terminal attributes.
        tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void command_exec(void)
{
        int i = 0;
        char l_cmd = ' ';
        bool l_first_time = true;
        nonblock(NB_ENABLE);
        // ---------------------------------------
        // Loop forever until user quits
        // ---------------------------------------
        while (!g_test_finished)
        {
                i = kbhit();
                if (i != 0)
                {
                        l_cmd = fgetc(stdin);
                        switch (l_cmd)
                        {
                        // -------------------------------------------
                        // Quit
                        // -------------------------------------------
                        case 'q':
                        {
                                g_test_finished = true;
                                break;
                        }
                        // -------------------------------------------
                        // default
                        // -------------------------------------------
                        default:
                        {
                                break;
                        }
                        }
                }

                // TODO add define...
                usleep(g_interval_ms*1000);

                // Check for done
                if(g_run_time_ms != -1)
                {
                        int32_t l_time_delta_ms = (int32_t)(ns_hlx::get_delta_time_ms(g_start_time_ms));
                        if(l_time_delta_ms >= g_run_time_ms)
                        {
                                g_test_finished = true;
                        }
                }

                bool l_is_running = false;
                for (t_hurl_list_t::iterator i_t = g_t_hurl_list.begin();
                     i_t != g_t_hurl_list.end();
                     ++i_t)
                {
                        if(!(*i_t)->m_stopped) l_is_running = true;
                }
                if(!l_is_running)
                {
                        g_test_finished = true;
                }
                if(!g_quiet && !g_verbose)
                {
                        if(g_show_response_codes)
                        {
                                if(l_first_time)
                                {
                                        display_responses_line_desc();
                                        ns_hlx::t_stat_cntr_t l_total;
                                        ns_hlx::t_stat_calc_t l_total_calc;
                                        ns_hlx::t_stat_cntr_list_t l_thread;
                                        get_stat(l_total, l_total_calc, l_thread);
                                        if(!g_test_finished)
                                        {
                                                usleep(g_interval_ms*1000+100);
                                        }
                                        l_first_time = false;
                                }
                                display_responses_line();
                        }
                        else
                        {
                                if(l_first_time)
                                {
                                        display_results_line_desc();
                                        ns_hlx::t_stat_cntr_t l_total;
                                        ns_hlx::t_stat_calc_t l_total_calc;
                                        ns_hlx::t_stat_cntr_list_t l_thread;
                                        get_stat(l_total, l_total_calc, l_thread);
                                        if(!g_test_finished)
                                        {
                                                usleep(g_interval_ms*1000+100);
                                        }
                                        l_first_time = false;
                                }
                                display_results_line();
                        }
                }
        }
        nonblock(NB_DISABLE);
}

//: ----------------------------------------------------------------------------
//: \details: Print the version.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_version(FILE* a_stream, int a_exit_code)
{

        // print out the version information
        fprintf(a_stream, "hurl HTTP Load Tester.\n");
        fprintf(a_stream, "Copyright (C) 2016 Verizon Digital Media.\n");
        fprintf(a_stream, "               Version: %s\n", HLX_VERSION);
        exit(a_exit_code);
}

//: ----------------------------------------------------------------------------
//: \details: Print the command line help.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_usage(FILE* a_stream, int a_exit_code)
{
        fprintf(a_stream, "Usage: hurl [http[s]://]hostname[:port]/path [options]\n");
        fprintf(a_stream, "Options are:\n");
        fprintf(a_stream, "  -h, --help           Display this help and exit.\n");
        fprintf(a_stream, "  -V, --version        Display the version number and exit.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Input Options:\n");
        fprintf(a_stream, "  -w, --no_wildcards   Don't wildcard the url.\n");
        fprintf(a_stream, "  -M, --mode           Request mode -if multipath [random(default) | sequential].\n");
        fprintf(a_stream, "  -d, --data           HTTP body data -supports curl style @ file specifier\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Settings:\n");
        fprintf(a_stream, "  -p, --parallel       Num parallel. Default: 100.\n");
        fprintf(a_stream, "  -f, --fetches        Num fetches.\n");
        fprintf(a_stream, "  -N, --num_calls      Number of requests per connection\n");
        fprintf(a_stream, "  -t, --threads        Number of parallel threads. Default: 1\n");
        fprintf(a_stream, "  -H, --header         Request headers -can add multiple ie -H<> -H<>...\n");
        fprintf(a_stream, "  -X, --verb           Request command -HTTP verb to use -GET/PUT/etc. Default GET\n");
        fprintf(a_stream, "  -l, --seconds        Run for <N> seconds .\n");
        fprintf(a_stream, "  -A, --rate           Max Request Rate.\n");
        fprintf(a_stream, "  -T, --timeout        Timeout (seconds).\n");
        fprintf(a_stream, "  -x, --no_stats       Don't collect stats -faster.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Print Options:\n");
        fprintf(a_stream, "  -v, --verbose        Verbose logging\n");
        fprintf(a_stream, "  -c, --no_color       Turn off colors\n");
        fprintf(a_stream, "  -q, --quiet          Suppress progress output\n");
        fprintf(a_stream, "  -C, --responses      Display http(s) response codes instead of request statistics\n");
        fprintf(a_stream, "  -L, --responses_per  Display http(s) response codes per interval instead of request statistics\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Stat Options:\n");
        fprintf(a_stream, "  -U, --update         Update output every N ms. Default 500ms.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Results Options:\n");
        fprintf(a_stream, "  -j, --json           Display results in json\n");
        fprintf(a_stream, "  -Y, --http_load      Display results in http load mode -Legacy support\n");
        fprintf(a_stream, "  -Z, --http_load_line Display results in http load mode on a single line -Legacy support\n");
        fprintf(a_stream, "  -o, --output         Output results to file <FILE> -default to stdout\n");
        fprintf(a_stream, "  \n");
#ifdef ENABLE_PROFILER
        fprintf(a_stream, "Profile Options:\n");
        fprintf(a_stream, "  -G, --gprofile       Google profiler output file\n");
        fprintf(a_stream, "  \n");
#endif
        fprintf(a_stream, "Note: If running long jobs consider enabling tcp_tw_reuse -eg:\n");
        fprintf(a_stream, "echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse\n");
        fprintf(a_stream, "\n");
        exit(a_exit_code);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int main(int argc, char** argv)
{
        results_scheme_t l_results_scheme = RESULTS_SCHEME_STD;

        // TODO Make default definitions
        bool l_input_flag = false;
        bool l_wildcarding = true;
        std::string l_output_file = "";

        // Suppress errors
        ns_hlx::trc_log_level_set(ns_hlx::TRC_LOG_LEVEL_NONE);

        // Log all packets
        //ns_hlx::trc_log_level_set(ns_hlx::TRC_LOG_LEVEL_ALL);
        //ns_hlx::trc_out_file_open("/dev/stdout");

        if(isatty(fileno(stdout)))
        {
                g_color = true;
        }

        // -------------------------------------------------
        // Subrequest settings
        // -------------------------------------------------
        ns_hlx::subr *l_subr = new ns_hlx::subr();
        l_subr->set_uid(get_next_subr_uuid());
        l_subr->set_save(false);

        // Default headers
        l_subr->set_header("User-Agent","hurl Server Load Tester");
        l_subr->set_header("Accept","*/*");
        l_subr->set_keepalive(true);
        //l_subr->set_num_reqs_per_conn(1);

        // Initialize rand...
        g_rand_ptr = (tinymt64_t*)calloc(1, sizeof(tinymt64_t));
        tinymt64_init(g_rand_ptr, ns_hlx::get_time_us());

        // Initialize mutex for sequential path requesting
        pthread_mutex_init(&g_path_vector_mutex, NULL);

        // Completion
        pthread_mutex_init(&g_completion_mutex, NULL);

        // -------------------------------------------
        // Get args...
        // -------------------------------------------
        char l_opt;
        std::string l_arg;
        int l_option_index = 0;
        struct option l_long_options[] =
                {
                { "help",           0, 0, 'h' },
                { "version",        0, 0, 'V' },
                { "no_wildcards",   0, 0, 'w' },
                { "data",           1, 0, 'd' },
                { "parallel",       1, 0, 'p' },
                { "fetches",        1, 0, 'f' },
                { "num_calls",      1, 0, 'N' },
                { "threads",        1, 0, 't' },
                { "header",         1, 0, 'H' },
                { "verb",           1, 0, 'X' },
                { "rate",           1, 0, 'A' },
                { "mode",           1, 0, 'M' },
                { "seconds",        1, 0, 'l' },
                { "timeout",        1, 0, 'T' },
                { "no_stats",       0, 0, 'x' },
                { "verbose",        0, 0, 'v' },
                { "no_color",       0, 0, 'c' },
                { "quiet",          0, 0, 'q' },
                { "responses",      0, 0, 'C' },
                { "responses_per",  0, 0, 'L' },
                { "json",           0, 0, 'j' },
                { "http_load",      0, 0, 'Y' },
                { "http_load_line", 0, 0, 'Z' },
                { "output",         1, 0, 'o' },
                { "update",         1, 0, 'U' },
#ifdef ENABLE_PROFILER
                { "gprofile",       1, 0, 'G' },
#endif
                // list sentinel
                { 0, 0, 0, 0 }
        };

        // -------------------------------------------
        // Assume unspecified arg url...
        // TODO Unsure if good way to allow unspecified
        // arg...
        // -------------------------------------------
        std::string l_url;
#ifdef ENABLE_PROFILER
        std::string l_gprof_file;
#endif
        bool is_opt = false;

        for(int i_arg = 1; i_arg < argc; ++i_arg) {

                if(argv[i_arg][0] == '-') {
                        // next arg is for the option
                        is_opt = true;
                }
                else if(argv[i_arg][0] != '-' && is_opt == false) {
                        // Stuff in url field...
                        l_url = std::string(argv[i_arg]);
                        //if(g_verbose)
                        //{
                        //      NDBG_PRINT("Found unspecified argument: %s --assuming url...\n", l_url.c_str());
                        //}
                        l_input_flag = true;
                        break;
                } else {
                        // reset option flag
                        is_opt = false;
                }

        }

#ifdef ENABLE_PROFILER
        char l_short_arg_list[] = "hVwd:p:f:N:t:H:X:A:M:l:T:xvcqCLjYZo:U:G:";
#else
        char l_short_arg_list[] = "hVwd:p:f:N:t:H:X:A:M:l:T:xvcqCLjYZo:U:";
#endif

        while ((l_opt = getopt_long_only(argc, argv, l_short_arg_list, l_long_options, &l_option_index)) != -1 && ((unsigned char)l_opt != 255))
        {

                if (optarg)
                        l_arg = std::string(optarg);
                else
                        l_arg.clear();
                //NDBG_PRINT("arg[%c=%d]: %s\n", l_opt, l_option_index, l_arg.c_str());

                switch (l_opt)
                {
                // ---------------------------------------
                // Help
                // ---------------------------------------
                case 'h':
                {
                        print_usage(stdout, 0);
                        break;
                }
                // ---------------------------------------
                // Version
                // ---------------------------------------
                case 'V':
                {
                        print_version(stdout, 0);
                        break;
                }
                // ---------------------------------------
                // Wildcarding
                // ---------------------------------------
                case 'w':
                {
                        l_wildcarding = false;
                        break;
                }
                // ---------------------------------------
                // Data
                // ---------------------------------------
                case 'd':
                {
                        // TODO Size limits???
                        int32_t l_s;
                        // If a_data starts with @ assume file
                        if(l_arg[0] == '@')
                        {
                                char *l_buf;
                                uint32_t l_len;
                                l_s = read_file(l_arg.data() + 1, &(l_buf), &(l_len));
                                if(l_s != 0)
                                {
                                        printf("Error reading body data from file: %s\n", l_arg.c_str() + 1);
                                        return HLX_STATUS_ERROR;
                                }
                                l_subr->set_body_data(l_buf, l_len);
                        }
                        else
                        {
                                char *l_buf;
                                uint32_t l_len;
                                l_len = l_arg.length() + 1;
                                l_buf = (char *)malloc(sizeof(char)*l_len);
                                l_subr->set_body_data(l_buf, l_len);
                        }

                        // Add content length
                        char l_len_str[64];
                        sprintf(l_len_str, "%u", l_subr->get_body_len());
                        l_subr->set_header("Content-Length", l_len_str);
                        break;
                }
#ifdef ENABLE_PROFILER
                // ---------------------------------------
                // Google Profiler Output File
                // ---------------------------------------
                case 'G':
                {
                        l_gprof_file = l_arg;
                        break;
                }
#endif
                // ---------------------------------------
                // cipher
                // ---------------------------------------
                case 'y':
                {
                        g_cipher_str_list = l_arg;
                        break;
                }
                // ---------------------------------------
                // parallel
                // ---------------------------------------
                case 'p':
                {
                        int32_t l_num_parallel = atoi(optarg);
                        if (l_num_parallel < 1)
                        {
                                printf("Error num parallel must be at least 1\n");
                                return HLX_STATUS_ERROR;
                        }
                        g_num_parallel = l_num_parallel;
                        break;
                }
                // ---------------------------------------
                // fetches
                // ---------------------------------------
                case 'f':
                {
                        int32_t l_end_fetches = atoi(optarg);
                        if (l_end_fetches < 1)
                        {
                                printf("Error fetches must be at least 1\n");
                                return HLX_STATUS_ERROR;
                        }
                        g_num_to_request = l_end_fetches;
                        break;
                }
                // ---------------------------------------
                // number of calls per connection
                // ---------------------------------------
                case 'N':
                {
                        int l_val = atoi(optarg);
                        if(l_val < 1)
                        {
                                printf("Error num-calls must be at least 1");
                                return HLX_STATUS_ERROR;
                        }
                        g_reqs_per_conn = l_val;
                        if (g_reqs_per_conn == 1)
                        {
                                l_subr->set_keepalive(false);
                        }
                        break;
                }
                // ---------------------------------------
                // num threads
                // ---------------------------------------
                case 't':
                {
                        //NDBG_PRINT("arg: --threads: %s\n", l_arg.c_str());
                        int l_val = atoi(optarg);
                        if (l_val < 0)
                        {
                                printf("Error num-threads must be 0 or greater\n");
                                return HLX_STATUS_ERROR;
                        }
                        g_num_threads = l_val;
                        break;
                }
                // ---------------------------------------
                // Header
                // ---------------------------------------
                case 'H':
                {
                        int32_t l_s;
                        std::string l_key;
                        std::string l_val;
                        l_s = ns_hlx::break_header_string(l_arg, l_key, l_val);
                        if (l_s != 0)
                        {
                                printf("Error breaking header string: %s -not in <HEADER>:<VAL> format?\n", l_arg.c_str());
                                return HLX_STATUS_ERROR;
                        }
                        l_s = l_subr->set_header(l_key, l_val);
                        if (l_s != 0)
                        {
                                printf("Error performing set_header: %s\n", l_arg.c_str());
                                return HLX_STATUS_ERROR;
                        }
                        break;
                }
                // ---------------------------------------
                // Verb
                // ---------------------------------------
                case 'X':
                {
                        if(l_arg.length() > 64)
                        {
                                printf("Error verb string: %s too large try < 64 chars\n", l_arg.c_str());
                                return HLX_STATUS_ERROR;
                        }
                        l_subr->set_verb(l_arg);
                        break;
                }
                // ---------------------------------------
                // rate
                // ---------------------------------------
                case 'A':
                {
                        int l_rate = atoi(optarg);
                        if (l_rate < 1)
                        {
                                printf("Error: rate must be at least 1\n");
                                //print_usage(stdout, -1);
                                return HLX_STATUS_ERROR;
                        }
                        g_rate_delta_us = 1000000 / l_rate;
                        break;
                }
                // ---------------------------------------
                // Mode
                // ---------------------------------------
                case 'M':
                {
                        std::string l_order = optarg;
                        if(l_order == "sequential")
                        {
                                g_path_order = EXPLODED_PATH_ORDER_SEQUENTIAL;
                        }
                        else if(l_order == "random")
                        {
                                g_path_order = EXPLODED_PATH_ORDER_RANDOM;
                        }
                        else
                        {
                                printf("Error: Mode must be [roundrobin|sequential|random]\n");
                                //print_usage(stdout, -1);
                                return HLX_STATUS_ERROR;
                        }
                        break;
                }
                // ---------------------------------------
                // seconds
                // ---------------------------------------
                case 'l':
                {
                        int l_run_time_s = atoi(optarg);
                        if (l_run_time_s < 1)
                        {
                                printf("Error: seconds must be at least 1\n");
                                //print_usage(stdout, -1);
                                return HLX_STATUS_ERROR;
                        }
                        g_run_time_ms = l_run_time_s*1000;
                        break;
                }
                // ---------------------------------------
                // timeout
                // ---------------------------------------
                case 'T':
                {
                        //NDBG_PRINT("arg: --fetches: %s\n", optarg);
                        //g_end_type = END_FETCHES;
                        int l_subreq_timeout_s = atoi(optarg);
                        if (l_subreq_timeout_s < 1)
                        {
                                printf("timeout must be > 0\n");
                                //print_usage(stdout, -1);
                                return HLX_STATUS_ERROR;
                        }
                        l_subr->set_timeout_ms(l_subreq_timeout_s*1000);
                        break;
                }
                // ---------------------------------------
                // No stats
                // ---------------------------------------
                case 'x':
                {
                        g_stats = false;
                        break;
                }
                // ---------------------------------------
                // verbose
                // ---------------------------------------
                case 'v':
                {
                        g_verbose = true;
                        l_subr->set_save(true);
                        break;
                }
                // ---------------------------------------
                // color
                // ---------------------------------------
                case 'c':
                {
                        g_color = false;
                        break;
                }
                // ---------------------------------------
                // quiet
                // ---------------------------------------
                case 'q':
                {
                        g_quiet = true;
                        break;
                }
                // ---------------------------------------
                // responses
                // ---------------------------------------
                case 'C':
                {
                        g_show_response_codes = true;
                        break;
                }
                // ---------------------------------------
                // per_interval
                // ---------------------------------------
                case 'L':
                {
                        g_show_response_codes = true;
                        g_show_per_interval = true;
                        break;
                }
                // ---------------------------------------
                // http_load
                // ---------------------------------------
                case 'Y':
                {
                        l_results_scheme = RESULTS_SCHEME_HTTP_LOAD;
                        break;
                }
                // ---------------------------------------
                // http_load_line
                // ---------------------------------------
                case 'Z':
                {
                        l_results_scheme = RESULTS_SCHEME_HTTP_LOAD_LINE;
                        break;
                }
                // ---------------------------------------
                // json
                // ---------------------------------------
                case 'j':
                {
                        l_results_scheme = RESULTS_SCHEME_JSON;
                        break;
                }
                // ---------------------------------------
                // output
                // ---------------------------------------
                case 'o':
                {
                        l_output_file = optarg;
                        break;
                }
                // ---------------------------------------
                // Update interval
                // ---------------------------------------
                case 'U':
                {
                        int l_interval_ms = atoi(optarg);
                        if (l_interval_ms < 1)
                        {
                                printf("Error: Update interval must be > 0 ms\n");
                                //print_usage(stdout, -1);
                                return HLX_STATUS_ERROR;
                        }
                        g_interval_ms = l_interval_ms;
                        break;
                }
                // ---------------------------------------
                // What???
                // ---------------------------------------
                case '?':
                {
                        // Required argument was missing
                        // '?' is provided when the 3rd arg to getopt_long does not begin with a ':', and is preceeded
                        // by an automatic error message.
                        fprintf(stdout, "  Exiting.\n");
                        print_usage(stdout, -1);
                        break;
                }
                // ---------------------------------------
                // Huh???
                // ---------------------------------------
                default:
                {
                        fprintf(stdout, "Unrecognized option.\n");
                        print_usage(stdout, -1);
                        break;
                }
                }
        }

        // Verify input
        if(!l_input_flag)
        {
                fprintf(stdout, "Error: Input url/url file/playback file required.");
                print_usage(stdout, -1);
        }

        // -------------------------------------------
        // Add url from command line
        // -------------------------------------------
        if(!l_url.length())
        {
                fprintf(stdout, "Error: No specified URL on cmd line.\n");
                print_usage(stdout, -1);
        }

        // -------------------------------------------------
        // Get resource limits
        // -------------------------------------------------
        int32_t l_s;
        struct rlimit l_rlim;
        errno = 0;
        l_s = getrlimit(RLIMIT_NOFILE, &l_rlim);
        if(l_s != 0)
        {
                fprintf(stdout, "Error performing getrlimit. Reason: %s\n", strerror(errno));
                return HLX_STATUS_ERROR;
        }
        if(l_rlim.rlim_cur < (uint64_t)(g_num_threads*g_num_parallel))
        {
                fprintf(stdout, "Error threads[%d]*parallelism[%d] > process fd resource limit[%u]\n",
                                g_num_threads, g_num_parallel, (uint32_t)l_rlim.rlim_cur);
                return HLX_STATUS_ERROR;
        }

        // -------------------------------------------
        // Sigint handler3
        // -------------------------------------------
        if (signal(SIGINT, sig_handler) == SIG_ERR)
        {
                printf("Error: can't catch SIGINT\n");
                return HLX_STATUS_ERROR;
        }

        // -------------------------------------------
        // Add url from command line
        // -------------------------------------------
        //printf("Adding url: %s\n", l_url.c_str());
        // Set url
        l_s = l_subr->init_with_url(l_url);
        if(l_s != 0)
        {
                printf("Error: performing init_with_url: %s\n", l_url.c_str());
                return HLX_STATUS_ERROR;
        }
        l_subr->set_completion_cb(s_completion_cb);

        // -------------------------------------------
        // Paths...
        // -------------------------------------------
        std::string l_raw_path = l_subr->get_path();
        //printf("l_raw_path: %s\n",l_raw_path.c_str());
        if(l_wildcarding)
        {
                int32_t l_s;
                l_s = special_effects_parse(l_raw_path);
                if(l_s != HLX_STATUS_OK)
                {
                        printf("Error performing special_effects_parse with path: %s\n", l_raw_path.c_str());
                        return HLX_STATUS_ERROR;
                }
                if(g_path_vector.size() > 1)
                {
                        g_multipath = true;
                }
        }
        else
        {
                g_path = l_raw_path;
        }

#ifdef ENABLE_PROFILER
        // Start Profiler
        if (!l_gprof_file.empty())
        {
                ProfilerStart(l_gprof_file.c_str());
        }
#endif

        // Message
        if(!g_quiet && !g_verbose)
        {
                if(g_reqs_per_conn < 0)
                {
                        fprintf(stdout, "Running %d threads %d parallel connections per thread with infinite requests per connection\n",
                                g_num_threads, g_num_parallel);
                }
                else
                {
                        fprintf(stdout, "Running %d threads %d parallel connections per thread with %ld requests per connection\n",
                                        g_num_threads, g_num_parallel, g_reqs_per_conn);
                }
        }

        // -------------------------------------------
        // resolve
        // -------------------------------------------
        ns_hlx::host_info l_host_info;
        l_s = ns_hlx::nlookup(l_subr->get_host(), l_subr->get_port(), l_host_info);
        if(l_s != HLX_STATUS_OK)
        {
                printf("Error: resolving: %s:%d\n", l_subr->get_host().c_str(), l_subr->get_port());
                return HLX_STATUS_ERROR;
        }
        l_subr->set_host_info(l_host_info);

        // -------------------------------------------
        // Init
        // -------------------------------------------
        for(uint32_t i_t = 0; i_t < g_num_threads; ++i_t)
        {
                // Calculate num to request
                int32_t l_num_to_request = -1;
                if(g_num_to_request > 0)
                {
                        // first thread gets remainder
                        l_num_to_request = g_num_to_request / g_num_threads;
                        if(i_t == 0)
                        {
                                l_num_to_request += g_num_to_request % g_num_threads;
                        }
                }
                t_hurl *l_t_hurl = new t_hurl(*l_subr, g_num_parallel, l_num_to_request);
                g_t_hurl_list.push_back(l_t_hurl);
                l_t_hurl->init();
                // TODO Check status
        }

        // -------------------------------------------
        // Run
        // -------------------------------------------
        g_start_time_ms = ns_hlx::get_time_ms();;
        for(t_hurl_list_t::iterator i_t = g_t_hurl_list.begin();
            i_t != g_t_hurl_list.end();
            ++i_t)
        {
                (*i_t)->run();
                // TODO Check status
        }

        // -------------------------------------------
        // Run command exec
        // -------------------------------------------
        command_exec();
        uint64_t l_end_time_ms = ns_hlx::get_time_ms() - g_start_time_ms;

        // -------------------------------------------
        // Stop
        // -------------------------------------------
        for(t_hurl_list_t::iterator i_t = g_t_hurl_list.begin();
            i_t != g_t_hurl_list.end();
            ++i_t)
        {
                (*i_t)->stop();
        }

        // -------------------------------------------
        // Join all threads before exit
        // -------------------------------------------
        for(t_hurl_list_t::iterator i_t = g_t_hurl_list.begin();
            i_t != g_t_hurl_list.end();
            ++i_t)
        {
                pthread_join(((*i_t)->m_t_run_thread), NULL);
        }

#ifdef ENABLE_PROFILER
        if (!l_gprof_file.empty())
        {
                ProfilerStop();
        }
#endif

        std::string l_out_str;
        switch(l_results_scheme)
        {
        case RESULTS_SCHEME_STD:
        {
                get_results(((double)l_end_time_ms)/1000.0, l_out_str);
                break;
        }
        case RESULTS_SCHEME_JSON:
        {
                get_results_json(((double)l_end_time_ms)/1000.0, l_out_str);
                break;
        }
        case RESULTS_SCHEME_HTTP_LOAD:
        {
                get_results_http_load(((double)l_end_time_ms)/1000.0, l_out_str, false);
                break;
        }
        case RESULTS_SCHEME_HTTP_LOAD_LINE:
        {
                get_results_http_load(((double)l_end_time_ms)/1000.0, l_out_str, true);
                break;
        }
        default:
        {
                get_results(((double)l_end_time_ms)/1000.0, l_out_str);
                break;
        }
        }

        // -------------------------------------------
        // Write results...
        // -------------------------------------------
        if(l_output_file.empty())
        {
                if(!g_verbose)
                {
                        printf("%s\n", l_out_str.c_str());
                }
        }
        else
        {
                int32_t l_num_bytes_written = 0;
                int32_t l_s = 0;
                // Open
                FILE *l_file_ptr = fopen(l_output_file.c_str(), "w+");
                if(l_file_ptr == NULL)
                {
                        printf("Error performing fopen. Reason: %s\n", strerror(errno));
                        return HLX_STATUS_ERROR;
                }

                // Write
                l_num_bytes_written = fwrite(l_out_str.c_str(), 1, l_out_str.length(), l_file_ptr);
                if(l_num_bytes_written != (int32_t)l_out_str.length())
                {
                        printf("Error performing fwrite. Reason: %s\n", strerror(errno));
                        fclose(l_file_ptr);
                        return HLX_STATUS_ERROR;
                }

                // Close
                l_s = fclose(l_file_ptr);
                if(l_s != 0)
                {
                        printf("Error performing fclose. Reason: %s\n", strerror(errno));
                        return HLX_STATUS_ERROR;
                }
        }

        // -------------------------------------------
        // Cleanup...
        // -------------------------------------------
        for(t_hurl_list_t::iterator i_t = g_t_hurl_list.begin();
            i_t != g_t_hurl_list.end();
            ++i_t)
        {
                if(*i_t)
                {
                        delete *i_t;
                        *i_t = NULL;
                }
        }
        g_t_hurl_list.clear();
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void get_stat(ns_hlx::t_stat_cntr_t &ao_total,
              ns_hlx::t_stat_calc_t &ao_total_calc,
              ns_hlx::t_stat_cntr_list_t &ao_thread)
{
        // ---------------------------------------
        // store last values -for calc'd stats
        // ---------------------------------------
        static ns_hlx::t_stat_cntr_t s_last;
        static uint64_t s_stat_last_time_ms = 0;

        uint64_t l_cur_time_ms = ns_hlx::get_time_ms();
        uint64_t l_delta_ms = l_cur_time_ms - s_stat_last_time_ms;

        // ---------------------------------------
        // Aggregate
        // ---------------------------------------
        ao_total.clear();
        for(t_hurl_list_t::const_iterator i_t = g_t_hurl_list.begin();
            i_t != g_t_hurl_list.end();
           ++i_t)
        {
                ao_total.m_upsv_bytes_read += (*i_t)->m_stat.m_upsv_bytes_read;
                ao_total.m_upsv_bytes_written += (*i_t)->m_stat.m_upsv_bytes_written;
                ao_total.m_upsv_reqs += (*i_t)->m_stat.m_upsv_reqs;
                ao_total.m_upsv_resp += (*i_t)->m_stat.m_upsv_resp;
                ao_total.m_upsv_errors += (*i_t)->m_stat.m_upsv_errors;

                // TODO
                ao_total.m_upsv_idle_killed += 0;

                ao_total.m_upsv_resp_status_2xx = (*i_t)->m_stat.m_upsv_resp_status_2xx;
                ao_total.m_upsv_resp_status_3xx = (*i_t)->m_stat.m_upsv_resp_status_3xx;
                ao_total.m_upsv_resp_status_4xx = (*i_t)->m_stat.m_upsv_resp_status_4xx;
                ao_total.m_upsv_resp_status_5xx = (*i_t)->m_stat.m_upsv_resp_status_5xx;

                add_stat(ao_total.m_upsv_stat_us_connect , (*i_t)->m_stat.m_upsv_stat_us_connect);
                add_stat(ao_total.m_upsv_stat_us_first_response , (*i_t)->m_stat.m_upsv_stat_us_first_response);
                add_stat(ao_total.m_upsv_stat_us_end_to_end , (*i_t)->m_stat.m_upsv_stat_us_end_to_end);
        }

        // ---------------------------------------
        // calc'd stats
        // ---------------------------------------
        uint64_t l_delta_reqs = ao_total.m_upsv_reqs - s_last.m_upsv_reqs;
        uint64_t l_delta_resp = ao_total.m_upsv_resp - s_last.m_upsv_resp;
        //NDBG_PRINT("l_delta_resp: %lu\n", l_delta_resp);
        if(l_delta_resp > 0)
        {
                ao_total_calc.m_upsv_resp_status_2xx_pcnt = 100.0*((float)(ao_total.m_upsv_resp_status_2xx - s_last.m_upsv_resp_status_2xx))/((float)l_delta_resp);
                ao_total_calc.m_upsv_resp_status_3xx_pcnt = 100.0*((float)(ao_total.m_upsv_resp_status_3xx - s_last.m_upsv_resp_status_3xx))/((float)l_delta_resp);
                ao_total_calc.m_upsv_resp_status_4xx_pcnt = 100.0*((float)(ao_total.m_upsv_resp_status_4xx - s_last.m_upsv_resp_status_4xx))/((float)l_delta_resp);
                ao_total_calc.m_upsv_resp_status_5xx_pcnt = 100.0*((float)(ao_total.m_upsv_resp_status_5xx - s_last.m_upsv_resp_status_5xx))/((float)l_delta_resp);
        }
        if(l_delta_ms > 0)
        {
                ao_total_calc.m_upsv_req_s = ((float)l_delta_reqs*1000)/((float)l_delta_ms);
                ao_total_calc.m_upsv_resp_s = ((float)l_delta_reqs*1000)/((float)l_delta_ms);
                ao_total_calc.m_upsv_bytes_read_s = ((float)((ao_total.m_upsv_bytes_read - s_last.m_upsv_bytes_read)*1000))/((float)l_delta_ms);
                ao_total_calc.m_upsv_bytes_write_s = ((float)((ao_total.m_upsv_bytes_written - s_last.m_upsv_bytes_written)*1000))/((float)l_delta_ms);
        }

        // -------------------------------------------------
        // copy
        // -------------------------------------------------
        s_last = ao_total;
        s_stat_last_time_ms = l_cur_time_ms;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void display_responses_line_desc(void)
{
        printf("+-----------+-------------+-----------+-----------+-----------+-----------+-----------+-----------+\n");
        if(g_show_per_interval)
        {
                if(g_color)
                {
                printf("| %s%9s%s / %s%11s%s / %s%9s%s / %s%9s%s / %s%9s%s | %s%9s%s | %s%9s%s | %s%9s%s | \n",
                                ANSI_COLOR_FG_WHITE, "Elapsed", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_WHITE, "Rsp/s", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_WHITE, "Cmpltd", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_WHITE, "Errors", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_GREEN, "200s %%", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_CYAN, "300s %%", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_MAGENTA, "400s %%", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_RED, "500s %%", ANSI_COLOR_OFF);
                }
                else
                {
                        printf("| %9s / %11s / %9s / %9s | %9s | %9s | %9s | %9s | \n",
                                        "Elapsed",
                                        "Req/s",
                                        "Cmpltd",
                                        "Errors",
                                        "200s %%",
                                        "300s %%",
                                        "400s %%",
                                        "500s %%");
                }
        }
        else
        {
                if(g_color)
                {
                printf("| %s%9s%s / %s%11s%s / %s%9s%s / %s%9s%s / %s%9s%s | %s%9s%s | %s%9s%s | %s%9s%s | \n",
                                ANSI_COLOR_FG_WHITE, "Elapsed", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_WHITE, "Req/s", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_WHITE, "Cmpltd", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_WHITE, "Errors", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_GREEN, "200s", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_CYAN, "300s", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_MAGENTA, "400s", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_RED, "500s", ANSI_COLOR_OFF);
                }
                else
                {
                        printf("| %9s / %11s / %9s / %9s | %9s | %9s | %9s | %9s | \n",
                                        "Elapsed",
                                        "Req/s",
                                        "Cmpltd",
                                        "Errors",
                                        "200s",
                                        "300s",
                                        "400s",
                                        "500s");
                }
        }
        printf("+-----------+-------------+-----------+-----------+-----------+-----------+-----------+-----------+\n");
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void get_status_codes(ns_hlx::status_code_count_map_t &ao_map)
{
        for(t_hurl_list_t::iterator i_t = g_t_hurl_list.begin();
            i_t != g_t_hurl_list.end();
            ++i_t)
        {
                ns_hlx::status_code_count_map_t i_m = (*i_t)->m_status_code_count_map;
                for(ns_hlx::status_code_count_map_t::iterator i_c = i_m.begin();
                    i_c != i_m.end();
                    ++i_c)
                {
                        ao_map[i_c->first] += i_c->second;
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void display_responses_line(void)
{
        // Get stats
        ns_hlx::t_stat_cntr_t l_total;
        ns_hlx::t_stat_calc_t l_total_calc;
        ns_hlx::t_stat_cntr_list_t l_thread;
        get_stat(l_total, l_total_calc, l_thread);
        if(g_show_per_interval)
        {
                if(g_color)
                {
                                printf("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %s%9.2f%s | %s%9.2f%s | %s%9.2f%s | %s%9.2f%s |\n",
                                                ((double)(ns_hlx::get_delta_time_ms(g_start_time_ms))) / 1000.0,
                                                l_total_calc.m_upsv_resp_s,
                                                l_total.m_upsv_resp,
                                                l_total.m_upsv_errors,
                                                ANSI_COLOR_FG_GREEN, l_total_calc.m_upsv_resp_status_2xx_pcnt, ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_CYAN, l_total_calc.m_upsv_resp_status_3xx_pcnt, ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_MAGENTA, l_total_calc.m_upsv_resp_status_4xx_pcnt, ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_RED, l_total_calc.m_upsv_resp_status_5xx_pcnt, ANSI_COLOR_OFF);
                }
                else
                {
                        printf("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %9.2f | %9.2f | %9.2f | %9.2f |\n",
                                        ((double)(ns_hlx::get_delta_time_ms(g_start_time_ms))) / 1000.0,
                                        l_total_calc.m_upsv_resp_s,
                                        l_total.m_upsv_resp,
                                        l_total.m_upsv_errors,
                                        l_total_calc.m_upsv_resp_status_2xx_pcnt,
                                        l_total_calc.m_upsv_resp_status_3xx_pcnt,
                                        l_total_calc.m_upsv_resp_status_4xx_pcnt,
                                        l_total_calc.m_upsv_resp_status_5xx_pcnt);
                }
        }
        else
        {
                // Aggregate over status code map
                uint32_t l_responses[10] = {0};
                ns_hlx::status_code_count_map_t l_status_code_count_map;
                get_status_codes(l_status_code_count_map);
                for(ns_hlx::status_code_count_map_t::iterator i_code = l_status_code_count_map.begin();
                    i_code != l_status_code_count_map.end();
                    ++i_code)
                {
                        if(i_code->first >= 200 && i_code->first <= 299)
                        {
                                l_responses[2] += i_code->second;
                        }
                        else if(i_code->first >= 300 && i_code->first <= 399)
                        {
                                l_responses[3] += i_code->second;
                        }
                        else if(i_code->first >= 400 && i_code->first <= 499)
                        {
                                l_responses[4] += i_code->second;
                        }
                        else if(i_code->first >= 500 && i_code->first <= 599)
                        {
                                l_responses[5] += i_code->second;
                        }
                }
                if(g_color)
                {
                                printf("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %s%9u%s | %s%9u%s | %s%9u%s | %s%9u%s |\n",
                                                ((double)(ns_hlx::get_delta_time_ms(g_start_time_ms))) / 1000.0,
                                                l_total_calc.m_upsv_resp_s,
                                                l_total.m_upsv_resp,
                                                l_total.m_upsv_errors,
                                                ANSI_COLOR_FG_GREEN, l_responses[2], ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_CYAN, l_responses[3], ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_MAGENTA, l_responses[4], ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_RED, l_responses[5], ANSI_COLOR_OFF);
                }
                else
                {
                        printf("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %9u | %9u | %9u | %9u |\n",
                                        ((double)(ns_hlx::get_delta_time_ms(g_start_time_ms))) / 1000.0,
                                        l_total_calc.m_upsv_resp_s,
                                        l_total.m_upsv_resp,
                                        l_total.m_upsv_errors,
                                        l_responses[2],
                                        l_responses[3],
                                        l_responses[4],
                                        l_responses[5]);
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void display_results_line_desc(void)
{
        printf("+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+\n");
        if(g_color)
        {
        printf("| %s%9s%s / %s%9s%s | %s%9s%s | %s%9s%s | %s%12s%s | %9s | %11s | %9s |\n",
                        ANSI_COLOR_FG_GREEN, "Completed", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_BLUE, "Requested", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_MAGENTA, "IdlKil", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_RED, "Errors", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_YELLOW, "kBytes Recvd", ANSI_COLOR_OFF,
                        "Elapsed",
                        "Req/s",
                        "MB/s");
        }
        else
        {
                printf("| %9s / %9s | %9s | %9s | %12s | %9s | %11s | %9s |\n",
                                "Completed",
                                "Requested",
                                "IdlKil",
                                "Errors",
                                "kBytes Recvd",
                                "Elapsed",
                                "Req/s",
                                "MB/s");
        }
        printf("+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+\n");
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void display_results_line(void)
{
        // Get stats
        ns_hlx::t_stat_cntr_t l_total;
        ns_hlx::t_stat_calc_t l_total_calc;
        ns_hlx::t_stat_cntr_list_t l_thread;
        get_stat(l_total, l_total_calc, l_thread);
        if(g_color)
        {
                        printf("| %s%9" PRIu64 "%s / %s%9" PRIi64 "%s | %s%9" PRIu64 "%s | %s%9" PRIu64 "%s | %s%12.2f%s | %8.2fs | %10.2fs | %8.2fs |\n",
                                        ANSI_COLOR_FG_GREEN, l_total.m_upsv_resp, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_BLUE, l_total.m_upsv_reqs, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_MAGENTA, l_total.m_upsv_idle_killed, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_RED, l_total.m_upsv_errors, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_YELLOW, l_total_calc.m_upsv_bytes_read_s/1024.0, ANSI_COLOR_OFF,
                                        ((double)(ns_hlx::get_delta_time_ms(g_start_time_ms))) / 1000.0,
                                        l_total_calc.m_upsv_req_s,
                                        l_total_calc.m_upsv_bytes_read_s/(1024.0*1024.0)
                                        );
        }
        else
        {
                printf("| %9" PRIu64 " / %9" PRIi64 " | %9" PRIu64 " | %9" PRIu64 " | %12.2f | %8.2fs | %10.2fs | %8.2fs |\n",
                                l_total.m_upsv_resp,
                                l_total.m_upsv_reqs,
                                l_total.m_upsv_idle_killed,
                                l_total.m_upsv_errors,
                                ((double)(l_total.m_upsv_bytes_read))/(1024.0),
                                ((double)(ns_hlx::get_delta_time_ms(g_start_time_ms)) / 1000.0),
                                l_total_calc.m_upsv_req_s,
                                l_total_calc.m_upsv_bytes_read_s/(1024.0*1024.0)
                                );
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define STR_PRINT(...) \
        do { \
                snprintf(l_buf,1024,__VA_ARGS__);\
                ao_results+=l_buf;\
        } while(0)

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void get_results(double a_elapsed_time,
                 std::string &ao_results)
{
        // Get stats
        ns_hlx::t_stat_cntr_t l_total;
        ns_hlx::t_stat_calc_t l_total_calc;
        ns_hlx::t_stat_cntr_list_t l_thread;
        get_stat(l_total, l_total_calc, l_thread);
        std::string l_tag;
        char l_buf[1024];
        // TODO Fix elapse and max parallel
        l_tag = "ALL";
        uint64_t l_total_bytes = l_total.m_upsv_bytes_read + l_total.m_upsv_bytes_written;
        if(g_color)
        {
        STR_PRINT("| %sRESULTS%s:             %s%s%s\n", ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF, ANSI_COLOR_FG_YELLOW, l_tag.c_str(), ANSI_COLOR_OFF);
        }
        else
        {
        STR_PRINT("| RESULTS:             %s\n", l_tag.c_str());
        }
        STR_PRINT("| fetches:             %" PRIu64 "\n", l_total.m_upsv_resp);
        STR_PRINT("| max parallel:        %u\n", g_num_parallel);
        STR_PRINT("| bytes:               %e\n", (double)(l_total_bytes));
        STR_PRINT("| seconds:             %f\n", a_elapsed_time);
        STR_PRINT("| mean bytes/conn:     %f\n", ((double)l_total_bytes)/((double)l_total.m_upsv_resp));
        STR_PRINT("| fetches/sec:         %f\n", ((double)l_total.m_upsv_resp)/(a_elapsed_time));
        STR_PRINT("| bytes/sec:           %e\n", ((double)l_total_bytes)/a_elapsed_time);
        // TODO Fix stdev/var calcs
#if 0
#define SHOW_XSTAT_LINE(_tag, stat)\
        do {\
        STR_PRINT("| %-16s %12.6f mean, %12.6f max, %12.6f min, %12.6f stdev, %12.6f var\n",\
               _tag,\
               stat.mean()/1000.0,\
               stat.max()/1000.0,\
               stat.min()/1000.0,\
               stat.stdev()/1000.0,\
               stat.var()/1000.0);\
        } while(0)
#else
#define SHOW_XSTAT_LINE(_tag, stat)\
        do {\
        STR_PRINT("| %-16s %12.6f mean, %12.6f max, %12.6f min\n",\
               _tag,\
               stat.mean()/1000.0,\
               stat.max()/1000.0,\
               stat.min()/1000.0);\
        } while(0)
#endif
        SHOW_XSTAT_LINE("ms/connect:", l_total.m_upsv_stat_us_connect);
        SHOW_XSTAT_LINE("ms/1st-response:", l_total.m_upsv_stat_us_first_response);
        SHOW_XSTAT_LINE("ms/end2end:", l_total.m_upsv_stat_us_end_to_end);

        if(g_color)
        {
        STR_PRINT("| %sHTTP response codes%s: \n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF);
        }
        else
        {
        STR_PRINT("| HTTP response codes: \n");
        }
        ns_hlx::status_code_count_map_t l_status_code_count_map;
        get_status_codes(l_status_code_count_map);
        for(ns_hlx::status_code_count_map_t::const_iterator i_status_code = l_status_code_count_map.begin();
            i_status_code != l_status_code_count_map.end();
            ++i_status_code)
        {
                if(g_color)
                {
                STR_PRINT("| %s%3d%s -- %u\n", ANSI_COLOR_FG_MAGENTA, i_status_code->first, ANSI_COLOR_OFF, i_status_code->second);
                }
                else
                {
                STR_PRINT("| %3d -- %u\n", i_status_code->first, i_status_code->second);
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void get_results_http_load(double a_elapsed_time,
                           std::string &ao_results,
                           bool a_one_line_flag)
{
        // Get stats
        ns_hlx::t_stat_cntr_t l_total;
        ns_hlx::t_stat_calc_t l_total_calc;
        ns_hlx::t_stat_cntr_list_t l_thread;
        get_stat(l_total, l_total_calc, l_thread);
        std::string l_tag;
        // Separator
        std::string l_sep = "\n";
        if(a_one_line_flag) l_sep = "||";

        // TODO Fix elapse and max parallel
        l_tag = "State";
        char l_buf[1024];
        uint64_t l_total_bytes = l_total.m_upsv_bytes_read + l_total.m_upsv_bytes_written;
        STR_PRINT("%s: ", l_tag.c_str());
        STR_PRINT("%" PRIu64 " fetches, ", l_total.m_upsv_resp);
        STR_PRINT("%u max parallel, ", g_num_parallel);
        STR_PRINT("%e bytes, ", (double)(l_total_bytes));
        STR_PRINT("in %f seconds, ", a_elapsed_time);
        STR_PRINT("%s", l_sep.c_str());
        STR_PRINT("%f mean bytes/connection, ", ((double)l_total_bytes)/((double)l_total.m_upsv_resp));
        STR_PRINT("%s", l_sep.c_str());
        STR_PRINT("%f fetches/sec, %e bytes/sec", ((double)l_total.m_upsv_resp)/(a_elapsed_time), ((double)l_total_bytes)/a_elapsed_time);
        STR_PRINT("%s", l_sep.c_str());
#define SHOW_XSTAT_LINE_LEGACY(_tag, stat)\
        STR_PRINT("%s %.6f mean, %.6f max, %.6f min, %.6f stdev",\
               _tag,                                          \
               stat.mean()/1000.0,                            \
               stat.max()/1000.0,                             \
               stat.min()/1000.0,                             \
               stat.stdev()/1000.0);                          \
        printf("%s", l_sep.c_str())
        SHOW_XSTAT_LINE_LEGACY("msecs/connect:", l_total.m_upsv_stat_us_connect);
        SHOW_XSTAT_LINE_LEGACY("msecs/first-response:", l_total.m_upsv_stat_us_first_response);
        SHOW_XSTAT_LINE_LEGACY("msecs/end2end:", l_total.m_upsv_stat_us_end_to_end);
        STR_PRINT("HTTP response codes: ");
        if(l_sep == "\n")
        {
        STR_PRINT("%s", l_sep.c_str());
        }
        ns_hlx::status_code_count_map_t l_status_code_count_map;
        get_status_codes(l_status_code_count_map);
        for(ns_hlx::status_code_count_map_t::const_iterator i_status_code = l_status_code_count_map.begin();
            i_status_code != l_status_code_count_map.end();
            ++i_status_code)
        {
        STR_PRINT("code %d -- %u, ", i_status_code->first, i_status_code->second);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void get_results_json(double a_elapsed_time,
                      std::string &ao_results)
{
        // Get stats
        ns_hlx::t_stat_cntr_t l_total;
        ns_hlx::t_stat_calc_t l_total_calc;
        ns_hlx::t_stat_cntr_list_t l_thread;
        get_stat(l_total, l_total_calc, l_thread);
        uint64_t l_total_bytes = l_total.m_upsv_bytes_read + l_total.m_upsv_bytes_written;
        rapidjson::Document l_body;
        l_body.SetObject();
        rapidjson::Document::AllocatorType& l_alloc = l_body.GetAllocator();
#define ADD_MEMBER(_l, _v) \
        l_body.AddMember(_l, _v, l_alloc)

        ADD_MEMBER("fetches", l_total.m_upsv_resp);
        ADD_MEMBER("max-parallel", g_num_parallel);
        ADD_MEMBER("bytes", (double)(l_total_bytes));
        ADD_MEMBER("seconds", a_elapsed_time);
        ADD_MEMBER("mean-bytes-per-conn", ((double)l_total_bytes)/((double)l_total.m_upsv_resp));
        ADD_MEMBER("fetches-per-sec", ((double)l_total.m_upsv_resp)/(a_elapsed_time));
        ADD_MEMBER("bytes-per-sec", ((double)l_total_bytes)/a_elapsed_time);

        // TODO Fix stdev/var calcs
        // Stats
        ADD_MEMBER("connect-ms-mean", l_total.m_upsv_stat_us_connect.mean()/1000.0);
        ADD_MEMBER("connect-ms-max", l_total.m_upsv_stat_us_connect.max()/1000.0);
        ADD_MEMBER("connect-ms-min", l_total.m_upsv_stat_us_connect.min()/1000.0);

        ADD_MEMBER("1st-resp-ms-mean", l_total.m_upsv_stat_us_first_response.mean()/1000.0);
        ADD_MEMBER("1st-resp-ms-max", l_total.m_upsv_stat_us_first_response.max()/1000.0);
        ADD_MEMBER("1st-resp-ms-min", l_total.m_upsv_stat_us_first_response.min()/1000.0);

        ADD_MEMBER("end2end-ms-mean", l_total.m_upsv_stat_us_end_to_end.mean()/1000.0);
        ADD_MEMBER("end2end-ms-max", l_total.m_upsv_stat_us_end_to_end.max()/1000.0);
        ADD_MEMBER("end2end-ms-min", l_total.m_upsv_stat_us_end_to_end.min()/1000.0);

        ns_hlx::status_code_count_map_t l_status_code_count_map;
        get_status_codes(l_status_code_count_map);
        if(l_status_code_count_map.size())
        {
        rapidjson::Value l_obj;
        l_obj.SetObject();
        for(ns_hlx::status_code_count_map_t::const_iterator i_status_code = l_status_code_count_map.begin();
            i_status_code != l_status_code_count_map.end();
            ++i_status_code)
        {
                char l_buf[16]; snprintf(l_buf,16,"%3d",i_status_code->first);
                l_obj.AddMember(rapidjson::Value(l_buf, l_alloc).Move(), i_status_code->second, l_alloc);
        }
        l_body.AddMember("response-codes", l_obj, l_alloc);
        }
        rapidjson::StringBuffer l_strbuf;
        rapidjson::Writer<rapidjson::StringBuffer> l_writer(l_strbuf);
        l_body.Accept(l_writer);
        ao_results.assign(l_strbuf.GetString(), l_strbuf.GetSize());
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t read_file(const char *a_file, char **a_buf, uint32_t *a_len)
{
        // Check is a file
        struct stat l_stat;
        int32_t l_s = HLX_STATUS_OK;
        l_s = stat(a_file, &l_stat);
        if(l_s != 0)
        {
                printf("Error performing stat on file: %s.  Reason: %s\n", a_file, strerror(errno));
                return HLX_STATUS_ERROR;
        }

        // Check if is regular file
        if(!(l_stat.st_mode & S_IFREG))
        {
                printf("Error opening file: %s.  Reason: is NOT a regular file\n", a_file);
                return HLX_STATUS_ERROR;
        }

        // Open file...
        FILE * l_file;
        l_file = fopen(a_file,"r");
        if (NULL == l_file)
        {
                printf("Error opening file: %s.  Reason: %s\n", a_file, strerror(errno));
                return HLX_STATUS_ERROR;
        }

        // Read in file...
        int32_t l_size = l_stat.st_size;
        *a_buf = (char *)malloc(sizeof(char)*l_size);
        *a_len = l_size;
        int32_t l_read_size;
        l_read_size = fread(*a_buf, 1, l_size, l_file);
        if(l_read_size != l_size)
        {
                printf("Error performing fread.  Reason: %s [%d:%d]\n",
                                strerror(errno), l_read_size, l_size);
                return HLX_STATUS_ERROR;
        }

        // Close file...
        l_s = fclose(l_file);
        if (HLX_STATUS_OK != l_s)
        {
                printf("Error performing fclose.  Reason: %s\n", strerror(errno));
                return HLX_STATUS_ERROR;
        }
        return 0;
}
