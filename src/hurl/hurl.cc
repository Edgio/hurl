//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hurl.cc
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
#include "hurl/dns/nlookup.h"
#include "hurl/nconn/nconn.h"
#include "hurl/nconn/nconn_tcp.h"
#include "hurl/nconn/nconn_tls.h"
#include "hurl/http/cb.h"
#include "hurl/support/obj_pool.h"
#include "hurl/support/tls_util.h"
#include "hurl/status.h"
#include "hurl/support/time_util.h"
#include "hurl/support/trace.h"
#include "hurl/support/string_util.h"
#include "hurl/support/atomic.h"
#include "hurl/support/nbq.h"
#include "hurl/evr/evr.h"
#include "hurl/http/resp.h"
#include "hurl/http/api_resp.h"
#include "support/ndebug.h"
#include "support/file_util.h"
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
#include <math.h>
#include <list>
#include <algorithm>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
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
// free context
#include <openssl/ssl.h>
// Json output
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define NB_ENABLE  1
#define NB_DISABLE 0
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
                        return HURL_STATUS_ERROR;\
                }\
        } while(0)
#define CHECK_FOR_NULL_ERROR(_data) \
        do {\
                if(!_data) {\
                        return HURL_STATUS_ERROR;\
                }\
        } while(0)
//: ----------------------------------------------------------------------------
//: types
//: ----------------------------------------------------------------------------
typedef std::vector <std::string> path_substr_vector_t;
typedef std::vector <std::string> path_vector_t;
//: ----------------------------------------------------------------------------
//: Globals
//: ----------------------------------------------------------------------------
static bool g_test_finished = false;
static bool g_verbose = false;
static bool g_color = false;
static uint64_t g_rate_delta_us = 0;
static uint32_t g_num_threads = 1;
static int64_t g_reqs_per_conn = -1;
static bool g_stats = true;
static bool g_quiet = false;
// -----------------------------------------------
// Path vector support
// -----------------------------------------------
static bool g_path_multi = false;
static tinymt64_t *g_path_rand_ptr = NULL;
static path_vector_t g_path_vector;
static std::string g_path;
static uint32_t g_path_vector_last_idx = 0;
static bool g_path_order_random = true;
static pthread_mutex_t g_path_vector_mutex;
static pthread_mutex_t g_completion_mutex;
//: ----------------------------------------------------------------------------
//:                               S T A T S
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::map<uint16_t, uint32_t > status_code_count_map_t;
//: ----------------------------------------------------------------------------
//: xstat
//: ----------------------------------------------------------------------------
typedef struct xstat_struct
{
        double m_sum_x;
        double m_sum_x2;
        double m_min;
        double m_max;
        uint64_t m_num;
        double min() const { return m_min; }
        double max() const { return m_max; }
        double mean() const { return (m_num > 0) ? m_sum_x / m_num : 0.0; }
        double var() const { return (m_num > 0) ? (m_sum_x2 - m_sum_x) / m_num : 0.0; }
        double stdev() const;
        xstat_struct():
                m_sum_x(0.0),
                m_sum_x2(0.0),
                m_min(1000000000.0),
                m_max(0.0),
                m_num(0)
        {}
        void clear()
        {
                m_sum_x = 0.0;
                m_sum_x2 = 0.0;
                m_min = 1000000000.0;
                m_max = 0.0;
                m_num = 0;
        };
} xstat_t;
//: ----------------------------------------------------------------------------
//: counter stats
//: ----------------------------------------------------------------------------
typedef struct t_stat_cntr_struct
{
        // Stats
        xstat_t m_stat_us_connect;
        xstat_t m_stat_us_first_response;
        xstat_t m_stat_us_end_to_end;
        // Upstream stats
        uint64_t m_conn_started;
        uint64_t m_conn_completed;
        uint64_t m_reqs;
        uint64_t m_idle_killed;
        uint64_t m_subr_queued;
        // Upstream resp
        uint64_t m_resp;
        uint64_t m_resp_status_2xx;
        uint64_t m_resp_status_3xx;
        uint64_t m_resp_status_4xx;
        uint64_t m_resp_status_5xx;
        // clnt counts
        uint64_t m_errors;
        uint64_t m_bytes_read;
        uint64_t m_bytes_written;
        // Totals
        uint64_t m_total_run;
        uint64_t m_total_add_timer;
        t_stat_cntr_struct():
                m_stat_us_connect(),
                m_stat_us_first_response(),
                m_stat_us_end_to_end(),
                m_conn_started(0),
                m_conn_completed(0),
                m_reqs(0),
                m_idle_killed(0),
                m_subr_queued(0),
                m_resp(0),
                m_resp_status_2xx(0),
                m_resp_status_3xx(0),
                m_resp_status_4xx(0),
                m_resp_status_5xx(0),
                m_errors(0),
                m_bytes_read(0),
                m_bytes_written(0),
                m_total_run(0),
                m_total_add_timer(0)
        {}
        void clear()
        {
                m_stat_us_connect.clear();
                m_stat_us_connect.clear();
                m_stat_us_first_response.clear();
                m_stat_us_end_to_end.clear();
                m_conn_started = 0;
                m_conn_completed = 0;
                m_reqs = 0;
                m_idle_killed = 0;
                m_subr_queued = 0;
                m_resp = 0;
                m_resp_status_2xx = 0;
                m_resp_status_3xx = 0;
                m_resp_status_4xx = 0;
                m_resp_status_5xx = 0;
                m_errors = 0;
                m_bytes_read = 0;
                m_bytes_written = 0;
                m_total_run = 0;
                m_total_add_timer = 0;
        }
} t_stat_cntr_t;
typedef std::list <t_stat_cntr_t> t_stat_cntr_list_t;
//: ----------------------------------------------------------------------------
//: calculated stats
//: ----------------------------------------------------------------------------
typedef struct t_stat_calc_struct
{
        // ups
        uint64_t m_req_delta;
        uint64_t m_resp_delta;
        float m_req_s;
        float m_resp_s;
        float m_bytes_read_s;
        float m_bytes_write_s;
        float m_resp_status_2xx_pcnt;
        float m_resp_status_3xx_pcnt;
        float m_resp_status_4xx_pcnt;
        float m_resp_status_5xx_pcnt;
        t_stat_calc_struct():
                m_req_delta(0),
                m_resp_delta(0),
                m_req_s(0.0),
                m_resp_s(0.0),
                m_bytes_read_s(0.0),
                m_bytes_write_s(0.0),
                m_resp_status_2xx_pcnt(0.0),
                m_resp_status_3xx_pcnt(0.0),
                m_resp_status_4xx_pcnt(0.0),
                m_resp_status_5xx_pcnt(0.0)
        {}
        void clear()
        {
                m_resp_s = 0.0;
                m_req_delta = 0;
                m_resp_delta = 0;
                m_req_s = 0.0;
                m_bytes_read_s = 0.0;
                m_bytes_write_s = 0.0;
                m_resp_status_2xx_pcnt = 0.0;
                m_resp_status_3xx_pcnt = 0.0;
                m_resp_status_4xx_pcnt = 0.0;
                m_resp_status_5xx_pcnt = 0.0;
        }
} t_stat_calc_t;
//: ----------------------------------------------------------------------------
//: \details: Update stat with new value
//: \return:  n/a
//: \param:   ao_stat stat to be updated
//: \param:   a_val value to update stat with
//: ----------------------------------------------------------------------------
void update_stat(xstat_t &ao_stat, double a_val)
{
        ao_stat.m_num++;
        ao_stat.m_sum_x += a_val;
        ao_stat.m_sum_x2 += a_val*a_val;
        if(a_val > ao_stat.m_max) ao_stat.m_max = a_val;
        if(a_val < ao_stat.m_min) ao_stat.m_min = a_val;
}
//: ----------------------------------------------------------------------------
//: \details: Add stats
//: \return:  n/a
//: \param:   ao_stat stat to be updated
//: \param:   a_from_stat stat to add
//: ----------------------------------------------------------------------------
void add_stat(xstat_t &ao_stat, const xstat_t &a_from_stat)
{
        ao_stat.m_num += a_from_stat.m_num;
        ao_stat.m_sum_x += a_from_stat.m_sum_x;
        ao_stat.m_sum_x2 += a_from_stat.m_sum_x2;
        if(a_from_stat.m_min < ao_stat.m_min) ao_stat.m_min = a_from_stat.m_min;
        if(a_from_stat.m_max > ao_stat.m_max) ao_stat.m_max = a_from_stat.m_max;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   n/a
//: ----------------------------------------------------------------------------
double xstat_struct::stdev() const
{
        return sqrt(var());
}
//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef struct range_struct {
        uint32_t m_start;
        uint32_t m_end;
} range_t;
//: ----------------------------------------------------------------------------
//: request object/meta
//: ----------------------------------------------------------------------------
class request {
public:
        typedef int32_t (*completion_cb_t)(request &, ns_hurl::nconn &, ns_hurl::resp &);
        typedef int32_t (*error_cb_t)(request &, ns_hurl::nconn *, ns_hurl::http_status_t, const char *);
        request():
                m_uid(0),
                m_scheme(ns_hurl::SCHEME_TCP),
                m_host(),
                m_url(),
                m_url_path(),
                m_url_query(),
                m_verb("GET"),
                m_headers(),
                m_body_data(NULL),
                m_body_data_len(0),
                m_port(0),
                m_expect_resp_body_flag(true),
                m_connect_only(false),
                m_keepalive(false),
                m_save(false),
                m_timeout_ms(10000),
                m_completion_cb(NULL),
                m_error_cb(NULL),
                m_host_info(),
                m_data(NULL),
                m_start_time_ms(0)
        {};
        request(const request &a_r):
                m_uid(a_r.m_uid),
                m_scheme(a_r.m_scheme),
                m_host(a_r.m_host),
                m_url(a_r.m_url),
                m_url_path(a_r.m_url_path),
                m_url_query(a_r.m_url_query),
                m_verb(a_r.m_verb),
                m_headers(a_r.m_headers),
                m_body_data(a_r.m_body_data),
                m_body_data_len(a_r.m_body_data_len),
                m_port(a_r.m_port),
                m_expect_resp_body_flag(a_r.m_expect_resp_body_flag),
                m_connect_only(a_r.m_connect_only),
                m_keepalive(a_r.m_keepalive),
                m_save(a_r.m_save),
                m_timeout_ms(a_r.m_timeout_ms),
                m_completion_cb(a_r.m_completion_cb),
                m_error_cb(a_r.m_error_cb),
                m_host_info(a_r.m_host_info),
                m_data(a_r.m_data),
                m_start_time_ms(a_r.m_start_time_ms)
        {}
        int set_header(const std::string &a_key, const std::string &a_val)
        {
                ns_hurl::kv_map_list_t::iterator i_obj = m_headers.find(a_key);
                if(i_obj != m_headers.end())
                {
                        // Special handling for Host/User-agent/referer
                        bool l_replace = false;
                        bool l_remove = false;
                        if(!strcasecmp(a_key.c_str(), "User-Agent") ||
                           !strcasecmp(a_key.c_str(), "Referer") ||
                           !strcasecmp(a_key.c_str(), "Accept") ||
                           !strcasecmp(a_key.c_str(), "Host"))
                        {
                                l_replace = true;
                                if(a_val.empty())
                                {
                                        l_remove = true;
                                }
                        }
                        if(l_replace)
                        {
                                i_obj->second.pop_front();
                                if(!l_remove)
                                {
                                        i_obj->second.push_back(a_val);
                                }
                        }
                        else
                        {
                                i_obj->second.push_back(a_val);
                        }
                }
                else
                {
                        ns_hurl::str_list_t l_list;
                        l_list.push_back(a_val);
                        m_headers[a_key] = l_list;
                }
                return HURL_STATUS_OK;
        }
        // Initialize
        int32_t init_with_url(const std::string &a_url);
        uint64_t m_uid;
        ns_hurl::scheme_t m_scheme;
        std::string m_host;
        std::string m_url;
        std::string m_url_path;
        std::string m_url_query;
        std::string m_verb;
        ns_hurl::kv_map_list_t m_headers;
        char *m_body_data;
        uint32_t m_body_data_len;
        uint16_t m_port;
        bool m_expect_resp_body_flag;
        bool m_connect_only;
        bool m_keepalive;
        bool m_save;
        uint32_t m_timeout_ms;
        completion_cb_t m_completion_cb;
        error_cb_t m_error_cb;
        ns_hurl::host_info m_host_info;
        void *m_data;
        uint64_t m_start_time_ms;
private:
        // Disallow copy/assign
        request& operator=(const request &);
};
typedef std::vector <range_t> range_vector_t;
class t_hurl;
typedef std::list <t_hurl *> t_hurl_list_t;
//: ----------------------------------------------------------------------------
//: Prototypes
//: ----------------------------------------------------------------------------
static int32_t create_request(request &a_request, ns_hurl::nbq &a_nbq);
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
ns_hurl::uint64_atomic_t g_cur_subr_uid = 0;
static inline uint64_t get_next_request_uuid(void)
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
        ns_hurl::nconn *m_nconn;
        t_hurl *m_t_hurl;
        ns_hurl::evr_timer_t *m_timer_obj;
        ns_hurl::resp *m_resp;
        ns_hurl::nbq *m_in_q;
        ns_hurl::nbq *m_out_q;
        request *m_request;
        uint64_t m_idx;
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
                m_request(NULL),
                m_idx(0)
#if 0
        ,
                m_last_active_ms(0),
                m_timeout_ms(10000)
#endif
        {}
        ~session(void)
        {
                if(m_resp)
                {
                        delete m_resp;
                        m_resp = NULL;
                }
                if(m_in_q)
                {
                        delete m_in_q;
                        m_in_q = NULL;
                }
                if(m_out_q)
                {
                        delete m_out_q;
                        m_out_q = NULL;
                }
        }
        int32_t cancel_timer(void *a_timer);
        int32_t teardown(ns_hurl::http_status_t a_status);
        int32_t request_error(ns_hurl::http_status_t a_status);
        uint64_t get_idx(void) {return m_idx;}
        void set_idx(uint64_t a_idx) {m_idx = a_idx;}
#if 0
        void clear(void);
        uint32_t get_timeout_ms(void);
        void set_timeout_ms(uint32_t a_t_ms);
        uint64_t get_last_active_ms(void);
        void set_last_active_ms(uint64_t a_time_ms);
#endif
        // -------------------------------------------------
        // Public Static (class) methods
        // -------------------------------------------------
        static int32_t evr_fd_readable_cb(void *a_data) {return run_state_machine(a_data, ns_hurl::EVR_MODE_READ);}
        static int32_t evr_fd_writeable_cb(void *a_data){return run_state_machine(a_data, ns_hurl::EVR_MODE_WRITE);}
        static int32_t evr_fd_error_cb(void *a_data) {return run_state_machine(a_data, ns_hurl::EVR_MODE_ERROR);}
        static int32_t evr_fd_timeout_cb(void *a_ctx, void *a_data){return run_state_machine(a_data, ns_hurl::EVR_MODE_TIMEOUT);}
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        session& operator=(const session &);
        session(const session &);
        bool request_complete(void);
        void request_log_status(uint16_t a_status = 0);
        // -------------------------------------------------
        // Private Static (class) methods
        // -------------------------------------------------
        static int32_t run_state_machine(void *a_data, ns_hurl::evr_mode_t a_conn_mode);
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
        typedef std::map <ns_hurl::nconn *, ns_hurl::nconn *> nconn_map_t;
        typedef std::list <ns_hurl::nconn *> nconn_list_t;
        typedef ns_hurl::obj_pool <session> session_pool_t;
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        t_hurl(const request &a_request,
               uint32_t a_max_parallel,
               int32_t a_num_to_request):
               m_stopped(true),
               m_t_run_thread(),
               m_active_nconn_map(),
               m_idle_nconn_list(),
               m_session_pool(),
               m_stat(),
               m_status_code_count_map(),
               m_num_in_progress(0),
               m_orphan_in_q(NULL),
               m_orphan_out_q(NULL),
               m_ctx(NULL),
               m_request(a_request),
               m_evr_loop(NULL),
               m_is_initd(false),
               m_num_parallel_max(a_max_parallel),
               m_num_to_request(a_num_to_request)
        {
                m_orphan_in_q = new ns_hurl::nbq(4096);
                m_orphan_out_q = new ns_hurl::nbq(4096);
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
                // clean up connections
                for(nconn_list_t::iterator i_n = m_idle_nconn_list.begin();
                    i_n != m_idle_nconn_list.end();
                    ++i_n)
                {
                        if(*i_n)
                        {
                                (*i_n)->nc_cleanup();
                        }
                        delete (*i_n);
                        (*i_n) = NULL;
                }
                m_idle_nconn_list.clear();
                // clean up connections
                for(nconn_map_t::iterator i_n = m_active_nconn_map.begin();
                    i_n != m_active_nconn_map.end();
                    ++i_n)
                {
                        if(i_n->second)
                        {
                                (i_n->second)->nc_cleanup();
                        }
                        delete (i_n->second);
                        i_n->second = NULL;
                }
                m_active_nconn_map.clear();
                if(m_evr_loop)
                {
                        delete m_evr_loop;
                        m_evr_loop = NULL;
                }
        }
        int32_t init(void)
        {
                if(m_is_initd) return HURL_STATUS_OK;
                // TODO -make loop configurable
#if defined(__linux__)
                m_evr_loop = new ns_hurl::evr_loop(ns_hurl::EVR_LOOP_EPOLL, 512);
#elif defined(__FreeBSD__) || defined(__APPLE__)
                m_evr_loop = new ns_hurl::evr_loop(ns_hurl::EVR_LOOP_SELECT, 512);
#else
                m_evr_loop = new ns_hurl::evr_loop(ns_hurl::EVR_LOOP_SELECT, 512);
#endif
                if(!m_evr_loop)
                {
                        TRC_ERROR("m_evr_loop == NULL");
                        return HURL_STATUS_ERROR;
                }
                m_is_initd = true;
                m_request.m_data = this;
                return HURL_STATUS_OK;
        }
        int run(void) {
                int32_t l_pthread_error = 0;
                l_pthread_error = pthread_create(&m_t_run_thread, NULL, t_run_static, this);
                if (l_pthread_error != 0) {
                        return HURL_STATUS_ERROR;
                }
                return HURL_STATUS_OK;
        };
        void *t_run(void *a_nothing);
        void stop(void) {
                m_stopped = true;
                m_evr_loop->signal();
        }
        bool is_running(void) { return !m_stopped; }
        int32_t cancel_timer(void *a_timer) {
                if(!m_evr_loop) return HURL_STATUS_ERROR;
                if(!a_timer) return HURL_STATUS_OK;
                ns_hurl::evr_timer_t *l_t = static_cast<ns_hurl::evr_timer_t *>(a_timer);
                return m_evr_loop->cancel_timer(l_t);
        }
        int32_t cleanup_session(session *a_ses, ns_hurl::nconn *a_nconn);
        void add_stat_to_agg(const ns_hurl::conn_stat_t &a_conn_stat, uint16_t a_status_code);
        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        sig_atomic_t m_stopped;
        pthread_t m_t_run_thread;
        nconn_map_t m_active_nconn_map;
        nconn_list_t m_idle_nconn_list;
        session_pool_t m_session_pool;
        t_stat_cntr_t m_stat;
        status_code_count_map_t m_status_code_count_map;
        uint32_t m_num_in_progress;
        ns_hurl::nbq *m_orphan_in_q;
        ns_hurl::nbq *m_orphan_out_q;
        SSL_CTX *m_ctx;
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
        ns_hurl::nconn *create_new_nconn(void);
        int32_t request_start(void);
        int32_t request_dequeue(void);
        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        request m_request;
        ns_hurl::evr_loop *m_evr_loop;
        bool m_is_initd;
        uint32_t m_num_parallel_max;
        int32_t m_num_to_request;
};
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t request::init_with_url(const std::string &a_url)
{
        std::string l_url_fixed = a_url;
        // Find scheme prefix "://"
        if(a_url.find("://", 0) == std::string::npos)
        {
                l_url_fixed = "http://" + a_url;
        }
        //NDBG_PRINT("Parse url:           %s\n", a_url.c_str());
        //NDBG_PRINT("Parse a_wildcarding: %d\n", a_wildcarding);
        http_parser_url l_url;
        http_parser_url_init(&l_url);
        // silence bleating memory sanitizers...
        //memset(&l_url, 0, sizeof(l_url));
        int l_status;
        l_status = http_parser_parse_url(l_url_fixed.c_str(), l_url_fixed.length(), 0, &l_url);
        if(l_status != 0)
        {
                NDBG_PRINT("Error parsing url: %s\n", l_url_fixed.c_str());
                // TODO get error msg from http_parser
                return HURL_STATUS_ERROR;
        }
        // Set no port
        m_port = 0;
        for(uint32_t i_part = 0; i_part < UF_MAX; ++i_part)
        {
                //NDBG_PRINT("i_part: %d offset: %d len: %d\n", i_part, l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                //NDBG_PRINT("len+off: %d\n",       l_url.field_data[i_part].len + l_url.field_data[i_part].off);
                //NDBG_PRINT("a_url.length(): %d\n", (int)a_url.length());
                if(l_url.field_data[i_part].len &&
                  // TODO Some bug with parser -parsing urls like "http://127.0.0.1" sans paths
                  ((l_url.field_data[i_part].len + l_url.field_data[i_part].off) <= l_url_fixed.length()))
                {
                        switch(i_part)
                        {
                        case UF_SCHEMA:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part: %s\n", l_part.c_str());
                                if(l_part == "http")
                                {
                                        m_scheme = ns_hurl::SCHEME_TCP;
                                }
                                else if(l_part == "https")
                                {
                                        m_scheme = ns_hurl::SCHEME_TLS;
                                }
                                else
                                {
                                        NDBG_PRINT("Error schema[%s] is unsupported\n", l_part.c_str());
                                        return HURL_STATUS_ERROR;
                                }
                                break;
                        }
                        case UF_HOST:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_HOST]: %s\n", l_part.c_str());
                                m_host = l_part;
                                break;
                        }
                        case UF_PORT:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_PORT]: %s\n", l_part.c_str());
                                m_port = (uint16_t)strtoul(l_part.c_str(), NULL, 10);
                                break;
                        }
                        case UF_PATH:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_PATH]: %s\n", l_part.c_str());
                                m_url_path = l_part;
                                break;
                        }
                        case UF_QUERY:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_QUERY]: %s\n", l_part.c_str());
                                m_url_query = l_part;
                                break;
                        }
                        case UF_FRAGMENT:
                        {
                                //std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_FRAGMENT]: %s\n", l_part.c_str());
                                //m_fragment = l_part;
                                break;
                        }
                        case UF_USERINFO:
                        {
                                //std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //sNDBG_PRINT("l_part[UF_USERINFO]: %s\n", l_part.c_str());
                                //m_userinfo = l_part;
                                break;
                        }
                        default:
                        {
                                break;
                        }
                        }
                }
        }
        // Default ports
        if(!m_port)
        {
                switch(m_scheme)
                {
                case ns_hurl::SCHEME_TCP:
                {
                        m_port = 80;
                        break;
                }
                case ns_hurl::SCHEME_TLS:
                {
                        m_port = 443;
                        break;
                }
                default:
                {
                        m_port = 80;
                        break;
                }
                }
        }
        //m_num_to_req = m_path_vector.size();
        //NDBG_PRINT("Showing parsed url.\n");
        //m_url.show();
        if (HURL_STATUS_OK != l_status)
        {
                // Failure
                NDBG_PRINT("Error parsing url: %s.\n", l_url_fixed.c_str());
                return HURL_STATUS_ERROR;
        }
        //NDBG_PRINT("Parsed url: %s\n", l_url_fixed.c_str());
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void session::request_log_status(uint16_t a_status)
{
        if(!m_t_hurl)
        {
                return;
        }
        // TODO log here???
        //++(m_t_hurl->m_stat.m_resp);
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
        else if((l_status >= 200) && (l_status < 300)){++m_t_hurl->m_stat.m_resp_status_2xx;}
        else if((l_status >= 300) && (l_status < 400)){++m_t_hurl->m_stat.m_resp_status_3xx;}
        else if((l_status >= 400) && (l_status < 500)){++m_t_hurl->m_stat.m_resp_status_4xx;}
        else if((l_status >= 500) && (l_status < 600)){++m_t_hurl->m_stat.m_resp_status_5xx;}
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool session::request_complete(void)
{
        request_log_status(0);
        if(!m_resp || !m_request || !m_nconn)
        {
                return true;
        }
        bool l_complete = false;
#if 0
        m_request->m_end_time_ms = ns_hurl::get_time_ms();
#endif
        // Get vars -completion -can delete subr object
        bool l_connect_only = m_request->m_connect_only;
        request::completion_cb_t l_completion_cb = m_request->m_completion_cb;
        // Call completion handler
        if(l_completion_cb)
        {
                l_completion_cb(*m_request, *m_nconn, *m_resp);
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
int32_t session::request_error(ns_hurl::http_status_t a_status)
{
        request_log_status(a_status);
        if(!m_request)
        {
                return HURL_STATUS_ERROR;
        }
#if 0
        m_request->m_end_time_ms = ns_hurl::get_time_ms();
        if(m_nconn && m_nconn->get_collect_stats_flag())
        {
                m_nconn->set_stat_tt_completion_us(ns_hurl::get_delta_time_us(m_nconn->get_connect_start_time_us()));
                m_nconn->reset_stats();
        }
#endif
        request::error_cb_t l_error_cb = m_request->m_error_cb;
        if(l_error_cb)
        {
                const char *l_err_str = NULL;
                if(m_nconn)
                {
                        l_err_str = m_nconn->get_last_error().c_str();
                }
                l_error_cb(*(m_request), m_nconn, a_status, l_err_str);
                // TODO Check status...
        }
        return HURL_STATUS_OK;
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
            return HURL_STATUS_ERROR;
        }
        int32_t l_s;
        l_s = m_t_hurl->cancel_timer(a_timer);
        if(l_s != HURL_STATUS_OK)
        {
                return HURL_STATUS_ERROR;
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t session::teardown(ns_hurl::http_status_t a_status)
{
        if(a_status != ns_hurl::HTTP_STATUS_OK)
        {
                if(m_resp)
                {
                        m_resp->set_status(a_status);
                }
                int32_t l_s;
                l_s = request_error(a_status);
                if(l_s != HURL_STATUS_OK)
                {
                        TRC_ERROR("performing subr_error\n");
                }
        }
        if(m_t_hurl)
        {
                int32_t l_s;
                l_s = m_t_hurl->cleanup_session(this, m_nconn);
                m_nconn = NULL;
                if(l_s != HURL_STATUS_OK)
                {
                        return HURL_STATUS_ERROR;
                }
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t session::run_state_machine(void *a_data, ns_hurl::evr_mode_t a_conn_mode)
{
        //NDBG_PRINT("%sRUN%s a_conn_mode: %d a_data: %p\n", ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF, a_conn_mode, a_data);
        //NDBG_PRINT_BT();
        //CHECK_FOR_NULL_ERROR(a_data);
        // TODO -return OK for a_data == NULL
        if(!a_data)
        {
                return HURL_STATUS_OK;
        }
        ns_hurl::nconn* l_nconn = static_cast<ns_hurl::nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_ctx());
        t_hurl *l_t_hurl = static_cast<t_hurl *>(l_nconn->get_ctx());
        session *l_ses = static_cast<session *>(l_nconn->get_data());
        // -------------------------------------------------
        // ERROR
        // -------------------------------------------------
        if(a_conn_mode == ns_hurl::EVR_MODE_ERROR)
        {
                // ignore errors for free connections
                if(l_nconn->is_free())
                {
                        TRC_ERROR("call back for free connection\n");
                        return HURL_STATUS_OK;
                }
                if(l_ses)
                {
                        l_ses->cancel_timer(l_ses->m_timer_obj);
                        // TODO Check status
                        l_ses->m_timer_obj = NULL;
                        return l_ses->teardown(ns_hurl::HTTP_STATUS_BAD_GATEWAY);
                }
                else
                {
                        if(l_t_hurl)
                        {
                                return l_t_hurl->cleanup_session(NULL, l_nconn);
                        }
                        else
                        {
                                TRC_ERROR("l_t_hurl == NULL\n");
                                return HURL_STATUS_ERROR;
                        }
                }
        }
        // -------------------------------------------------
        // TIMEOUT
        // -------------------------------------------------
        else if(a_conn_mode == ns_hurl::EVR_MODE_TIMEOUT)
        {
                // ignore timeout for free connections
                if(l_nconn->is_free())
                {
                        TRC_ERROR("call back for free connection\n");
                        return HURL_STATUS_OK;
                }
                // calc time since last active
                if(l_ses && l_t_hurl)
                {
                        // ---------------------------------
                        // timeout
                        // ---------------------------------
#if 0
                        uint64_t l_ct_ms = ns_hurl::get_time_ms();
                        if(((uint32_t)(l_ct_ms - l_uss->get_last_active_ms())) >= l_uss->get_timeout_ms())
                        {
                                ++(l_t_srvr->m_stat.m_errors);
                                ++(l_t_srvr->m_stat.m_idle_killed);
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
                                return HURL_STATUS_OK;
                        }
#endif
                }
                else
                {
                        TRC_ERROR("a_conn_mode[%d] ups_srvr_session[%p] || t_srvr[%p] == NULL\n",
                                        a_conn_mode,
                                        l_ses,
                                        l_t_hurl);
                        return HURL_STATUS_ERROR;
                }
        }
        else if(a_conn_mode == ns_hurl::EVR_MODE_READ)
        {
                // ignore readable for free connections
                if(l_nconn->is_free())
                {
                        TRC_ERROR("call back for free connection\n");
                        return HURL_STATUS_OK;
                }
        }
        // -------------------------------------------------
        // TODO unknown conn mode???
        // -------------------------------------------------
        else if((a_conn_mode != ns_hurl::EVR_MODE_READ) &&
                (a_conn_mode != ns_hurl::EVR_MODE_WRITE))
        {
                TRC_ERROR("unknown a_conn_mode: %d\n", a_conn_mode);
                return HURL_STATUS_OK;
        }
#if 0
        // set last active
        if(l_ses)
        {
                l_uss->set_last_active_ms(ns_hurl::get_time_ms());
        }
#endif
        // -------------------------------------------------
        // in/out q's
        // -------------------------------------------------
        ns_hurl::nbq *l_in_q = NULL;
        ns_hurl::nbq *l_out_q = NULL;
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
        int32_t l_s = HURL_STATUS_OK;
        do {
                uint32_t l_read = 0;
                uint32_t l_written = 0;
                l_s = l_nconn->nc_run_state_machine(a_conn_mode, l_in_q, l_read, l_out_q, l_written);
                //NDBG_PRINT("l_nconn->nc_run_state_machine(%d): status: %d read: %u l_written: %u\n", a_conn_mode, l_s, l_read, l_written);
                if(l_t_hurl)
                {
                        l_t_hurl->m_stat.m_bytes_read += l_read;
                        l_t_hurl->m_stat.m_bytes_written += l_written;
                        if(!g_verbose && (l_written > 0))
                        {
                                l_in_q->reset_write();
                        }
                }
                if(!l_ses ||
                   !l_ses->m_request ||
                   (l_s == ns_hurl::nconn::NC_STATUS_EOF) ||
                   (l_s == ns_hurl::nconn::NC_STATUS_ERROR) ||
                   l_nconn->is_done())
                {
                        goto check_conn_status;
                }
                // -----------------------------------------
                // READABLE
                // -----------------------------------------
                if(a_conn_mode == ns_hurl::EVR_MODE_READ)
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
                                        ++l_t_hurl->m_stat.m_resp;
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
                                        l_nconn->set_stat_tt_completion_us(ns_hurl::get_delta_time_us(l_nconn->get_connect_start_time_us()));

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
                                bool l_keepalive = l_ses->m_request->m_keepalive;
                                bool l_complete = l_ses->request_complete();
                                if(l_complete ||
                                  (!l_nconn_can_reuse ||
                                   !l_keepalive ||
                                   !l_hmsg_keep_alive))
                                {
                                        l_s = ns_hurl::nconn::NC_STATUS_EOF;
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
                else if(l_s == ns_hurl::nconn::NC_STATUS_OK)
                {
                        l_s = ns_hurl::nconn::NC_STATUS_BREAK;
                        //NDBG_PRINT("goto check_conn_status\n");
                        goto check_conn_status;
                }

check_conn_status:
                //NDBG_PRINT("goto check_conn_status\n");
                if(l_nconn->is_free())
                {
                        return HURL_STATUS_OK;
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
                                return HURL_STATUS_ERROR;
                        }
                }
                switch(l_s)
                {
                case ns_hurl::nconn::NC_STATUS_BREAK:
                {
                        //NDBG_PRINT("GOTO DONE!\n");
                        goto done;
                }
                case ns_hurl::nconn::NC_STATUS_EOF:
                {
                        // Connect only && done --early exit...
                        if(l_ses->m_request &&
                           l_ses->m_request->m_connect_only)
                        {
                                if(l_ses->m_resp)
                                {
                                        l_ses->m_resp->set_status(ns_hurl::HTTP_STATUS_OK);
                                }
                                if(l_t_hurl)
                                {
                                        l_t_hurl->add_stat_to_agg(l_nconn->get_stats(), HTTP_STATUS_OK);
                                }
                        }
                        if(l_ses->m_request)
                        {
                                l_ses->request_complete();
                        }
                        return l_ses->teardown(ns_hurl::HTTP_STATUS_OK);
                }
                case ns_hurl::nconn::NC_STATUS_ERROR:
                {
                        if(l_t_hurl)
                        {
                                ++(l_t_hurl->m_stat.m_errors);
                        }
                        l_ses->teardown(ns_hurl::HTTP_STATUS_BAD_GATEWAY);
                        TRC_ERROR("ns_hurl::nconn::NC_STATUS_ERROR\n");
                        return HURL_STATUS_ERROR;
                }
                default:
                {
                        break;
                }
                }
                if(l_nconn->is_done())
                {
                        return l_ses->teardown(ns_hurl::HTTP_STATUS_OK);
                }
        } while((l_s != ns_hurl::nconn::NC_STATUS_AGAIN) &&
                (l_t_hurl->is_running()));
idle_check:
        //NDBG_PRINT("%sIDLE CONNECTION CHECK%s: l_idle: %d\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, l_idle);
        if(l_idle)
        {
                if(l_t_hurl)
                {
                        l_t_hurl->m_idle_nconn_list.push_back(l_nconn);
                        l_t_hurl->m_active_nconn_map.erase(l_nconn);
                }
                l_nconn->set_data(NULL);
                // TODO start new subr???
                return HURL_STATUS_OK;
        }
done:
        return HURL_STATUS_OK;
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
        if(l_s != HURL_STATUS_OK)
        {
                TRC_ERROR("performing init.\n");
                return NULL;
        }
        m_stopped = false;
        m_stat.clear();
        l_s = request_dequeue();
        // TODO check return status???
        // -------------------------------------------------
        // Run server
        // -------------------------------------------------
        while((!m_stopped) &&
              ((m_num_to_request < 0) || ((uint32_t)m_num_to_request > m_stat.m_resp)))
        {
                //NDBG_PRINT("Running.\n");
                l_s = m_evr_loop->run();
                if(l_s != HURL_STATUS_OK)
                {
                        // TODO log run failure???
                }
                // Subrequests
                l_s = request_dequeue();
                if(l_s != HURL_STATUS_OK)
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
int32_t t_hurl::request_dequeue(void)
{
        uint32_t l_dq = 0;
        while(!g_test_finished &&
              !m_stopped &&
              (m_num_in_progress < m_num_parallel_max) &&
              ((m_num_to_request < 0) || ((uint32_t)m_num_to_request > m_stat.m_reqs)))
        {
                int32_t l_s;
                l_s = request_start();
                if(l_s != HURL_STATUS_OK)
                {
                        // Log error
                        TRC_ERROR("performing request_start\n");
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
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ns_hurl::nconn *t_hurl::create_new_nconn(void)
{
        // -------------------------------------------------
        // get new connection
        // -------------------------------------------------
        ns_hurl::nconn *l_nconn = NULL;
        if(m_request.m_scheme == ns_hurl::SCHEME_TLS)
        {
                l_nconn = new ns_hurl::nconn_tls();
                l_nconn->set_opt(ns_hurl::nconn_tls::OPT_TLS_CTX, m_ctx, sizeof(m_ctx));
        }
        else if(m_request.m_scheme == ns_hurl::SCHEME_TCP)
        {
                l_nconn = new ns_hurl::nconn_tcp();
        }
        else
        {
                return NULL;
        }
        // -------------------------------------------------
        // turn of linger -just for load tester
        // -------------------------------------------------
        bool l_val = true;
        l_nconn->set_opt(ns_hurl::nconn_tcp::OPT_TCP_NO_LINGER, &l_val, sizeof(l_val));
        // -------------------------------------------------
        // set params
        // -------------------------------------------------
        l_nconn->set_ctx(this);
        l_nconn->set_num_reqs_per_conn(g_reqs_per_conn);
        l_nconn->set_collect_stats(false);
        l_nconn->set_connect_only(false);
        l_nconn->set_evr_loop(m_evr_loop);
        l_nconn->setup_evr_fd(session::evr_fd_readable_cb,
                              session::evr_fd_writeable_cb,
                              session::evr_fd_error_cb);
        l_nconn->set_host_info(m_request.m_host_info);
        return l_nconn;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hurl::request_start(void)
{
        //NDBG_PRINT("%ssubr label%s: %s --HOST: %s\n",
        //                ANSI_COLOR_FG_RED, ANSI_COLOR_OFF,
        //                m_subr.get_label().c_str(), m_subr.get_host().c_str());
        int32_t l_s;
        // Try get idle from proxy pool
        ns_hurl::nconn *l_nconn = NULL;
        // try get from idle list
        if(m_idle_nconn_list.empty() ||
           (m_idle_nconn_list.front() == NULL))
        {
                l_nconn = create_new_nconn();
                l_nconn->set_label(m_request.m_host);
                m_active_nconn_map[l_nconn] = l_nconn;
                //NDBG_PRINT("%sCREATING NEW CONNECTION%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
        }
        else
        {
                l_nconn = m_idle_nconn_list.front();
                m_idle_nconn_list.pop_front();
                m_active_nconn_map[l_nconn] = l_nconn;
        }
        if(!l_nconn)
        {
                // TODO fatal???
                TRC_ERROR("l_nconn == NULL\n");
                return HURL_STATUS_ERROR;
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
        l_ses->m_request = &m_request;
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
                l_ses->m_resp = new ns_hurl::resp();
        }
        l_ses->m_resp->init(g_verbose);
        l_ses->m_resp->m_http_parser->data = l_ses->m_resp;
        l_nconn->set_read_cb(ns_hurl::http_parse);
        l_nconn->set_read_cb_data(l_ses->m_resp);
        l_ses->m_resp->m_expect_resp_body_flag = m_request.m_expect_resp_body_flag;
        // setup q's
        if(!l_ses->m_in_q)
        {
                l_ses->m_in_q = new ns_hurl::nbq(8*1024);
                l_ses->m_resp->set_q(l_ses->m_in_q);
        }
        else
        {
                l_ses->m_in_q->reset_write();
        }
        if(!l_ses->m_out_q)
        {
                l_ses->m_out_q = new ns_hurl::nbq(8*1024);
        }
        else
        {
                if(l_ses_reused && !g_path_multi)
                {
                        l_ses->m_out_q->reset_read();
                }
                else
                {
                        l_ses->m_out_q->reset_write();
                }
        }
        // create request
        if(!l_ses_reused || g_path_multi)
        {
                l_s = create_request(m_request, *(l_ses->m_out_q));
                if(HURL_STATUS_OK != l_s)
                {
                        TRC_ERROR("performing create_request\n");
                        return session::evr_fd_error_cb(l_nconn);
                }
        }
        // stats
        ++m_stat.m_reqs;
        if(g_stats)
        {
                m_request.m_start_time_ms = ns_hurl::get_time_ms();
                l_nconn->set_request_start_time_us(ns_hurl::get_time_us());
        }
#if 0
        l_uss->set_last_active_ms(ns_hurl::get_time_ms());
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
        if(l_s != HURL_STATUS_OK)
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
        //NDBG_PRINT("%sSTARTING REQUEST...%s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF);
        l_s = session::evr_fd_writeable_cb(l_nconn);
        if(l_s != HURL_STATUS_OK)
        {
                TRC_ERROR("performing evr_fd_writeable_cb\n");
                return HURL_STATUS_ERROR;
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hurl::cleanup_session(session *a_ses, ns_hurl::nconn *a_nconn)
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
                m_active_nconn_map.erase(a_nconn);
                a_nconn->nc_cleanup();
                delete a_nconn;
                // TODO Log error???
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_hurl::add_stat_to_agg(const ns_hurl::conn_stat_t &a_conn_stat, uint16_t a_status_code)
{
        update_stat(m_stat.m_stat_us_connect, a_conn_stat.m_tt_connect_us);
        update_stat(m_stat.m_stat_us_first_response, a_conn_stat.m_tt_first_read_us);
        update_stat(m_stat.m_stat_us_end_to_end, a_conn_stat.m_tt_completion_us);
        ++m_status_code_count_map[a_status_code];
}
//: ----------------------------------------------------------------------------
//: \details: Completion callback
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t s_completion_cb(request &a_request,
                               ns_hurl::nconn &a_nconn,
                               ns_hurl::resp &a_resp)
{
        //pthread_mutex_lock(&g_completion_mutex);
        //++g_num_completed;
        //pthread_mutex_unlock(&g_completion_mutex);
#if 0
        if((l_num_to_request != -1) && (g_num_completed >= (uint64_t)l_num_to_request))
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
                return HURL_STATUS_ERROR;
        }
        ++g_addrx_addr_ipv4;
        if(g_addrx_addr_ipv4 >= g_addrx_addr_ipv4_max)
        {
                g_addrx_addr_ipv4 = LOCAL_ADDR_V4_MIN;
        }
        pthread_mutex_unlock(&g_addrx_mutex);
        return HURL_STATUS_OK;
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
        return HURL_STATUS_ERROR;
success:
        // Check range...
        if(l_start > l_end)
        {
                printf("HURL_STATUS_ERROR: Bad range start[%u] > end[%u]\n", l_start, l_end);
                return HURL_STATUS_ERROR;
        }
        // Successfully matched we outie
        ao_range.m_start = l_start;
        ao_range.m_end = l_end;
        return HURL_STATUS_OK;
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
                        printf("HURL_STATUS_ERROR: Bad range for path: %s at pos: %zu\n", a_path, l_range_start_pos);
                        return HURL_STATUS_ERROR;
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
                int l_s = HURL_STATUS_OK;
                l_s = convert_exp_to_range(l_range_exp, l_range);
                if(HURL_STATUS_OK != l_s)
                {
                        printf("HURL_STATUS_ERROR: performing convert_exp_to_range(%s)\n", l_range_exp.c_str());
                        return HURL_STATUS_ERROR;
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
        return HURL_STATUS_OK;
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
                return HURL_STATUS_OK;
        }
        a_path_part.append(a_substr_vector[a_substr_idx]);
        ++a_substr_idx;
        if(a_range_idx >= a_range_vector.size())
        {
                g_path_vector.push_back(a_path_part);
                return HURL_STATUS_OK;
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
        return HURL_STATUS_OK;
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
                return HURL_STATUS_OK;
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
                a_path = l_p;
                int32_t l_s;
                path_substr_vector_t l_path_substr_vector;
                range_vector_t l_range_vector;
                if(l_p)
                {
                        l_s = parse_path(l_p, l_path_substr_vector, l_range_vector);
                        if(l_s != HURL_STATUS_OK)
                        {
                                printf("HURL_STATUS_ERROR: Performing parse_path(%s)\n", l_p);
                                return HURL_STATUS_ERROR;
                        }
                }
                // If empty path explode
                if(l_range_vector.size())
                {
                        l_s = path_exploder(std::string(""), l_path_substr_vector, 0, l_range_vector, 0);
                        if(l_s != HURL_STATUS_OK)
                        {
                                printf("HURL_STATUS_ERROR: Performing explode_path(%s)\n", l_p);
                                return HURL_STATUS_ERROR;
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
                if(!l_p)
                {
                        l_p = strtok_r(NULL, SPECIAL_EFX_OPT_SEPARATOR, &l_save_ptr);
                        continue;
                }
                char l_options[1024];
                char *l_options_save_ptr;
                strncpy(l_options, l_p, 1024);
                //printf("Options: %s\n", l_options);
                char *l_k = strtok_r(l_options, SPECIAL_EFX_KV_SEPARATOR, &l_options_save_ptr);
                char *l_v = strtok_r(NULL, SPECIAL_EFX_KV_SEPARATOR, &l_options_save_ptr);
                std::string l_key = l_k;
                std::string l_val = l_v;
                //printf("key: %s\n", l_key.c_str());
                //printf("val: %s\n", l_val.c_str());
                // ---------------------------------
                //
                // ---------------------------------
                if (l_key == "order")
                {
                        if (l_val == "sequential") { g_path_order_random = false; }
                        else if (l_val == "random"){ g_path_order_random = true; }
                        else
                        {
                                printf("HURL_STATUS_ERROR: Bad value[%s] for key[%s]\n", l_val.c_str(), l_key.c_str());
                                return HURL_STATUS_ERROR;
                        }
                }
#if 0
                else if (l_key == "sampling")
                {
                        if (l_val == "oneshot") m_run_only_once_flag = true;
                        else if (l_val) m_run_only_once_flag = false;
                        else
                        {
                                NDBG_PRINT("HURL_STATUS_ERROR: Bad value[%s] for key[%s]\n", l_val.c_str(), l_key.c_str());
                                return HURL_STATUS_ERROR;
                        }
                }
#endif
                else
                {
                        printf("HURL_STATUS_ERROR: Unrecognized key[%s]\n", l_key.c_str());
                        return HURL_STATUS_ERROR;
                }
                l_p = strtok_r(NULL, SPECIAL_EFX_OPT_SEPARATOR, &l_save_ptr);
        }
        //printf("a_path: %s\n", a_path.c_str());
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &get_path(void *a_rand)
{
        // TODO -make this threadsafe -mutex per???
        if(!g_path_vector.size())
        {
                return g_path;
        }
        // Rollover..
        if(g_path_order_random == false)
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
//: ----------------------------------------------------------------------------
//: \details: create request callback
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t create_request(request &a_request, ns_hurl::nbq &a_nbq)
{
        // TODO grab from path...
        std::string l_path_ref = get_path(g_path_rand_ptr);
        char l_buf[2048];
        int32_t l_len = 0;
        if(l_path_ref.empty())
        {
                l_path_ref = "/";
        }
        if(!(a_request.m_url_query.empty()))
        {
                l_path_ref += "?";
                l_path_ref += a_request.m_url_query;
        }
        //NDBG_PRINT("HOST: %s PATH: %s\n", a_reqlet.m_url.m_host.c_str(), l_path_ref.c_str());
        l_len = snprintf(l_buf, sizeof(l_buf),
                        "%s %s HTTP/1.1",
                        a_request.m_verb.c_str(), l_path_ref.c_str());
        ns_hurl::nbq_write_request_line(a_nbq, l_buf, l_len);
        // -------------------------------------------------
        // Add repo headers
        // -------------------------------------------------
        bool l_specd_host = false;
        // Loop over reqlet map
        for(ns_hurl::kv_map_list_t::const_iterator i_hl = a_request.m_headers.begin();
            i_hl != a_request.m_headers.end();
            ++i_hl)
        {
                if(i_hl->first.empty() || i_hl->second.empty())
                {
                        continue;
                }
                for(ns_hurl::str_list_t::const_iterator i_v = i_hl->second.begin();
                    i_v != i_hl->second.end();
                    ++i_v)
                {
                        ns_hurl::nbq_write_header(a_nbq, i_hl->first.c_str(), i_hl->first.length(), i_v->c_str(), i_v->length());
                        if (strcasecmp(i_hl->first.c_str(), "host") == 0)
                        {
                                l_specd_host = true;
                        }
                }
        }
        // -------------------------------------------------
        // Default Host if unspecified
        // -------------------------------------------------
        if (!l_specd_host)
        {
                ns_hurl::nbq_write_header(a_nbq,
                                         "Host", strlen("Host"),
                                         a_request.m_host.c_str(), a_request.m_host.length());
        }
        // -------------------------------------------------
        // body
        // -------------------------------------------------
        if(a_request.m_body_data && a_request.m_body_data_len)
        {
                //NDBG_PRINT("Write: buf: %p len: %d\n", l_buf, l_len);
                ns_hurl::nbq_write_body(a_nbq, a_request.m_body_data, a_request.m_body_data_len);
        }
        else
        {
                ns_hurl::nbq_write_body(a_nbq, NULL, 0);
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void get_stat(t_stat_cntr_t &ao_total,
              t_stat_calc_t &ao_total_calc,
              t_stat_cntr_list_t &ao_thread,
              t_hurl_list_t &a_hurl_list)
{
        // -------------------------------------------------
        // store last values -for calc'd stats
        // -------------------------------------------------
        static t_stat_cntr_t s_last;
        static uint64_t s_stat_last_time_ms = 0;
        uint64_t l_cur_time_ms = ns_hurl::get_time_ms();
        uint64_t l_delta_ms = l_cur_time_ms - s_stat_last_time_ms;
        // -------------------------------------------------
        // Aggregate
        // -------------------------------------------------
        ao_total.clear();
        for(t_hurl_list_t::const_iterator i_t = a_hurl_list.begin();
            i_t != a_hurl_list.end();
           ++i_t)
        {
                ao_total.m_bytes_read += (*i_t)->m_stat.m_bytes_read;
                ao_total.m_bytes_written += (*i_t)->m_stat.m_bytes_written;
                ao_total.m_reqs += (*i_t)->m_stat.m_reqs;
                ao_total.m_resp += (*i_t)->m_stat.m_resp;
                ao_total.m_errors += (*i_t)->m_stat.m_errors;
                // TODO
                ao_total.m_idle_killed += 0;
                ao_total.m_resp_status_2xx = (*i_t)->m_stat.m_resp_status_2xx;
                ao_total.m_resp_status_3xx = (*i_t)->m_stat.m_resp_status_3xx;
                ao_total.m_resp_status_4xx = (*i_t)->m_stat.m_resp_status_4xx;
                ao_total.m_resp_status_5xx = (*i_t)->m_stat.m_resp_status_5xx;
                add_stat(ao_total.m_stat_us_connect , (*i_t)->m_stat.m_stat_us_connect);
                add_stat(ao_total.m_stat_us_first_response , (*i_t)->m_stat.m_stat_us_first_response);
                add_stat(ao_total.m_stat_us_end_to_end , (*i_t)->m_stat.m_stat_us_end_to_end);
        }
        // -------------------------------------------------
        // calc'd stats
        // -------------------------------------------------
        uint64_t l_delta_reqs = ao_total.m_reqs - s_last.m_reqs;
        uint64_t l_delta_resp = ao_total.m_resp - s_last.m_resp;
        //NDBG_PRINT("l_delta_resp: %lu\n", l_delta_resp);
        if(l_delta_resp > 0)
        {
                ao_total_calc.m_resp_status_2xx_pcnt = 100.0*((float)(ao_total.m_resp_status_2xx - s_last.m_resp_status_2xx))/((float)l_delta_resp);
                ao_total_calc.m_resp_status_3xx_pcnt = 100.0*((float)(ao_total.m_resp_status_3xx - s_last.m_resp_status_3xx))/((float)l_delta_resp);
                ao_total_calc.m_resp_status_4xx_pcnt = 100.0*((float)(ao_total.m_resp_status_4xx - s_last.m_resp_status_4xx))/((float)l_delta_resp);
                ao_total_calc.m_resp_status_5xx_pcnt = 100.0*((float)(ao_total.m_resp_status_5xx - s_last.m_resp_status_5xx))/((float)l_delta_resp);
        }
        if(l_delta_ms > 0)
        {
                ao_total_calc.m_req_s = ((float)l_delta_reqs*1000)/((float)l_delta_ms);
                ao_total_calc.m_resp_s = ((float)l_delta_reqs*1000)/((float)l_delta_ms);
                ao_total_calc.m_bytes_read_s = ((float)((ao_total.m_bytes_read - s_last.m_bytes_read)*1000))/((float)l_delta_ms);
                ao_total_calc.m_bytes_write_s = ((float)((ao_total.m_bytes_written - s_last.m_bytes_written)*1000))/((float)l_delta_ms);
        }
        // copy
        s_last = ao_total;
        s_stat_last_time_ms = l_cur_time_ms;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void get_status_codes(status_code_count_map_t &ao_map, t_hurl_list_t &a_t_hurl_list)
{
        for(t_hurl_list_t::iterator i_t = a_t_hurl_list.begin();
            i_t != a_t_hurl_list.end();
            ++i_t)
        {
                status_code_count_map_t i_m = (*i_t)->m_status_code_count_map;
                for(status_code_count_map_t::iterator i_c = i_m.begin();
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
static int kbhit()
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
static void nonblock(int state)
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
//: \details: Print the version.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_version(FILE* a_stream, int a_exit_code)
{
        // print out the version information
        fprintf(a_stream, "hurl HTTP Load Tester.\n");
        fprintf(a_stream, "Copyright (C) 2017 Verizon Digital Media.\n");
        fprintf(a_stream, "               Version: %s\n", HURL_VERSION);
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
        fprintf(a_stream, "  -o, --output         Output results to file <FILE> -default to stdout\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Debug Options:\n");
        fprintf(a_stream, "  -r, --trace          Turn on tracing (error/warn/debug/verbose/all)\n");
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
        int64_t l_num_to_request = -1;
        uint32_t l_interval_ms = 500;
        uint32_t l_num_parallel = 100;
        uint64_t l_start_time_ms = 0;
        int32_t l_run_time_ms = -1;
        bool l_show_response_codes = false;
        bool l_show_per_interval = true;
        bool l_display_results_json_flag = false;
        // TODO Make default definitions
        bool l_input_flag = false;
        bool l_wildcarding = true;
        std::string l_output_file = "";
        ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_NONE);
        ns_hurl::tls_init();
        SSL_CTX *l_ctx = NULL;
        std::string l_unused;
        l_ctx = ns_hurl::tls_init_ctx(l_unused,   // ctx cipher list str
                                      0,          // ctx options
                                      l_unused,   // ctx ca file
                                      l_unused,   // ctx ca path
                                      false,      // is server?
                                      l_unused,   // tls key
                                      l_unused);  // tls crt
        // TODO check result...
        if(isatty(fileno(stdout)))
        {
                g_color = true;
        }
        // -------------------------------------------------
        // Subrequest settings
        // -------------------------------------------------
        request *l_request = new request();
        l_request->m_uid = get_next_request_uuid();
        l_request->m_save = false;
        // Default headers
        l_request->set_header("User-Agent","hurl Server Load Tester");
        l_request->set_header("Accept","*/*");
        l_request->m_keepalive = true;
        //l_subr->set_num_reqs_per_conn(1);
        // Initialize rand...
        g_path_rand_ptr = (tinymt64_t*)calloc(1, sizeof(tinymt64_t));
        tinymt64_init(g_path_rand_ptr, ns_hurl::get_time_us());
        // Initialize mutex for sequential path requesting
        pthread_mutex_init(&g_path_vector_mutex, NULL);
        // Completion
        pthread_mutex_init(&g_completion_mutex, NULL);
        // -------------------------------------------------
        // Get args...
        // -------------------------------------------------
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
                { "output",         1, 0, 'o' },
                { "update",         1, 0, 'U' },
                { "trace",          1, 0, 'r' },
#ifdef ENABLE_PROFILER
                { "gprofile",       1, 0, 'G' },
#endif
                // list sentinel
                { 0, 0, 0, 0 }
        };
        // -------------------------------------------------
        // Assume unspecified arg url...
        // TODO Unsure if good way to allow unspecified
        // arg...
        // -------------------------------------------------
        std::string l_url;
#ifdef ENABLE_PROFILER
        std::string l_gprof_file;
#endif
        bool is_opt = false;
        for(int i_arg = 1; i_arg < argc; ++i_arg)
        {
                if(argv[i_arg][0] == '-')
                {
                        // next arg is for the option
                        is_opt = true;
                }
                else if((argv[i_arg][0] != '-') &&
                        (is_opt == false))
                {
                        // Stuff in url field...
                        l_url = std::string(argv[i_arg]);
                        //if(g_verbose)
                        //{
                        //      NDBG_PRINT("Found unspecified argument: %s --assuming url...\n", l_url.c_str());
                        //}
                        l_input_flag = true;
                        break;
                } else
                {
                        // reset option flag
                        is_opt = false;
                }
        }
#ifdef ENABLE_PROFILER
        char l_short_arg_list[] = "hVwd:p:f:N:t:H:X:A:M:l:T:xvcqCLjo:U:r:G:";
#else
        char l_short_arg_list[] = "hVwd:p:f:N:t:H:X:A:M:l:T:xvcqCLjo:U:r:";
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
                // -----------------------------------------
                // Help
                // -----------------------------------------
                case 'h':
                {
                        print_usage(stdout, 0);
                        break;
                }
                // -----------------------------------------
                // Version
                // -----------------------------------------
                case 'V':
                {
                        print_version(stdout, 0);
                        break;
                }
                // -----------------------------------------
                // Wildcarding
                // -----------------------------------------
                case 'w':
                {
                        l_wildcarding = false;
                        break;
                }
                // -----------------------------------------
                // Data
                // -----------------------------------------
                case 'd':
                {
                        // TODO Size limits???
                        int32_t l_s;
                        // If a_data starts with @ assume file
                        if(l_arg[0] == '@')
                        {
                                char *l_buf;
                                uint32_t l_len;
                                l_s = ns_hurl::read_file(l_arg.data() + 1, &(l_buf), &(l_len));
                                if(l_s != HURL_STATUS_OK)
                                {
                                        printf("Error reading body data from file: %s\n", l_arg.c_str() + 1);
                                        return HURL_STATUS_ERROR;
                                }
                                l_request->m_body_data = l_buf;
                                l_request->m_body_data_len = l_len;
                        }
                        else
                        {
                                char *l_buf;
                                uint32_t l_len;
                                l_len = l_arg.length() + 1;
                                l_buf = (char *)malloc(sizeof(char)*l_len);
                                l_request->m_body_data = l_buf;
                                l_request->m_body_data_len = l_len;
                        }

                        // Add content length
                        char l_len_str[64];
                        sprintf(l_len_str, "%u", l_request->m_body_data_len);
                        l_request->set_header("Content-Length", l_len_str);
                        break;
                }
                // -----------------------------------------
                // parallel
                // -----------------------------------------
                case 'p':
                {
                        int32_t l_num_parallel = atoi(optarg);
                        if (l_num_parallel < 1)
                        {
                                printf("Error num parallel must be at least 1\n");
                                return HURL_STATUS_ERROR;
                        }
                        l_num_parallel = l_num_parallel;
                        break;
                }
                // -----------------------------------------
                // fetches
                // -----------------------------------------
                case 'f':
                {
                        int32_t l_end_fetches = atoi(optarg);
                        if (l_end_fetches < 1)
                        {
                                printf("Error fetches must be at least 1\n");
                                return HURL_STATUS_ERROR;
                        }
                        l_num_to_request = l_end_fetches;
                        break;
                }
                // -----------------------------------------
                // number of calls per connection
                // -----------------------------------------
                case 'N':
                {
                        int l_val = atoi(optarg);
                        if(l_val < 1)
                        {
                                printf("Error num-calls must be at least 1");
                                return HURL_STATUS_ERROR;
                        }
                        g_reqs_per_conn = l_val;
                        if (g_reqs_per_conn == 1)
                        {
                                l_request->m_keepalive = false;
                        }
                        break;
                }
                // -----------------------------------------
                // num threads
                // -----------------------------------------
                case 't':
                {
                        //NDBG_PRINT("arg: --threads: %s\n", l_arg.c_str());
                        int l_val = atoi(optarg);
                        if (l_val < 0)
                        {
                                printf("Error num-threads must be 0 or greater\n");
                                return HURL_STATUS_ERROR;
                        }
                        g_num_threads = l_val;
                        break;
                }
                // -----------------------------------------
                // Header
                // -----------------------------------------
                case 'H':
                {
                        int32_t l_s;
                        std::string l_key;
                        std::string l_val;
                        l_s = ns_hurl::break_header_string(l_arg, l_key, l_val);
                        if (l_s != 0)
                        {
                                printf("Error breaking header string: %s -not in <HEADER>:<VAL> format?\n", l_arg.c_str());
                                return HURL_STATUS_ERROR;
                        }
                        l_s = l_request->set_header(l_key, l_val);
                        if (l_s != 0)
                        {
                                printf("Error performing set_header: %s\n", l_arg.c_str());
                                return HURL_STATUS_ERROR;
                        }
                        break;
                }
                // -----------------------------------------
                // Verb
                // -----------------------------------------
                case 'X':
                {
                        if(l_arg.length() > 64)
                        {
                                printf("Error verb string: %s too large try < 64 chars\n", l_arg.c_str());
                                return HURL_STATUS_ERROR;
                        }
                        l_request->m_verb = l_arg;
                        break;
                }
                // -----------------------------------------
                // rate
                // -----------------------------------------
                case 'A':
                {
                        int l_rate = atoi(optarg);
                        if (l_rate < 1)
                        {
                                printf("Error: rate must be at least 1\n");
                                //print_usage(stdout, -1);
                                return HURL_STATUS_ERROR;
                        }
                        g_rate_delta_us = 1000000 / l_rate;
                        break;
                }
                // -----------------------------------------
                // Mode
                // -----------------------------------------
                case 'M':
                {
                        std::string l_order = optarg;
                        if(l_order == "sequential") { g_path_order_random = false; }
                        else if(l_order == "random"){ g_path_order_random = true;}
                        else
                        {
                                printf("Error: Mode must be [roundrobin|sequential|random]\n");
                                //print_usage(stdout, -1);
                                return HURL_STATUS_ERROR;
                        }
                        break;
                }
                // -----------------------------------------
                // seconds
                // -----------------------------------------
                case 'l':
                {
                        int l_run_time_s = atoi(optarg);
                        if (l_run_time_s < 1)
                        {
                                printf("Error: seconds must be at least 1\n");
                                //print_usage(stdout, -1);
                                return HURL_STATUS_ERROR;
                        }
                        l_run_time_ms = l_run_time_s*1000;
                        break;
                }
                // -----------------------------------------
                // timeout
                // -----------------------------------------
                case 'T':
                {
                        //NDBG_PRINT("arg: --fetches: %s\n", optarg);
                        //g_end_type = END_FETCHES;
                        int l_subreq_timeout_s = atoi(optarg);
                        if (l_subreq_timeout_s < 1)
                        {
                                printf("timeout must be > 0\n");
                                //print_usage(stdout, -1);
                                return HURL_STATUS_ERROR;
                        }
                        l_request->m_timeout_ms = l_subreq_timeout_s*1000;
                        break;
                }
                // -----------------------------------------
                // No stats
                // -----------------------------------------
                case 'x':
                {
                        g_stats = false;
                        break;
                }
                // -----------------------------------------
                // verbose
                // -----------------------------------------
                case 'v':
                {
                        g_verbose = true;
                        l_request->m_save = true;
                        break;
                }
                // -----------------------------------------
                // color
                // -----------------------------------------
                case 'c':
                {
                        g_color = false;
                        break;
                }
                // -----------------------------------------
                // quiet
                // -----------------------------------------
                case 'q':
                {
                        g_quiet = true;
                        break;
                }
                // -----------------------------------------
                // responses
                // -----------------------------------------
                case 'C':
                {
                        l_show_response_codes = true;
                        break;
                }
                // -----------------------------------------
                // per_interval
                // -----------------------------------------
                case 'L':
                {
                        l_show_response_codes = true;
                        l_show_per_interval = true;
                        break;
                }
                // -----------------------------------------
                // json
                // -----------------------------------------
                case 'j':
                {
                        l_display_results_json_flag = true;
                        break;
                }
                // -----------------------------------------
                // output
                // -----------------------------------------
                case 'o':
                {
                        l_output_file = optarg;
                        break;
                }
                // -----------------------------------------
                // Update interval
                // -----------------------------------------
                case 'U':
                {
                        int l_interval_ms = atoi(optarg);
                        if (l_interval_ms < 1)
                        {
                                printf("Error: Update interval must be > 0 ms\n");
                                //print_usage(stdout, -1);
                                return HURL_STATUS_ERROR;
                        }
                        l_interval_ms = l_interval_ms;
                        break;
                }
                // -----------------------------------------
                // trace
                // -----------------------------------------
                case 'r':
                {

#define ELIF_TRACE_STR(_level) else if(strncasecmp(_level, l_arg.c_str(), sizeof(_level)) == 0)

                        bool l_trace = false;
                        if(0) {}
                        ELIF_TRACE_STR("error") { ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_ERROR); l_trace = true; }
                        ELIF_TRACE_STR("warn") { ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_WARN); l_trace = true; }
                        ELIF_TRACE_STR("debug") { ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_DEBUG); l_trace = true; }
                        ELIF_TRACE_STR("verbose") { ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_VERBOSE); l_trace = true; }
                        ELIF_TRACE_STR("all") { ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_ALL); l_trace = true; }
                        else
                        {
                                ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_NONE);
                        }
                        if(l_trace)
                        {
                                ns_hurl::trc_log_file_open("/dev/stdout");
                        }
                        break;
                }
#ifdef ENABLE_PROFILER
                // -----------------------------------------
                // Google Profiler Output File
                // -----------------------------------------
                case 'G':
                {
                        l_gprof_file = l_arg;
                        break;
                }
#endif
                // -----------------------------------------
                // What???
                // -----------------------------------------
                case '?':
                {
                        // Required argument was missing
                        // '?' is provided when the 3rd arg to getopt_long does not begin with a ':', and is preceeded
                        // by an automatic error message.
                        fprintf(stdout, "  Exiting.\n");
                        print_usage(stdout, -1);
                        break;
                }
                // -----------------------------------------
                // Huh???
                // -----------------------------------------
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
                return HURL_STATUS_ERROR;
        }
        if(l_rlim.rlim_cur < (uint64_t)(g_num_threads*l_num_parallel))
        {
                fprintf(stdout, "Error threads[%d]*parallelism[%d] > process fd resource limit[%u]\n",
                                g_num_threads, l_num_parallel, (uint32_t)l_rlim.rlim_cur);
                return HURL_STATUS_ERROR;
        }
        // -------------------------------------------
        // Sigint handler
        // -------------------------------------------
        if (signal(SIGINT, sig_handler) == SIG_ERR)
        {
                printf("Error: can't catch SIGINT\n");
                return HURL_STATUS_ERROR;
        }
        // -------------------------------------------
        // Add url from command line
        // -------------------------------------------
        //printf("Adding url: %s\n", l_url.c_str());
        // Set url
        l_s = l_request->init_with_url(l_url);
        if(l_s != 0)
        {
                printf("Error: performing init_with_url: %s\n", l_url.c_str());
                return HURL_STATUS_ERROR;
        }
        l_request->m_completion_cb = s_completion_cb;
        // -------------------------------------------
        // Paths...
        // -------------------------------------------
        std::string l_raw_path = l_request->m_url_path;
        //printf("l_raw_path: %s\n",l_raw_path.c_str());
        if(l_wildcarding)
        {
                int32_t l_s;
                l_s = special_effects_parse(l_raw_path);
                if(l_s != HURL_STATUS_OK)
                {
                        printf("Error performing special_effects_parse with path: %s\n", l_raw_path.c_str());
                        return HURL_STATUS_ERROR;
                }
                if(g_path_vector.size() > 1)
                {
                        g_path_multi = true;
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
        // -------------------------------------------
        // message
        // -------------------------------------------
        if(!g_quiet && !g_verbose)
        {
                if(g_reqs_per_conn < 0)
                {
                        fprintf(stdout, "Running %d threads %d parallel connections per thread with infinite requests per connection\n",
                                g_num_threads, l_num_parallel);
                }
                else
                {
                        fprintf(stdout, "Running %d threads %d parallel connections per thread with %ld requests per connection\n",
                                        g_num_threads, l_num_parallel, g_reqs_per_conn);
                }
        }
        // -------------------------------------------
        // resolve
        // -------------------------------------------
        ns_hurl::host_info l_host_info;
        l_s = ns_hurl::nlookup(l_request->m_host, l_request->m_port, l_host_info);
        if(l_s != HURL_STATUS_OK)
        {
                printf("Error: resolving: %s:%d\n", l_request->m_host.c_str(), l_request->m_port);
                return HURL_STATUS_ERROR;
        }
        l_request->m_host_info = l_host_info;
        // -------------------------------------------
        // init
        // -------------------------------------------
        static t_hurl_list_t l_t_hurl_list;
        for(uint32_t i_t = 0; i_t < g_num_threads; ++i_t)
        {
                // Calculate num to request
                int32_t l_num_to_request_per = -1;
                if(l_num_to_request > 0)
                {
                        // first thread gets remainder
                        l_num_to_request_per = l_num_to_request / g_num_threads;
                        if(i_t == 0)
                        {
                                l_num_to_request_per += l_num_to_request % g_num_threads;
                        }
                }
                t_hurl *l_t_hurl = new t_hurl(*l_request, l_num_parallel, l_num_to_request_per);
                l_t_hurl_list.push_back(l_t_hurl);
                l_t_hurl->init();
                // TODO Check status
        }
        // -------------------------------------------
        // run
        // -------------------------------------------
        l_start_time_ms = ns_hurl::get_time_ms();;
        for(t_hurl_list_t::iterator i_t = l_t_hurl_list.begin();
            i_t != l_t_hurl_list.end();
            ++i_t)
        {
                (*i_t)->m_ctx = l_ctx;
                (*i_t)->run();
                // TODO Check status
        }
        // -------------------------------------------
        // *******************************************
        //              c o l o r s
        // *******************************************
        // -------------------------------------------
        const char *l_c_fg_white = NULL;
        const char *l_c_fg_red = NULL;
        const char *l_c_fg_blue = NULL;
        const char *l_c_fg_cyan = NULL;
        const char *l_c_fg_magenta = NULL;
        const char *l_c_fg_green = NULL;
        const char *l_c_fg_yellow = NULL;
        const char *l_c_off = NULL;
        if(g_color)
        {
                l_c_fg_white = ANSI_COLOR_FG_WHITE;
                l_c_fg_red = ANSI_COLOR_FG_RED;
                l_c_fg_blue = ANSI_COLOR_FG_BLUE;
                l_c_fg_cyan = ANSI_COLOR_FG_CYAN;
                l_c_fg_magenta = ANSI_COLOR_FG_MAGENTA;
                l_c_fg_green = ANSI_COLOR_FG_GREEN;
                l_c_fg_yellow = ANSI_COLOR_FG_YELLOW;
                l_c_off = ANSI_COLOR_OFF;
        }
        // -------------------------------------------
        // *******************************************
        //              command exec
        // *******************************************
        // -------------------------------------------
        int i = 0;
        char l_cmd = ' ';
        bool l_first_time = true;
        nonblock(NB_ENABLE);
        // -------------------------------------------
        // Loop forever until user quits
        // -------------------------------------------
        while (!g_test_finished)
        {
                i = kbhit();
                if (i != 0)
                {
                        l_cmd = fgetc(stdin);
                        switch (l_cmd)
                        {
                        // ---------------------------------
                        // Quit
                        // ---------------------------------
                        case 'q':
                        {
                                g_test_finished = true;
                                break;
                        }
                        // ---------------------------------
                        // default
                        // ---------------------------------
                        default:
                        {
                                break;
                        }
                        }
                }
                // TODO add define...
                usleep(l_interval_ms*1000);
                // Check for done
                if(l_run_time_ms != -1)
                {
                        int32_t l_time_delta_ms = (int32_t)(ns_hurl::get_delta_time_ms(l_start_time_ms));
                        if(l_time_delta_ms >= l_run_time_ms)
                        {
                                g_test_finished = true;
                        }
                }
                bool l_is_running = false;
                for (t_hurl_list_t::iterator i_t = l_t_hurl_list.begin();
                     i_t != l_t_hurl_list.end();
                     ++i_t)
                {
                        if(!(*i_t)->m_stopped) l_is_running = true;
                }
                if(!l_is_running)
                {
                        g_test_finished = true;
                }
                if(g_quiet || g_verbose)
                {
                        // skip stats
                        continue;
                }
                if(l_first_time)
                {
                if(l_show_response_codes)
                {
                printf("+-----------+-------------+-----------+-----------+-----------+-----------+-----------+-----------+\n");
                if(l_show_per_interval)
                {
                printf("| %s%9s%s / %s%11s%s / %s%9s%s / %s%9s%s / %s%9s%s | %s%9s%s | %s%9s%s | %s%9s%s | \n",
                                l_c_fg_white, "Elapsed", l_c_off,
                                l_c_fg_white, "Rsp/s", l_c_off,
                                l_c_fg_white, "Cmpltd", l_c_off,
                                l_c_fg_white, "Errors", l_c_off,
                                l_c_fg_green, "200s %%", l_c_off,
                                l_c_fg_cyan, "300s %%", l_c_off,
                                l_c_fg_magenta, "400s %%", l_c_off,
                                l_c_fg_red, "500s %%", l_c_off);
                }
                else
                {
                printf("| %s%9s%s / %s%11s%s / %s%9s%s / %s%9s%s / %s%9s%s | %s%9s%s | %s%9s%s | %s%9s%s | \n",
                                l_c_fg_white, "Elapsed", l_c_off,
                                l_c_fg_white, "Req/s", l_c_off,
                                l_c_fg_white, "Cmpltd", l_c_off,
                                l_c_fg_white, "Errors", l_c_off,
                                l_c_fg_green, "200s", l_c_off,
                                l_c_fg_cyan, "300s", l_c_off,
                                l_c_fg_magenta, "400s", l_c_off,
                                l_c_fg_red, "500s", l_c_off);
                }
                printf("+-----------+-------------+-----------+-----------+-----------+-----------+-----------+-----------+\n");
                }
                else
                {
                printf("+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+\n");
                printf("| %s%9s%s / %s%9s%s | %s%9s%s | %s%9s%s | %s%12s%s | %9s | %11s | %9s |\n",
                                l_c_fg_green, "Completed", l_c_off,
                                l_c_fg_blue, "Requested", l_c_off,
                                l_c_fg_magenta, "IdlKil", l_c_off,
                                l_c_fg_red, "Errors", l_c_off,
                                l_c_fg_yellow, "kBytes Recvd", l_c_off,
                                "Elapsed",
                                "Req/s",
                                "MB/s");
                printf("+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+\n");
                }
                }
                // Get stats
                t_stat_cntr_t l_total;
                t_stat_calc_t l_total_calc;
                t_stat_cntr_list_t l_thread;
                get_stat(l_total, l_total_calc, l_thread, l_t_hurl_list);
                // skip stat display first time
                if(l_first_time) { l_first_time = false; continue; }
                if(l_show_response_codes)
                {
                if(l_show_per_interval)
                {
                printf("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %s%9.2f%s | %s%9.2f%s | %s%9.2f%s | %s%9.2f%s |\n",
                                ((double)(ns_hurl::get_delta_time_ms(l_start_time_ms))) / 1000.0,
                                l_total_calc.m_resp_s,
                                l_total.m_resp,
                                l_total.m_errors,
                                l_c_fg_green, l_total_calc.m_resp_status_2xx_pcnt, l_c_off,
                                l_c_fg_cyan, l_total_calc.m_resp_status_3xx_pcnt, l_c_off,
                                l_c_fg_magenta, l_total_calc.m_resp_status_4xx_pcnt, l_c_off,
                                l_c_fg_red, l_total_calc.m_resp_status_5xx_pcnt, l_c_off);
                }
                else
                {
                // Aggregate over status code map
                uint32_t l_responses[10] = {0};
                status_code_count_map_t l_status_code_count_map;
                get_status_codes(l_status_code_count_map, l_t_hurl_list);
                for(status_code_count_map_t::iterator i_code = l_status_code_count_map.begin();
                    i_code != l_status_code_count_map.end();
                    ++i_code) {
                        if(0) {}
                        else if(i_code->first >= 200 && i_code->first <= 299) { l_responses[2] += i_code->second;}
                        else if(i_code->first >= 300 && i_code->first <= 399) { l_responses[3] += i_code->second;}
                        else if(i_code->first >= 400 && i_code->first <= 499) { l_responses[4] += i_code->second;}
                        else if(i_code->first >= 500 && i_code->first <= 599) { l_responses[5] += i_code->second;}
                }
                printf("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %s%9u%s | %s%9u%s | %s%9u%s | %s%9u%s |\n",
                                ((double)(ns_hurl::get_delta_time_ms(l_start_time_ms))) / 1000.0,
                                l_total_calc.m_resp_s,
                                l_total.m_resp,
                                l_total.m_errors,
                                l_c_fg_green, l_responses[2], l_c_off,
                                l_c_fg_cyan, l_responses[3], l_c_off,
                                l_c_fg_magenta, l_responses[4], l_c_off,
                                l_c_fg_red, l_responses[5], l_c_off);
                }
                }
                else
                {
                printf("| %s%9" PRIu64 "%s / %s%9" PRIi64 "%s | %s%9" PRIu64 "%s | %s%9" PRIu64 "%s | %s%12.2f%s | %8.2fs | %10.2fs | %8.2fs |\n",
                                l_c_fg_green, l_total.m_resp, l_c_off,
                                l_c_fg_blue, l_total.m_reqs, l_c_off,
                                l_c_fg_magenta, l_total.m_idle_killed, l_c_off,
                                l_c_fg_red, l_total.m_errors, l_c_off,
                                l_c_fg_yellow, l_total_calc.m_bytes_read_s/1024.0, l_c_off,
                                ((double)(ns_hurl::get_delta_time_ms(l_start_time_ms))) / 1000.0,
                                l_total_calc.m_req_s,
                                l_total_calc.m_bytes_read_s/(1024.0*1024.0)
                                );
                }
        }
        nonblock(NB_DISABLE);
        uint64_t l_end_time_ms = ns_hurl::get_time_ms() - l_start_time_ms;
        // -------------------------------------------
        // stop
        // -------------------------------------------
        for(t_hurl_list_t::iterator i_t = l_t_hurl_list.begin();
            i_t != l_t_hurl_list.end();
            ++i_t)
        {
                (*i_t)->stop();
        }
        // -------------------------------------------
        // Join all threads before exit
        // -------------------------------------------
        for(t_hurl_list_t::iterator i_t = l_t_hurl_list.begin();
            i_t != l_t_hurl_list.end();
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
        // -------------------------------------------
        // Get stats
        // -------------------------------------------
        status_code_count_map_t l_status_code_count_map;
        t_stat_cntr_t l_total;
        t_stat_calc_t l_total_calc;
        t_stat_cntr_list_t l_thread;
        get_stat(l_total, l_total_calc, l_thread, l_t_hurl_list);
        uint64_t l_total_bytes = l_total.m_bytes_read + l_total.m_bytes_written;
        get_status_codes(l_status_code_count_map, l_t_hurl_list);
        double l_elapsed_time_s = ((double)l_end_time_ms)/1000.0;
        // -------------------------------------------
        // results str
        // -------------------------------------------
        if(!l_display_results_json_flag)
        {
                std::string l_tag;
                char l_buf[1024];
                // TODO Fix elapse and max parallel
                l_tag = "ALL";
#define STR_PRINT(...) do { snprintf(l_buf,1024,__VA_ARGS__); l_out_str+=l_buf;} while(0)
                STR_PRINT("| %sRESULTS%s:             %s%s%s\n", l_c_fg_cyan, l_c_off, l_c_fg_yellow, l_tag.c_str(), l_c_off);
                STR_PRINT("| fetches:             %" PRIu64 "\n", l_total.m_resp);
                STR_PRINT("| max parallel:        %u\n", l_num_parallel);
                STR_PRINT("| bytes:               %e\n", (double)(l_total_bytes));
                STR_PRINT("| seconds:             %f\n", l_elapsed_time_s);
                STR_PRINT("| mean bytes/conn:     %f\n", ((double)l_total_bytes)/((double)l_total.m_resp));
                STR_PRINT("| fetches/sec:         %f\n", ((double)l_total.m_resp)/(l_elapsed_time_s));
                STR_PRINT("| bytes/sec:           %e\n", ((double)l_total_bytes)/l_elapsed_time_s);
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
                SHOW_XSTAT_LINE("ms/connect:", l_total.m_stat_us_connect);
                SHOW_XSTAT_LINE("ms/1st-response:", l_total.m_stat_us_first_response);
                SHOW_XSTAT_LINE("ms/end2end:", l_total.m_stat_us_end_to_end);
                STR_PRINT("| %sHTTP response codes%s: \n", l_c_fg_green, l_c_off);
                for(status_code_count_map_t::const_iterator i_status_code = l_status_code_count_map.begin();
                    i_status_code != l_status_code_count_map.end();
                    ++i_status_code)
                {
                        STR_PRINT("| %s%3d%s -- %u\n", l_c_fg_magenta, i_status_code->first, l_c_off, i_status_code->second);
                }
        }
        else
        {
                rapidjson::Document l_body;
                l_body.SetObject();
                rapidjson::Document::AllocatorType& l_alloc = l_body.GetAllocator();
#define ADD_MEMBER(_l, _v) l_body.AddMember(_l, _v, l_alloc)
                ADD_MEMBER("fetches", l_total.m_resp);
                ADD_MEMBER("max-parallel", l_num_parallel);
                ADD_MEMBER("bytes", (double)(l_total_bytes));
                ADD_MEMBER("seconds", l_elapsed_time_s);
                ADD_MEMBER("mean-bytes-per-conn", ((double)l_total_bytes)/((double)l_total.m_resp));
                ADD_MEMBER("fetches-per-sec", ((double)l_total.m_resp)/(l_elapsed_time_s));
                ADD_MEMBER("bytes-per-sec", ((double)l_total_bytes)/l_elapsed_time_s);
                // TODO Fix stdev/var calcs
                ADD_MEMBER("connect-ms-mean", l_total.m_stat_us_connect.mean()/1000.0);
                ADD_MEMBER("connect-ms-max", l_total.m_stat_us_connect.max()/1000.0);
                ADD_MEMBER("connect-ms-min", l_total.m_stat_us_connect.min()/1000.0);
                ADD_MEMBER("1st-resp-ms-mean", l_total.m_stat_us_first_response.mean()/1000.0);
                ADD_MEMBER("1st-resp-ms-max", l_total.m_stat_us_first_response.max()/1000.0);
                ADD_MEMBER("1st-resp-ms-min", l_total.m_stat_us_first_response.min()/1000.0);
                ADD_MEMBER("end2end-ms-mean", l_total.m_stat_us_end_to_end.mean()/1000.0);
                ADD_MEMBER("end2end-ms-max", l_total.m_stat_us_end_to_end.max()/1000.0);
                ADD_MEMBER("end2end-ms-min", l_total.m_stat_us_end_to_end.min()/1000.0);
                if(l_status_code_count_map.size())
                {
                rapidjson::Value l_obj;
                l_obj.SetObject();
                for(status_code_count_map_t::const_iterator i_status_code = l_status_code_count_map.begin();
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
                l_out_str.assign(l_strbuf.GetString(), l_strbuf.GetSize());
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
                FILE *l_file_ptr = fopen(l_output_file.c_str(), "w+");
                if(l_file_ptr == NULL)
                {
                        printf("Error performing fopen. Reason: %s\n", strerror(errno));
                        return HURL_STATUS_ERROR;
                }
                l_num_bytes_written = fwrite(l_out_str.c_str(), 1, l_out_str.length(), l_file_ptr);
                if(l_num_bytes_written != (int32_t)l_out_str.length())
                {
                        printf("Error performing fwrite. Reason: %s\n", strerror(errno));
                        fclose(l_file_ptr);
                        return HURL_STATUS_ERROR;
                }
                l_s = fclose(l_file_ptr);
                if(l_s != 0)
                {
                        printf("Error performing fclose. Reason: %s\n", strerror(errno));
                        return HURL_STATUS_ERROR;
                }
        }
        // -------------------------------------------
        // Cleanup...
        // -------------------------------------------
        for(t_hurl_list_t::iterator i_t = l_t_hurl_list.begin();
            i_t != l_t_hurl_list.end();
            ++i_t)
        {
                if(*i_t)
                {
                        delete *i_t;
                        *i_t = NULL;
                }
        }
        l_t_hurl_list.clear();
        if(l_request)
        {
                delete l_request;
                l_request = NULL;
        }
        if(l_ctx)
        {
                SSL_CTX_free(l_ctx);
                l_ctx = NULL;
        }
        return 0;
}
