//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    phurl.cc
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
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "http_parser/http_parser.h"
// hurl
#include "hurl/status.h"
#include "hurl/support/atomic.h"
#include "hurl/support/trace.h"
#include "hurl/support/kv_map_list.h"
#include "hurl/support/string_util.h"
#include "hurl/support/nbq.h"
#include "hurl/support/nbq_stream.h"
#include "hurl/nconn/host_info.h"
#include "hurl/nconn/scheme.h"
#include "hurl/http/http_status.h"
#include "hurl/http/resp.h"
#include "hurl/http/api_resp.h"
#include "hurl/dns/nresolver.h"
#include "hurl/support/tls_util.h"
#include "hurl/nconn/nconn.h"
#include "hurl/nconn/nconn_tcp.h"
#include "hurl/nconn/nconn_tls.h"
#include "hurl/http/cb.h"
// internal
#include "support/ndebug.h"
#include "support/file_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <termios.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
// Get resource limits
#include <sys/resource.h>
#include <string>
#include <list>
#include <queue>
#include <algorithm>
// Profiler
#ifdef ENABLE_PROFILER
#include <gperftools/profiler.h>
#include <gperftools/heap-profiler.h>
#endif
// TLS
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define NB_ENABLE  1
#define NB_DISABLE 0

#define MAX_READLINE_SIZE 1024

#ifndef STATUS_OK
#define STATUS_OK 0
#endif
#ifndef STATUS_ERROR
#define STATUS_ERROR -1
#endif

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------
// ---------------------------------------
// Output types
// ---------------------------------------
typedef enum {
        OUTPUT_LINE_DELIMITED,
        OUTPUT_JSON
} output_type_t;

// ---------------------------------------
// Output types
// ---------------------------------------
typedef enum {
        PART_HOST = 1,
        PART_SERVER = 1 << 1,
        PART_STATUS_CODE = 1 << 2,
        PART_HEADERS = 1 << 3,
        PART_BODY = 1 << 4
} output_part_t;

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define UNUSED(x) ( (void)(x) )
#define ARESP(_str) l_responses_str += _str

#define CHECK_FOR_NULL_ERROR_DEBUG(_data) \
        do {\
                if(!_data) {\
                        NDBG_PRINT("Error.\n");\
                        return STATUS_ERROR;\
                }\
        } while(0);

#define CHECK_FOR_NULL_ERROR(_data) \
        do {\
                if(!_data) {\
                        return STATUS_ERROR;\
                }\
        } while(0);

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
class t_phurl;
typedef std::list <t_phurl *> t_phurl_list_t;

// -----------------------------------------------
// request object/meta
// -----------------------------------------------
class request {
public:
        request():
                m_nconn(NULL),
                m_t_phurl(NULL),
                m_timer_obj(NULL),
                m_resp(NULL),
                m_in_q(NULL),
                m_out_q(NULL),
                m_host(),
                m_hostname(),
                m_id(),
                m_where(),
                m_url(),
                m_port(0),
                m_tls_info_cipher_str(),
                m_tls_info_protocol_str(),
                m_error_str(),
#ifdef ASYNC_DNS_SUPPORT
                m_adns_lookup_job_handle(NULL),
#endif
                m_host_info(),
                m_host_resolved(false)
        {};
        // -------------------------------------------------
        // Public Static (class) methods
        // -------------------------------------------------
        static int32_t evr_fd_readable_cb(void *a_data) {return run_state_machine(a_data, ns_hurl::EVR_MODE_READ);}
        static int32_t evr_fd_writeable_cb(void *a_data){return run_state_machine(a_data, ns_hurl::EVR_MODE_WRITE);}
        static int32_t evr_fd_error_cb(void *a_data) {return run_state_machine(a_data, ns_hurl::EVR_MODE_ERROR);}
        static int32_t evr_fd_timeout_cb(void *a_ctx, void *a_data){return run_state_machine(a_data, ns_hurl::EVR_MODE_TIMEOUT);}
        ns_hurl::nconn *m_nconn;
        t_phurl *m_t_phurl;
        ns_hurl::evr_timer_t *m_timer_obj;
        ns_hurl::resp *m_resp;
        ns_hurl::nbq *m_in_q;
        ns_hurl::nbq *m_out_q;

        // User defined...
        std::string m_host;
        std::string m_hostname;
        std::string m_id;
        std::string m_where;
        std::string m_url;
        uint16_t m_port;
        std::string m_tls_info_cipher_str;
        std::string m_tls_info_protocol_str;
        std::string m_error_str;
        // address resolution
#ifdef ASYNC_DNS_SUPPORT
        void *m_adns_lookup_job_handle;
#endif
        ns_hurl::host_info m_host_info;
        bool m_host_resolved;
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        request& operator=(const request &);
        request(const request &);
        int32_t teardown(ns_hurl::http_status_t a_status);
        // -------------------------------------------------
        // Private Static (class) methods
        // -------------------------------------------------
        static int32_t run_state_machine(void *a_data, ns_hurl::evr_mode_t a_conn_mode);
        // TODO -subr/rqst/resp/tls info?
};

//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------
static int32_t s_create_request(request &a_request, ns_hurl::nbq &a_nbq);

typedef std::list <request *> request_list_t;
typedef std::queue<request *> request_queue_t;

//: ----------------------------------------------------------------------------
//: Globals
//: ----------------------------------------------------------------------------

// -----------------------------------------------
// settings
// -----------------------------------------------

t_phurl_list_t g_t_phurl_list;
bool g_conf_color = true;
bool g_conf_verbose = false;
bool g_conf_show_summary = false;
uint32_t g_conf_num_threads = 1;
uint32_t g_conf_num_parallel = 100;
uint32_t g_conf_timeout_ms = 10000;
bool g_conf_connect_only = false;
uint32_t g_conf_completion_time_ms = 10000;
float g_conf_completion_ratio = 100.0;
uint32_t g_conf_num_hosts = 0;

std::string g_conf_verb = "GET";
ns_hurl::kv_map_list_t g_conf_headers;
char *g_conf_body_data = NULL;
uint32_t g_conf_body_data_len = 0;

// url
ns_hurl::scheme_t g_conf_url_scheme = ns_hurl::SCHEME_TCP;
uint16_t g_conf_url_port = 80;
std::string g_conf_url_path = "";
std::string g_conf_url_query = "";

// tls
std::string g_conf_tls_cipher_list;
long g_conf_tls_options = (SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
bool g_conf_tls_verify = false;
bool g_conf_tls_sni = false;
bool g_conf_tls_self_ok = false;
bool g_conf_tls_no_host_check = false;
std::string g_conf_tls_ca_file;
std::string g_conf_tls_ca_path;

// dns
bool g_conf_dns_use_sync = false;
bool g_conf_dns_use_ai_cache = true;
std::string g_conf_dns_ai_cache_file = "/tmp/addr_info_cache.json";

// run time
bool g_runtime_stop = false;
bool g_runtime_finished = false;
bool g_runtime_cancelled = false;

ns_hurl::atomic_gcc_builtin <uint32_t> g_dns_num_requested = 0;
ns_hurl::atomic_gcc_builtin <uint32_t> g_dns_num_in_flight = 0;
ns_hurl::atomic_gcc_builtin <uint32_t> g_dns_num_resolved = 0;
ns_hurl::atomic_gcc_builtin <uint32_t> g_dns_num_errors = 0;

ns_hurl::atomic_gcc_builtin <uint32_t> g_req_num_requested = 0;
ns_hurl::atomic_gcc_builtin <uint32_t> g_req_num_in_flight = 0;
ns_hurl::atomic_gcc_builtin <uint32_t> g_req_num_completed = 0;
ns_hurl::atomic_gcc_builtin <uint32_t> g_req_num_errors = 0;
ns_hurl::atomic_gcc_builtin <uint32_t> g_req_num_pending = 0;

ns_hurl::atomic_gcc_builtin <uint32_t> g_sum_success = 0;
ns_hurl::atomic_gcc_builtin <uint32_t> g_sum_error = 0;
ns_hurl::atomic_gcc_builtin <uint32_t> g_sum_error_addr = 0;
ns_hurl::atomic_gcc_builtin <uint32_t> g_sum_error_conn = 0;
ns_hurl::atomic_gcc_builtin <uint32_t> g_sum_error_unknown = 0;
ns_hurl::atomic_gcc_builtin <uint32_t> g_sum_error_tls_hostname = 0;
ns_hurl::atomic_gcc_builtin <uint32_t> g_sum_error_tls_self_signed = 0;
ns_hurl::atomic_gcc_builtin <uint32_t> g_sum_error_tls_expired = 0;
ns_hurl::atomic_gcc_builtin <uint32_t> g_sum_error_tls_issuer = 0;
ns_hurl::atomic_gcc_builtin <uint32_t> g_sum_error_tls_other = 0;

typedef std::map <std::string, uint32_t> summary_map_t;
pthread_mutex_t g_sum_info_mutex;
summary_map_t g_sum_info_tls_protocols;
summary_map_t g_sum_info_tls_ciphers;
//: ----------------------------------------------------------------------------
//: t_hurl
//: ----------------------------------------------------------------------------
class t_phurl
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        t_phurl(uint32_t a_max_parallel):
               m_stopped(true),
               m_t_run_thread(),
               m_orphan_in_q(NULL),
               m_orphan_out_q(NULL),
               m_ctx(NULL),
               m_request_queue(),
               m_evr_loop(NULL),
               m_num_parallel_max(a_max_parallel),
               m_num_in_progress(0),
               m_num_pending(0),
               m_is_initd(false)
        {
                m_orphan_in_q = new ns_hurl::nbq(4096);
                m_orphan_out_q = new ns_hurl::nbq(4096);
        }
        ~t_phurl()
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
                if(m_evr_loop)
                {
                        delete m_evr_loop;
                        m_evr_loop = NULL;
                }
        }
        int32_t init(void)
        {
                if(m_is_initd) return STATUS_OK;
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
                        return STATUS_ERROR;
                }
                m_is_initd = true;
                return STATUS_OK;
        }
        int run(void) {
                int32_t l_pthread_error = 0;
                l_pthread_error = pthread_create(&m_t_run_thread, NULL, t_run_static, this);
                if (l_pthread_error != 0) {
                        return STATUS_ERROR;
                }
                return STATUS_OK;
        };
        void *t_run(void *a_nothing);
        int32_t subr_try_start(void);
        void stop(void) {
                m_stopped = true;
                m_evr_loop->signal();
        }
        bool is_running(void) { return !m_stopped; }
        int32_t cancel_timer(void *a_timer) {
                if(!m_evr_loop) return STATUS_ERROR;
                if(!a_timer) return STATUS_OK;
                ns_hurl::evr_timer_t *l_t = static_cast<ns_hurl::evr_timer_t *>(a_timer);
                return m_evr_loop->cancel_timer(l_t);
        }

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        sig_atomic_t m_stopped;
        pthread_t m_t_run_thread;
        ns_hurl::nbq *m_orphan_in_q;
        ns_hurl::nbq *m_orphan_out_q;
        SSL_CTX *m_ctx;
        request_queue_t m_request_queue;
        ns_hurl::evr_loop *m_evr_loop;
        uint32_t m_num_parallel_max;
        uint32_t m_num_in_progress;
        uint32_t m_num_pending;
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        t_phurl& operator=(const t_phurl &);
        t_phurl(const t_phurl &);

        //Helper for pthreads
        static void *t_run_static(void *a_context)
        {
                return reinterpret_cast<t_phurl *>(a_context)->t_run(NULL);
        }
        ns_hurl::nconn *create_new_nconn(const request &a_request);
        int32_t request_start(request &a_request);
        int32_t request_dequeue(void);
        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        bool m_is_initd;
};
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t request::teardown(ns_hurl::http_status_t a_status)
{
        if(m_timer_obj)
        {
                m_t_phurl->m_evr_loop->cancel_timer(m_timer_obj);
                m_timer_obj = NULL;
        }
        --(m_t_phurl->m_num_pending);
        --(m_t_phurl->m_num_in_progress);
        ++g_req_num_completed;
        --g_req_num_in_flight;
        --g_req_num_pending;
        if(a_status >= 500)
        {
                ++g_req_num_errors;
                ++g_sum_error;
                // on error

                if(m_nconn)
                {
                        ns_hurl::conn_status_t l_conn_status = ns_hurl::nconn_get_status(*m_nconn);
                        //printf("%s.%s.%d: host:          %s\n",__FILE__,__FUNCTION__,__LINE__,a_subr.get_host().c_str());
                        //printf("%s.%s.%d: m_error_str:   %s\n",__FILE__,__FUNCTION__,__LINE__,l_resp->m_error_str.c_str());
                        //printf("%s.%s.%d: l_conn_status: %d\n",__FILE__,__FUNCTION__,__LINE__,l_conn_status);
                        switch(l_conn_status)
                        {
                        case ns_hurl::CONN_STATUS_CANCELLED:
                        {
                                ++(g_sum_error_conn);
                                break;
                        }
                        case ns_hurl::CONN_STATUS_ERROR_ADDR_LOOKUP_FAILURE:
                        {
                                ++(g_sum_error_addr);
                                break;
                        }
                        case ns_hurl::CONN_STATUS_ERROR_ADDR_LOOKUP_TIMEOUT:
                        {
                                ++(g_sum_error_addr);
                                break;
                        }
                        case ns_hurl::CONN_STATUS_ERROR_CONNECT_TLS:
                        {
                                ++(g_sum_error_conn);
                                // Get last error
                                if(m_nconn)
                                {
                                        SSL *l_tls = ns_hurl::nconn_get_SSL(*m_nconn);
                                        long l_tls_vr = SSL_get_verify_result(l_tls);
                                        if ((l_tls_vr == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) ||
                                            (l_tls_vr == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN))
                                        {
                                                ++(g_sum_error_tls_self_signed);
                                        }
                                        else if(l_tls_vr == X509_V_ERR_CERT_HAS_EXPIRED)
                                        {
                                                ++(g_sum_error_tls_expired);
                                        }
                                        else if((l_tls_vr == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT) ||
                                                (l_tls_vr == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY))
                                        {
                                                ++(g_sum_error_tls_issuer);
                                        }
                                        else
                                        {
                                                //long l_err = ns_hurl::nconn_get_last_SSL_err(a_nconn);
                                                //printf("ERRORXXXX: %ld\n", l_err);
                                                //printf("ERRORXXXX: %s\n", ERR_error_string(l_err,NULL));
                                                //printf("ERRORXXXX: %s\n", l_resp->m_error_str.c_str());
                                                ++(g_sum_error_tls_other);
                                        }
                                }
                                break;

                        }
                        case ns_hurl::CONN_STATUS_ERROR_CONNECT_TLS_HOST:
                        {
                                ++(g_sum_error_conn);
                                ++(g_sum_error_tls_hostname);
                                break;
                        }
                        default:
                        {
                                //printf("CONN STATUS: %d\n", l_conn_status);
                                ++(g_sum_error_conn);
                                ++(g_sum_error_unknown);
                                break;
                        }
                        }
                }
        }
        else
        {
                ++g_sum_success;
                // on success
                if(m_nconn)
                {
                        SSL *l_SSL = ns_hurl::nconn_get_SSL(*m_nconn);
                        if(l_SSL)
                        {
                                int32_t l_protocol_num = ns_hurl::get_tls_info_protocol_num(l_SSL);
                                m_tls_info_cipher_str = ns_hurl::get_tls_info_cipher_str(l_SSL);
                                m_tls_info_protocol_str = ns_hurl::get_tls_info_protocol_str(l_protocol_num);

                                pthread_mutex_lock(&g_sum_info_mutex);
                                ++(g_sum_info_tls_ciphers[m_tls_info_cipher_str]);
                                ++(g_sum_info_tls_protocols[m_tls_info_protocol_str]);
                                pthread_mutex_unlock(&g_sum_info_mutex);

#if 0
                                SSL_SESSION *m_tls_session = SSL_get_session(m_tls);
                                SSL_SESSION_print_fp(stdout, m_tls_session);
                                X509* l_cert = NULL;
                                l_cert = SSL_get_peer_certificate(m_tls);
                                if(NULL == l_cert)
                                {
                                        NDBG_PRINT("SSL_get_peer_certificate error.  tls: %p\n", m_tls);
                                        return NC_STATUS_ERROR;
                                }
                                X509_print_fp(stdout, l_cert);
#endif
                        }
                }
        }
        if(m_nconn)
        {
                m_nconn->nc_cleanup();
                delete m_nconn;
                m_nconn = NULL;
        }
        return STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t request::run_state_machine(void *a_data, ns_hurl::evr_mode_t a_conn_mode)
{
        //NDBG_PRINT("RUN a_conn_mode: %d a_data: %p\n", a_conn_mode, a_data);
        //CHECK_FOR_NULL_ERROR(a_data);
        // TODO -return OK for a_data == NULL
        if(!a_data)
        {
                return STATUS_OK;
        }
        ns_hurl::nconn* l_nconn = static_cast<ns_hurl::nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_ctx());
        t_phurl *l_t_phurl = static_cast<t_phurl *>(l_nconn->get_ctx());
        request *l_rx = static_cast<request *>(l_nconn->get_data());

        // -------------------------------------------------
        // ERROR
        // -------------------------------------------------
        if(a_conn_mode == ns_hurl::EVR_MODE_ERROR)
        {
                // ignore errors for free connections
                if(l_nconn->is_free())
                {
                        TRC_ERROR("call back for free connection\n");
                        return STATUS_OK;
                }
                if(l_t_phurl)
                {
                        // TODO FIX!!!
                        //++l_t_phurl->m_stat.m_upsv_errors;
                }
                if(l_rx)
                {
                        l_t_phurl->m_evr_loop->cancel_timer(l_rx->m_timer_obj);
                        // TODO Check status
                        l_rx->m_timer_obj = NULL;
                        // TODO FIX!!!
                        return l_rx->teardown(ns_hurl::HTTP_STATUS_BAD_GATEWAY);
                        return STATUS_OK;
                }
                else
                {
                        if(l_t_phurl)
                        {
                                // TODO FIX!!!
                                //return l_t_phurl->cleanup_srvr_session(NULL, l_nconn);
                                return STATUS_OK;
                        }
                        else
                        {
                                // TODO log error???
                                return STATUS_ERROR;
                        }
                }
        }
        // -------------------------------------------------
        // TIMEOUT
        // -------------------------------------------------
        else if(a_conn_mode == ns_hurl::EVR_MODE_TIMEOUT)
        {
                //NDBG_PRINT("%sTIMEOUT%s --HOST: %s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_rx->m_host.c_str());
                // ignore timeout for free connections
                if(l_nconn->is_free())
                {
                        TRC_ERROR("call back for free connection\n");
                        return STATUS_OK;
                }
                if(l_rx)
                {
                        return l_rx->teardown(ns_hurl::HTTP_STATUS_GATEWAY_TIMEOUT);
                        return STATUS_OK;
                }
                return STATUS_ERROR;
        }
        else if(a_conn_mode == ns_hurl::EVR_MODE_READ)
        {
                // ignore readable for free connections
                if(l_nconn->is_free())
                {
                        TRC_ERROR("call back for free connection\n");
                        return STATUS_OK;
                }
        }
        // -------------------------------------------------
        // TODO unknown conn mode???
        // -------------------------------------------------
        else if((a_conn_mode != ns_hurl::EVR_MODE_READ) &&
                (a_conn_mode != ns_hurl::EVR_MODE_WRITE))
        {
                TRC_ERROR("unknown a_conn_mode: %d\n", a_conn_mode);
                return STATUS_OK;
        }

        // -------------------------------------------------
        // in/out q's
        // -------------------------------------------------
        ns_hurl::nbq *l_in_q = NULL;
        ns_hurl::nbq *l_out_q = NULL;
        if(l_rx)
        {
                l_in_q = l_rx->m_in_q;
                l_out_q = l_rx->m_out_q;
        }
        else
        {
                l_in_q = l_t_phurl->m_orphan_in_q;
                l_out_q = l_t_phurl->m_orphan_out_q;
        }
        // -------------------------------------------------
        // conn loop
        // -------------------------------------------------
        int32_t l_s = HURL_STATUS_OK;
        do {

                uint32_t l_read = 0;
                uint32_t l_written = 0;
                l_s = l_nconn->nc_run_state_machine(a_conn_mode, l_in_q, l_read, l_out_q, l_written);
                //NDBG_PRINT("l_nconn->nc_run_state_machine(%d): status: %d\n", a_conn_mode, l_s);

                if(l_t_phurl)
                {
                        //l_t_phurl->m_stat.m_upsv_bytes_read += l_read;
                        //l_t_phurl->m_stat.m_upsv_bytes_written += l_written;
                }
                if(!l_rx ||
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
                        if(l_rx->m_resp &&
                           l_rx->m_resp->m_complete)
                        {
                                // Cancel timer
                                l_t_phurl->m_evr_loop->cancel_timer(l_rx->m_timer_obj);
                                // TODO Check status
                                l_rx->m_timer_obj = NULL;
                                if(g_conf_verbose && l_rx->m_resp)
                                {
                                        if(g_conf_color) TRC_OUTPUT("%s", ANSI_COLOR_FG_CYAN);
                                        l_rx->m_resp->show();
                                        if(g_conf_color) TRC_OUTPUT("%s", ANSI_COLOR_OFF);
                                }
                                // Get request time
                                if(l_nconn->get_collect_stats_flag())
                                {
                                        //l_nconn->set_stat_tt_completion_us(get_delta_time_us(l_nconn->get_connect_start_time_us()));
                                }
                                if(l_rx->m_resp && l_t_phurl)
                                {
                                        //l_t_phurl->add_stat_to_agg(l_nconn->get_stats(), l_rx->m_resp->get_status());
                                }
                                l_s = ns_hurl::nconn::NC_STATUS_EOF;
                                goto check_conn_status;
                        }
                }
                // -----------------------------------------
                // STATUS_OK
                // -----------------------------------------
                else if(l_s == ns_hurl::nconn::NC_STATUS_OK)
                {
                        l_s = ns_hurl::nconn::NC_STATUS_BREAK;
                        goto check_conn_status;
                }
check_conn_status:
                if(l_nconn->is_free())
                {
                        return STATUS_OK;
                }
                if(!l_rx)
                {
                        TRC_ERROR("no ups_srvr_session associated with nconn mode: %d\n", a_conn_mode);
                        if(l_t_phurl)
                        {
                                // TODO FIX!!!
                                //return l_t_phurl->cleanup_srvr_session(NULL, l_nconn);
                                return STATUS_OK;
                        }
                        else
                        {
                                // TODO log error???
                                return STATUS_ERROR;
                        }
                }
                switch(l_s)
                {
                case ns_hurl::nconn::NC_STATUS_BREAK:
                {
                        goto done;
                }
                case ns_hurl::nconn::NC_STATUS_EOF:
                {
                        // Connect only && done --early exit...
                        if(g_conf_connect_only)
                        {
                                if(l_rx->m_resp)
                                {
                                        l_rx->m_resp->set_status(ns_hurl::HTTP_STATUS_OK);
                                }
                                if(l_t_phurl)
                                {
                                        //l_t_phurl->add_stat_to_agg(l_nconn->get_stats(), ns_hurl::HTTP_STATUS_OK);
                                }
                        }
                        return l_rx->teardown(ns_hurl::HTTP_STATUS_OK);
                }
                case ns_hurl::nconn::NC_STATUS_ERROR:
                {
                        if(l_t_phurl)
                        {
                                //++(l_t_phurl->m_stat.m_upsv_errors);
                        }
                        return l_rx->teardown(ns_hurl::HTTP_STATUS_BAD_GATEWAY);
                }
                default:
                {
                        break;
                }
                }
                if(l_nconn->is_done())
                {
                        return l_rx->teardown(ns_hurl::HTTP_STATUS_OK);
                }
        } while((l_s != ns_hurl::nconn::NC_STATUS_AGAIN) &&
                (l_t_phurl->is_running()));

done:
        return STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_phurl::t_run(void *a_nothing)
{
        int32_t l_s;
        l_s = init();
        if(l_s != HURL_STATUS_OK)
        {
                printf("Error performing init.\n");
                return NULL;
        }
        m_stopped = false;
        l_s = request_dequeue();
        // TODO check return status???
        // -------------------------------------------------
        // Run server
        // -------------------------------------------------
        while(!m_stopped &&
               m_num_pending)
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
int32_t t_phurl::request_dequeue(void)
{
        uint32_t l_dq = 0;
        while(!m_request_queue.empty() &&
              !m_stopped &&
              (m_num_in_progress < m_num_parallel_max))
        {
                request *l_r = NULL;
                l_r = m_request_queue.front();
                m_request_queue.pop();
                if(l_r == NULL)
                {
                        continue;
                }
                int32_t l_s;
                l_s = request_start(*l_r);
                if(l_s != HURL_STATUS_OK)
                {
                        // Log error
                        printf("error dequeue'ing\n");
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
        return STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_phurl::request_start(request &a_request)
{
        //NDBG_PRINT("%srequest%s --HOST: %s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, a_request.m_host.c_str());
        // Try get idle from proxy pool
        ns_hurl::nconn *l_nconn = NULL;
        l_nconn = create_new_nconn(a_request);
        // try get from idle list
        if(!l_nconn)
        {
                // TODO fatal???
                return STATUS_ERROR;
        }
        //NDBG_PRINT("Adding http_data: %p.\n", l_clnt_session);
        a_request.m_t_phurl = this;
        a_request.m_timer_obj = NULL;
        // Setup clnt_session
        a_request.m_nconn = l_nconn;
        l_nconn->set_data(&a_request);
        // ---------------------------------------
        // setup resp
        // ---------------------------------------
        //NDBG_PRINT("TID[%lu]: %sSTART%s: Host: %s l_nconn: %p\n",
        //                pthread_self(),
        //                ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF,
        //                a_subr.get_label().c_str(),
        //                &a_nconn);
        if(!a_request.m_resp)
        {
                a_request.m_resp = new ns_hurl::resp();
        }
        a_request.m_resp->init(true);
        a_request.m_resp->m_http_parser->data = a_request.m_resp;
        l_nconn->set_read_cb(ns_hurl::http_parse);
        l_nconn->set_read_cb_data(a_request.m_resp);
        // TODO
        //a_request.m_resp->m_expect_resp_body_flag = a_request.m_expect_resp_body_flag;
        // setup q's
        if(!a_request.m_in_q)
        {
                a_request.m_in_q = new ns_hurl::nbq(8*1024);
                a_request.m_resp->set_q(a_request.m_in_q);
        }
        else
        {
                a_request.m_in_q->reset_write();
        }
        if(!a_request.m_out_q)
        {
                a_request.m_out_q = new ns_hurl::nbq(8*1024);
        }
        else
        {
                a_request.m_out_q->reset_write();
        }
        // create request
        int32_t l_s;
        l_s = s_create_request(a_request, *(a_request.m_out_q));
        if(HURL_STATUS_OK != l_s)
        {
                return request::evr_fd_error_cb(l_nconn);
        }
        // ---------------------------------------
        // idle timer
        // ---------------------------------------
        // TODO ???
        l_s = m_evr_loop->add_timer(g_conf_timeout_ms,
                                    request::evr_fd_timeout_cb,
                                    this,
                                    l_nconn,
                                    &(a_request.m_timer_obj));
        if(l_s != HURL_STATUS_OK)
        {
                return request::evr_fd_error_cb(l_nconn);
        }
        // ---------------------------------------
        // Display data from out q
        // ---------------------------------------
        if(g_conf_verbose)
        {
                if(g_conf_color) TRC_OUTPUT("%s", ANSI_COLOR_FG_YELLOW);
                a_request.m_out_q->print();
                if(g_conf_color) TRC_OUTPUT("%s", ANSI_COLOR_OFF);
        }
        // ---------------------------------------
        // start writing request
        // ---------------------------------------
        //NDBG_PRINT("%sSTARTING REQUEST...%s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF);
        ++g_req_num_requested;
        ++g_req_num_in_flight;
        return request::evr_fd_writeable_cb(l_nconn);
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define _SET_NCONN_OPT(_conn, _opt, _buf, _len) do { \
                int _status = 0; \
                _status = _conn->set_opt((_opt), (_buf), (_len)); \
                if (_status != ns_hurl::nconn::NC_STATUS_OK) { \
                        delete _conn;\
                        _conn = NULL;\
                        return NULL;\
                } \
        } while(0)
ns_hurl::nconn *t_phurl::create_new_nconn(const request &a_request)
{
        ns_hurl::nconn *l_nconn = NULL;
        if(g_conf_url_scheme == ns_hurl::SCHEME_TLS)
        {
                l_nconn = new ns_hurl::nconn_tls();
                _SET_NCONN_OPT(l_nconn,ns_hurl::nconn_tls::OPT_TLS_CTX,m_ctx,sizeof(m_ctx));
                _SET_NCONN_OPT(l_nconn,ns_hurl::nconn_tls::OPT_TLS_CIPHER_STR,g_conf_tls_cipher_list.c_str(),g_conf_tls_cipher_list.length());
                bool l_val = g_conf_tls_verify;
                _SET_NCONN_OPT(l_nconn, ns_hurl::nconn_tls::OPT_TLS_VERIFY, &(l_val), sizeof(bool));
                l_val = g_conf_tls_self_ok;
                _SET_NCONN_OPT(l_nconn, ns_hurl::nconn_tls::OPT_TLS_VERIFY_ALLOW_SELF_SIGNED, &(l_val), sizeof(bool));
                l_val = g_conf_tls_no_host_check;
                _SET_NCONN_OPT(l_nconn, ns_hurl::nconn_tls::OPT_TLS_VERIFY_NO_HOST_CHECK, &(l_val), sizeof(bool));
                l_val = g_conf_tls_sni;
                _SET_NCONN_OPT(l_nconn, ns_hurl::nconn_tls::OPT_TLS_SNI, &(l_val), sizeof(bool));
                if(!a_request.m_hostname.empty())
                {
                        _SET_NCONN_OPT(l_nconn, ns_hurl::nconn_tls::OPT_TLS_HOSTNAME, a_request.m_hostname.c_str(), a_request.m_hostname.length());
                }
                else
                {
                        _SET_NCONN_OPT(l_nconn, ns_hurl::nconn_tls::OPT_TLS_HOSTNAME, a_request.m_host.c_str(), a_request.m_host.length());
                }
        }
        else if(g_conf_url_scheme == ns_hurl::SCHEME_TCP)
        {
                l_nconn = new ns_hurl::nconn_tcp();
        }
        else
        {
                return NULL;
        }
        l_nconn->set_ctx(this);
        l_nconn->set_num_reqs_per_conn(1);
        l_nconn->set_collect_stats(false);
        l_nconn->set_connect_only(false);
        l_nconn->set_evr_loop(m_evr_loop);
        l_nconn->setup_evr_fd(request::evr_fd_readable_cb,
                              request::evr_fd_writeable_cb,
                              request::evr_fd_error_cb);
        l_nconn->set_host_info(a_request.m_host_info);
        return l_nconn;
}
//: ----------------------------------------------------------------------------
//: \details: create request callback
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t s_create_request(request &a_request, ns_hurl::nbq &a_nbq)
{
        // TODO grab from path...
        char l_buf[2048];
        //if(!(a_request.m_url_query.empty()))
        //{
        //        l_path_ref += "?";
        //        l_path_ref += a_request.m_url_query;
        //}
        //NDBG_PRINT("HOST: %s PATH: %s\n", a_reqlet.m_url.m_host.c_str(), l_path_ref.c_str());
        int l_len;
        l_len = snprintf(l_buf, sizeof(l_buf),
                        "%s %s?%s HTTP/1.1",
                        g_conf_verb.c_str(),
                        g_conf_url_path.c_str(),
                        g_conf_url_query.c_str());
        ns_hurl::nbq_write_request_line(a_nbq, l_buf, l_len);
        // -------------------------------------------
        // Add repo headers
        // -------------------------------------------
        bool l_specd_host = false;
        // Loop over reqlet map
        for(ns_hurl::kv_map_list_t::const_iterator i_hl = g_conf_headers.begin();
            i_hl != g_conf_headers.end();
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
        // -------------------------------------------
        // Default Host if unspecified
        // -------------------------------------------
        if (!l_specd_host)
        {
                ns_hurl::nbq_write_header(a_nbq,
                                         "Host", strlen("Host"),
                                         a_request.m_host.c_str(), a_request.m_host.length());
        }
        // -------------------------------------------
        // body
        // -------------------------------------------
        if(g_conf_body_data && g_conf_body_data_len)
        {
                //NDBG_PRINT("Write: buf: %p len: %d\n", l_buf, l_len);
                ns_hurl::nbq_write_body(a_nbq, g_conf_body_data, g_conf_body_data_len);
        }
        else
        {
                ns_hurl::nbq_write_body(a_nbq, NULL, 0);
        }
        return STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: Prototypes
//: ----------------------------------------------------------------------------
void display_status_line(void);
void display_summary(void);
std::string dump_all_responses(request_list_t &a_request_list,
                               bool a_color,
                               bool a_pretty,
                               output_type_t a_output_type,
                               int a_part_map);
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int set_header(const std::string &a_key, const std::string &a_val)
{
        ns_hurl::kv_map_list_t::iterator i_obj = g_conf_headers.find(a_key);
        if(i_obj != g_conf_headers.end())
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
                g_conf_headers[a_key] = l_list;
        }
        return STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: Signal handler
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void sig_handler(int signo)
{
        if (signo == SIGINT)
        {
                // Kill program
                g_runtime_stop = true;
                g_runtime_finished = true;
                g_runtime_cancelled = true;
                //g_start_time_ms = ns_hurl::get_time_ms();;
                for(t_phurl_list_t::iterator i_t = g_t_phurl_list.begin();
                    i_t != g_t_phurl_list.end();
                    ++i_t)
                {
                        (*i_t)->stop();
                }
        }
}
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
int command_exec(bool a_send_stop)
{
        int i = 0;
        char l_cmd = ' ';
        nonblock(NB_ENABLE);
        g_runtime_finished = false;
        // ---------------------------------------
        //   Loop forever until user quits
        // ---------------------------------------
        while(!g_runtime_cancelled &&
              !g_runtime_finished)
        {
                i = kbhit();
                if (i != 0)
                {
                        l_cmd = fgetc(stdin);

                        switch (l_cmd)
                        {
                        // -------------------------------------------
                        // Display
                        // -only works when not reading from stdin
                        // -------------------------------------------
                        case 'd':
                        {
                                //a_settings.m_srvr->display_stat();
                                break;
                        }
                        // -------------------------------------------
                        // Quit
                        // -only works when not reading from stdin
                        // -------------------------------------------
                        case 'q':
                        {
                                g_runtime_finished = true;
                                //l_sent_stop = true;
                                break;
                        }
                        // -------------------------------------------
                        // Default
                        // -------------------------------------------
                        default:
                        {
                                break;
                        }
                        }
                }
                // TODO add define...
                usleep(500000);
                if(g_conf_show_summary)
                {
                        display_status_line();
                }
                if(!g_runtime_finished)
                {
                        g_runtime_finished = true;
                        for(t_phurl_list_t::iterator i_t = g_t_phurl_list.begin();
                            i_t != g_t_phurl_list.end();
                            ++i_t)
                        {
                                if((*i_t)->is_running())
                                {
                                        g_runtime_finished = false;
                                        break;
                                }
                        }
                }
        }
        if(!g_runtime_finished)
        {
                for(t_phurl_list_t::iterator i_t = g_t_phurl_list.begin();
                    i_t != g_t_phurl_list.end();
                    ++i_t)
                {
                        (*i_t)->stop();
                }
                for(t_phurl_list_t::iterator i_t = g_t_phurl_list.begin();
                    i_t != g_t_phurl_list.end();
                    ++i_t)
                {
                        if((*i_t)->is_running())
                        {
                                pthread_join(((*i_t)->m_t_run_thread), NULL);
                        }
                }
        }
        //printf("%s.%s.%d: STOP\n", __FILE__,__FUNCTION__,__LINE__);
        // One more status for the lovers
        if(g_conf_show_summary)
        {
                display_status_line();
        }
        nonblock(NB_DISABLE);
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t init_with_url(ns_hurl::scheme_t &ao_scheme,
                      uint16_t &ao_port,
                      std::string &ao_path,
                      std::string &ao_query,
                      const std::string &a_url)
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
        int l_s;
        l_s = http_parser_parse_url(l_url_fixed.c_str(), l_url_fixed.length(), 0, &l_url);
        if(l_s != 0)
        {
                printf("Error parsing url: %s\n", l_url_fixed.c_str());
                // TODO get error msg from http_parser
                return STATUS_ERROR;
        }

        // Set no port
        ao_port = 0;

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
                                        ao_scheme = ns_hurl::SCHEME_TCP;
                                }
                                else if(l_part == "https")
                                {
                                        ao_scheme = ns_hurl::SCHEME_TLS;
                                }
                                else
                                {
                                        printf("Error schema[%s] is unsupported\n", l_part.c_str());
                                        return STATUS_ERROR;
                                }
                                break;
                        }
                        case UF_PORT:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_PORT]: %s\n", l_part.c_str());
                                ao_port = (uint16_t)strtoul(l_part.c_str(), NULL, 10);
                                break;
                        }
                        case UF_PATH:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_PATH]: %s\n", l_part.c_str());
                                ao_path = l_part;
                                break;
                        }
                        // TODO ???
                        case UF_QUERY:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_QUERY]: %s\n", l_part.c_str());
                                ao_query = l_part;
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
        if(!ao_port)
        {
                switch(ao_scheme)
                {
                case ns_hurl::SCHEME_TCP:
                {
                        ao_port = 80;
                        break;
                }
                case ns_hurl::SCHEME_TLS:
                {
                        ao_port = 443;
                        break;
                }
                default:
                {
                        ao_port = 80;
                        break;
                }
                }
        }
        //m_num_to_req = m_path_vector.size();
        //NDBG_PRINT("Showing parsed url.\n");
        //m_url.show();
        if (HURL_STATUS_OK != l_s)
        {
                // Failure
                printf("Error parsing url: %s.\n", l_url_fixed.c_str());
                return STATUS_ERROR;
        }
        //NDBG_PRINT("Parsed url: %s\n", l_url_fixed.c_str());
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t add_line(FILE *a_file_ptr, request_list_t &ao_request_list)
{
        char l_readline[MAX_READLINE_SIZE];
        while(fgets(l_readline, sizeof(l_readline), a_file_ptr))
        {
                size_t l_readline_len = strnlen(l_readline, MAX_READLINE_SIZE);
                if(MAX_READLINE_SIZE == l_readline_len)
                {
                        // line was truncated
                        // Bail out -reject lines longer than limit
                        // -host names ought not be too long
                        printf("Error: hostnames must be shorter than %d chars\n", MAX_READLINE_SIZE);
                        return STATUS_ERROR;
                }
                // read full line
                // Nuke endline
                l_readline[l_readline_len - 1] = '\0';
                std::string l_host(l_readline);
                l_host.erase( std::remove_if( l_host.begin(), l_host.end(), ::isspace ), l_host.end() );
                if(!l_host.empty())
                {
                        request *l_rqst = new request();
                        l_rqst->m_host = l_host;
                        l_rqst->m_port = g_conf_url_port;
                        ao_request_list.push_back(l_rqst);
                }
                //printf("READLINE: %s\n", l_readline);
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t host_resolved_cb(const ns_hurl::host_info *a_host_info, void *a_ctx)
{
        --g_dns_num_in_flight;
        ++g_dns_num_resolved;
        if(!a_ctx)
        {
                ++g_dns_num_errors;
                return STATUS_ERROR;
        }
        if(!a_host_info)
        {
                // TODO log error???
                ++g_dns_num_errors;
                return STATUS_OK;
        }
        request *l_r = static_cast<request *>(a_ctx);
        l_r->m_host_info = *a_host_info;
        l_r->m_host_resolved = true;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: Print the version.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_version(FILE* a_stream, int a_exit_code)
{
        // print out the version information
        fprintf(a_stream, "phurl HTTP Parallel Curl.\n");
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
        fprintf(a_stream, "Usage: phurl -u [http[s]://]hostname[:port]/path [options]\n");
        fprintf(a_stream, "Options are:\n");
        fprintf(a_stream, "  -h, --help           Display this help and exit.\n");
        fprintf(a_stream, "  -V, --version        Display the version number and exit.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "URL Options -or without parameter\n");
        fprintf(a_stream, "  -u, --url            URL -REQUIRED (unless running cli: see --cli option).\n");
        fprintf(a_stream, "  -d, --data           HTTP body data -supports curl style @ file specifier\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Hostname Input Options -also STDIN:\n");
        fprintf(a_stream, "  -f, --host_file      Host name file.\n");
        fprintf(a_stream, "  -J, --host_json      Host listing json format.\n");
        fprintf(a_stream, "  -x, --execute        Script to execute to get host names.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Settings:\n");
        fprintf(a_stream, "  -p, --parallel       Num parallel.\n");
        fprintf(a_stream, "  -t, --threads        Number of parallel threads.\n");
        fprintf(a_stream, "  -H, --header         Request headers -can add multiple ie -H<> -H<>...\n");
        fprintf(a_stream, "  -X, --verb           Request command -HTTP verb to use -GET/PUT/etc\n");
        fprintf(a_stream, "  -T, --timeout        Timeout (seconds).\n");
        fprintf(a_stream, "  -n, --no_async_dns   Use getaddrinfo to resolve.\n");
        fprintf(a_stream, "  -k, --no_cache       Don't use addr info cache.\n");
        fprintf(a_stream, "  -A, --ai_cache       Path to Address Info Cache (DNS lookup cache).\n");
        fprintf(a_stream, "  -C, --connect_only   Only connect -do not send request.\n");
        fprintf(a_stream, "  -Q, --complete_time  Cancel requests after N seconds.\n");
        fprintf(a_stream, "  -W, --complete_ratio Cancel requests after %% complete (0.0-->100.0).\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "TLS Settings:\n");
        fprintf(a_stream, "  -y, --cipher         Cipher --see \"openssl ciphers\" for list.\n");
        fprintf(a_stream, "  -O, --tls_options    SSL Options string.\n");
        fprintf(a_stream, "  -K, --tls_verify     Verify server certificate.\n");
        fprintf(a_stream, "  -N, --tls_sni        Use SSL SNI.\n");
        fprintf(a_stream, "  -B, --tls_self_ok    Allow self-signed certificates.\n");
        fprintf(a_stream, "  -M, --tls_no_host    Skip host name checking.\n");
        fprintf(a_stream, "  -F, --tls_ca_file    SSL CA File.\n");
        fprintf(a_stream, "  -L, --tls_ca_path    SSL CA Path.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Print Options:\n");
        fprintf(a_stream, "  -v, --verbose        Verbose logging\n");
        fprintf(a_stream, "  -c, --no_color       Turn off colors\n");
        fprintf(a_stream, "  -m, --show_summary   Show summary output\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Output Options: -defaults to line delimited\n");
        fprintf(a_stream, "  -o, --output         File to write output to. Defaults to stdout\n");
        fprintf(a_stream, "  -l, --line_delimited Output <HOST> <RESPONSE BODY> per line\n");
        fprintf(a_stream, "  -j, --json           JSON { <HOST>: \"body\": <RESPONSE> ...\n");
        fprintf(a_stream, "  -P, --pretty         Pretty output\n");
        fprintf(a_stream, "  \n");
#ifdef ENABLE_PROFILER
        fprintf(a_stream, "Debug Options:\n");
        fprintf(a_stream, "  -G, --gprofile       Google profiler output file\n");
#endif
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Note: If running large jobs consider enabling tcp_tw_reuse -eg:\n");
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
        // Suppress errors
        ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_NONE);
        //ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_ALL);
        //ns_hurl::trc_log_file_open("/dev/stdout");
        if(isatty(fileno(stdout)) == 0)
        {
                g_conf_color = false;
        }
        pthread_mutex_init(&g_sum_info_mutex, NULL);
        // -------------------------------------------------
        // request settings
        // -------------------------------------------------
        request_list_t *l_request_list = new request_list_t();
        set_header("User-Agent", "phurl Parallel Curl");
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
                { "url",            1, 0, 'u' },
                { "data",           1, 0, 'd' },
                { "host_file",      1, 0, 'f' },
                { "host_file_json", 1, 0, 'J' },
                { "execute",        1, 0, 'x' },
                { "parallel",       1, 0, 'p' },
                { "threads",        1, 0, 't' },
                { "header",         1, 0, 'H' },
                { "verb",           1, 0, 'X' },
                { "timeout",        1, 0, 'T' },
                { "no_async_dns",   1, 0, 'n' },
                { "no_cache",       0, 0, 'k' },
                { "ai_cache",       1, 0, 'A' },
                { "connect_only",   0, 0, 'C' },
                { "complete_time",  1, 0, 'Q' },
                { "complete_ratio", 1, 0, 'W' },
                { "cipher",         1, 0, 'y' },
                { "tls_options",    1, 0, 'O' },
                { "tls_verify",     0, 0, 'K' },
                { "tls_sni",        0, 0, 'N' },
                { "tls_self_ok",    0, 0, 'B' },
                { "tls_no_host",    0, 0, 'M' },
                { "tls_ca_file",    1, 0, 'F' },
                { "tls_ca_path",    1, 0, 'L' },
                { "verbose",        0, 0, 'v' },
                { "no_color",       0, 0, 'c' },
                { "show_summary",   0, 0, 'm' },
                { "output",         1, 0, 'o' },
                { "line_delimited", 0, 0, 'l' },
                { "json",           0, 0, 'j' },
                { "pretty",         0, 0, 'P' },
#ifdef ENABLE_PROFILER
                { "gprofile",       1, 0, 'G' },
#endif
                // list sentinel
                { 0, 0, 0, 0 }
        };
#ifdef ENABLE_PROFILER
        std::string l_gprof_file;
#endif

        std::string l_url;
        std::string l_execute_line;
        std::string l_host_file_str;
        std::string l_host_file_json_str;
        std::string l_output_file = "";
        // Defaults
        output_type_t l_output_mode = OUTPUT_JSON;
        int l_output_part = PART_HOST |
                            PART_SERVER |
                            PART_STATUS_CODE |
                            PART_HEADERS |
                            PART_BODY;
        bool l_output_pretty = false;

        // TODO REMOVE
        UNUSED(l_output_mode);
        UNUSED(l_output_part);
        UNUSED(l_output_pretty);

        // -------------------------------------------
        // Assume unspecified arg url...
        // TODO Unsure if good way to allow unspecified
        // arg...
        // -------------------------------------------
        bool is_opt = false;
        for(int i_arg = 1; i_arg < argc; ++i_arg) {

                if(argv[i_arg][0] == '-') {
                        // next arg is for the option
                        is_opt = true;
                }
                else if(argv[i_arg][0] != '-' && is_opt == false) {
                        // Stuff in url field...
                        l_url = std::string(argv[i_arg]);
                        //if(l_settings.m_verbose)
                        //{
                        //      printf("Found unspecified argument: %s --assuming url...\n", l_url.c_str());
                        //}
                        break;
                } else {
                        // reset option flag
                        is_opt = false;
                }
        }

        // -------------------------------------------------
        // Args...
        // -------------------------------------------------
#ifdef ENABLE_PROFILER
        char l_short_arg_list[] = "hVvu:d:f:J:x:y:O:KNBMF:L:p:t:H:X:T:nkA:CQ:W:Rcmo:ljPG:";
#else
        char l_short_arg_list[] = "hVvu:d:f:J:x:y:O:KNBMF:L:p:t:H:X:T:nkA:CQ:W:Rcmo:ljP";
#endif
        while ((l_opt = getopt_long_only(argc, argv, l_short_arg_list, l_long_options, &l_option_index)) != -1 && ((unsigned char)l_opt != 255))
        {

                if (optarg)
                {
                        l_arg = std::string(optarg);
                }
                else
                {
                        l_arg.clear();
                }
                //printf("arg[%c=%d]: %s\n", l_opt, l_option_index, l_arg.c_str());

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
                // URL
                // -----------------------------------------
                case 'u':
                {
                        l_url = l_arg;
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
                                        return STATUS_ERROR;
                                }
                                g_conf_body_data = l_buf;
                                g_conf_body_data_len = l_len;
                        }
                        else
                        {
                                char *l_buf;
                                uint32_t l_len;
                                l_len = l_arg.length() + 1;
                                l_buf = (char *)malloc(sizeof(char)*l_len);
                                g_conf_body_data = l_buf;
                                g_conf_body_data_len = l_len;
                        }

                        // Add content length
                        char l_len_str[64];
                        sprintf(l_len_str, "%u", g_conf_body_data_len);
                        set_header("Content-Length", l_len_str);
                        break;
                }
                // -----------------------------------------
                // Host file
                // -----------------------------------------
                case 'f':
                {
                        l_host_file_str = l_arg;
                        break;
                }
                // -----------------------------------------
                // Host file JSON
                // -----------------------------------------
                case 'J':
                {
                        l_host_file_json_str = l_arg;
                        break;
                }
                // -----------------------------------------
                // Execute line
                // -----------------------------------------
                case 'x':
                {
                        l_execute_line = l_arg;
                        break;
                }
                // -----------------------------------------
                // cipher
                // -----------------------------------------
                case 'y':
                {
                        g_conf_tls_cipher_list = l_arg;
                        break;
                }
                // -----------------------------------------
                // tls options
                // -----------------------------------------
                case 'O':
                {
                        int32_t l_s;
                        long l_tls_options;
                        l_s = ns_hurl::get_tls_options_str_val(l_arg, l_tls_options);
                        if(l_s != HURL_STATUS_OK)
                        {
                                return STATUS_ERROR;
                        }
                        g_conf_tls_options = l_tls_options;
                        break;
                }
                // -----------------------------------------
                // tls verify
                // -----------------------------------------
                case 'K':
                {
                        g_conf_tls_verify = true;
                        break;
                }
                // -----------------------------------------
                // tls sni
                // -----------------------------------------
                case 'N':
                {
                        g_conf_tls_sni = true;
                        break;
                }
                // -----------------------------------------
                // tls self signed
                // -----------------------------------------
                case 'B':
                {
                        g_conf_tls_self_ok = true;
                        break;
                }
                // -----------------------------------------
                // tls skip host check
                // -----------------------------------------
                case 'M':
                {
                        g_conf_tls_no_host_check = true;
                        break;
                }
                // -----------------------------------------
                // tls ca file
                // -----------------------------------------
                case 'F':
                {
                        g_conf_tls_ca_file = l_arg;
                        break;
                }
                // -----------------------------------------
                // tls ca path
                // -----------------------------------------
                case 'L':
                {
                        g_conf_tls_ca_path = l_arg;
                        break;
                }
                // -----------------------------------------
                // parallel
                // -----------------------------------------
                case 'p':
                {
                        //printf("arg: --parallel: %s\n", optarg);
                        //l_settings.m_start_type = START_PARALLEL;
                        int l_val;
                        l_val = atoi(optarg);
                        if (l_val < 1)
                        {
                                printf("Error parallel must be at least 1\n");
                                return STATUS_ERROR;
                        }
                        g_conf_num_parallel = (uint32_t)l_val;
                        break;
                }
                // -----------------------------------------
                // num threads
                // -----------------------------------------
                case 't':
                {
                        //printf("arg: --threads: %s\n", l_arg.c_str());
                        int l_val;
                        l_val = atoi(optarg);
                        if (l_val < 0)
                        {
                                printf("Error num-threads must be 0 or greater\n");
                                return STATUS_ERROR;
                        }
                        g_conf_num_threads = (uint32_t)l_val;
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
                                return STATUS_ERROR;
                        }
                        l_s = set_header(l_key, l_val);
                        if (l_s != 0)
                        {
                                printf("Error performing set_header: %s\n", l_arg.c_str());
                                return STATUS_ERROR;
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
                                return STATUS_ERROR;
                        }
                        g_conf_verb = l_arg;
                        break;
                }
                // -----------------------------------------
                // Timeout
                // -----------------------------------------
                case 'T':
                {
                        int l_timeout_s = -1;
                        //printf("arg: --threads: %s\n", l_arg.c_str());
                        l_timeout_s = atoi(optarg);
                        if (l_timeout_s < 1)
                        {
                                printf("Error connection timeout must be > 0\n");
                                return STATUS_ERROR;
                        }
                        g_conf_timeout_ms = l_timeout_s*1000;
                        break;
                }
                // -----------------------------------------
                // No async dns
                // -----------------------------------------
                case 'n':
                {
                        g_conf_dns_use_sync = true;
                        break;
                }
                // -----------------------------------------
                // I'm poor -I aint got no cache!
                // -----------------------------------------
                case 'k':
                {
                        g_conf_dns_use_ai_cache = false;
                        break;
                }
                // -----------------------------------------
                // Address Info cache
                // -----------------------------------------
                case 'A':
                {
                        g_conf_dns_ai_cache_file = l_arg;
                        break;
                }
                // -----------------------------------------
                // connect only
                // -----------------------------------------
                case 'C':
                {
                        g_conf_connect_only = true;
                        break;
                }
                // -----------------------------------------
                // completion time
                // -----------------------------------------
                case 'Q':
                {
                        // Set complete time in ms seconds
                        int l_val = atoi(optarg);
                        // TODO Check value...
                        g_conf_completion_time_ms = l_val*1000;
                        break;
                }
                // -----------------------------------------
                // completion ratio
                // -----------------------------------------
                case 'W':
                {
                        double l_val = atof(optarg);
                        if((l_val < 0.0) || (l_val > 100.0))
                        {
                                printf("Error: completion ratio must be > 0.0 and < 100.0\n");
                                return STATUS_ERROR;
                        }
                        g_conf_completion_ratio = (float)l_val;
                        break;
                }
                // -----------------------------------------
                // verbose
                // -----------------------------------------
                case 'v':
                {
                        g_conf_verbose = true;
                        break;
                }
                // -----------------------------------------
                // color
                // -----------------------------------------
                case 'c':
                {
                        g_conf_color = false;
                        break;
                }
                // -----------------------------------------
                // show progress
                // -----------------------------------------
                case 'm':
                {
                        g_conf_show_summary = true;
                        break;
                }
                // -----------------------------------------
                // output file
                // -----------------------------------------
                case 'o':
                {
                        l_output_file = l_arg;
                        break;
                }
                // -----------------------------------------
                // line delimited
                // -----------------------------------------
                case 'l':
                {
                        l_output_mode = OUTPUT_LINE_DELIMITED;
                        break;
                }
                // -----------------------------------------
                // json output
                // -----------------------------------------
                case 'j':
                {
                        l_output_mode = OUTPUT_JSON;
                        break;
                }
                // -----------------------------------------
                // pretty output
                // -----------------------------------------
                case 'P':
                {
                        l_output_pretty = true;
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
                // What???
                case '?':
                {
                        // Required argument was missing
                        // '?' is provided when the 3rd arg to getopt_long does not begin with a ':', and is preceeded
                        // by an automatic error message.
                        fprintf(stdout, "  Exiting.\n");
                        print_usage(stdout, -1);
                        break;
                }
                // Huh???
                default:
                {
                        fprintf(stdout, "Unrecognized option.\n");
                        print_usage(stdout, -1);
                        break;
                }
                }
        }
        // Check for required url argument
        if(l_url.empty())
        {
                fprintf(stdout, "No URL specified.\n");
                print_usage(stdout, -1);
        }
        // else set url
        int32_t l_s;
        l_s = init_with_url(g_conf_url_scheme,
                            g_conf_url_port,
                            g_conf_url_path,
                            g_conf_url_query,
                            l_url);
        if(l_s != STATUS_OK)
        {
                printf("Error performing init_with_url: url: %s\n", l_url.c_str());
                return STATUS_ERROR;
        }
        // -------------------------------------------------
        // Get resource limits
        // -------------------------------------------------
#if 0
        int32_t l_s;
        struct rlimit l_rlim;
        errno = 0;
        l_s = getrlimit(RLIMIT_NOFILE, &l_rlim);
        if(l_s != 0)
        {
                fprintf(stdout, "Error performing getrlimit. Reason: %s\n", strerror(errno));
                return STATUS_ERROR;
        }
        if(l_rlim.rlim_cur < (uint64_t)(l_max_threads*l_num_parallel))
        {
                fprintf(stdout, "Error threads[%d]*parallelism[%d] > process fd resource limit[%u]\n",
                                l_max_threads, l_num_parallel, (uint32_t)l_rlim.rlim_cur);
                return STATUS_ERROR;
        }
#endif
        // -------------------------------------------------
        // Host list processing
        // -------------------------------------------------
        // Read from command
        if(!l_execute_line.empty())
        {
                FILE *fp;
                int32_t l_s = HURL_STATUS_OK;

                fp = popen(l_execute_line.c_str(), "r");
                // Error executing...
                if (fp == NULL)
                {
                }

                l_s = add_line(fp, *l_request_list);
                if(HURL_STATUS_OK != l_s)
                {
                        return STATUS_ERROR;
                }

                l_s = pclose(fp);
                // Error reported by pclose()
                if (l_s == -1)
                {
                        printf("Error: performing pclose\n");
                        return STATUS_ERROR;
                }
                // Use macros described under wait() to inspect `status' in order
                // to determine success/failure of command executed by popen()
                else
                {
                }
        }
        // Read from file
        else if(!l_host_file_str.empty())
        {
                FILE * l_file;
                int32_t l_s = HURL_STATUS_OK;

                l_file = fopen(l_host_file_str.c_str(),"r");
                if (NULL == l_file)
                {
                        printf("Error opening file: %s.  Reason: %s\n", l_host_file_str.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }

                l_s = add_line(l_file, *l_request_list);
                if(HURL_STATUS_OK != l_s)
                {
                        return STATUS_ERROR;
                }

                //printf("ADD_FILE: DONE: %s\n", a_url_file.c_str());

                l_s = fclose(l_file);
                if (0 != l_s)
                {
                        printf("Error performing fclose.  Reason: %s\n", strerror(errno));
                        return STATUS_ERROR;
                }
        }
        else if(!l_host_file_json_str.empty())
        {
                // TODO Create a function to do all this mess
                // -----------------------------------------
                // Check is a file
                // TODO
                // -----------------------------------------
                struct stat l_stat;
                int32_t l_s = HURL_STATUS_OK;
                l_s = stat(l_host_file_json_str.c_str(), &l_stat);
                if(l_s != 0)
                {
                        printf("Error performing stat on file: %s.  Reason: %s\n", l_host_file_json_str.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }

                // Check if is regular file
                if(!(l_stat.st_mode & S_IFREG))
                {
                        printf("Error opening file: %s.  Reason: is NOT a regular file\n", l_host_file_json_str.c_str());
                        return STATUS_ERROR;
                }

                // -----------------------------------------
                // Open file...
                // -----------------------------------------
                FILE * l_file;
                l_file = fopen(l_host_file_json_str.c_str(),"r");
                if (NULL == l_file)
                {
                        printf("Error opening file: %s.  Reason: %s\n", l_host_file_json_str.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }

                // -----------------------------------------
                // Read in file...
                // -----------------------------------------
                int32_t l_size = l_stat.st_size;
                int32_t l_read_size;
                char *l_buf = (char *)malloc(sizeof(char)*l_size);
                l_read_size = fread(l_buf, 1, l_size, l_file);
                if(l_read_size != l_size)
                {
                        printf("Error performing fread.  Reason: %s [%d:%d]\n",
                                        strerror(errno), l_read_size, l_size);
                        return STATUS_ERROR;
                }
                std::string l_buf_str;
                l_buf_str.assign(l_buf, l_size);
                if(l_buf)
                {
                        free(l_buf);
                        l_buf = NULL;
                }

                // NOTE: rapidjson assert's on errors -interestingly
                rapidjson::Document l_doc;
                l_doc.Parse(l_buf_str.c_str());
                if(!l_doc.IsArray())
                {
                        printf("Error reading json from file: %s.  Reason: data is not an array\n",
                                        l_host_file_json_str.c_str());
                        return STATUS_ERROR;
                }
                // rapidjson uses SizeType instead of size_t.
                for(rapidjson::SizeType i_record = 0; i_record < l_doc.Size(); ++i_record)
                {
                        if(!l_doc[i_record].IsObject())
                        {
                                printf("Error reading json from file: %s.  Reason: array member not an object\n",
                                                l_host_file_json_str.c_str());
                                return STATUS_ERROR;
                        }
                        request *l_r = new request();
                        // "host" : "coolhost.com:443",
                        // "hostname" : "coolhost.com",
                        // "id" : "DE4D",
                        // "where" : "my_house"
                        if(l_doc[i_record].HasMember("host")) l_r->m_host = l_doc[i_record]["host"].GetString();
                        //else l_host.m_host = "NO_HOST";
                        if(l_doc[i_record].HasMember("hostname")) l_r->m_hostname = l_doc[i_record]["hostname"].GetString();
                        //else l_host.m_hostname = "NO_HOSTNAME";
                        if(l_doc[i_record].HasMember("id")) l_r->m_id = l_doc[i_record]["id"].GetString();
                        //else l_host.m_id = "NO_ID";
                        if(l_doc[i_record].HasMember("where")) l_r->m_where = l_doc[i_record]["where"].GetString();
                        //else l_host.m_where = "NO_WHERE";
                        if(l_doc[i_record].HasMember("port")) l_r->m_port = l_doc[i_record]["port"].GetUint();
                        else l_r->m_port = g_conf_url_port;
                        l_request_list->push_back(l_r);
                }
                // -----------------------------------------
                // Close file...
                // -----------------------------------------
                l_s = fclose(l_file);
                if (HURL_STATUS_OK != l_s)
                {
                        printf("Error performing fclose.  Reason: %s\n", strerror(errno));
                        return STATUS_ERROR;
                }
        }
        // Read from stdin
        else
        {
                int32_t l_s = HURL_STATUS_OK;
                l_s = add_line(stdin, *l_request_list);
                if(HURL_STATUS_OK != l_s)
                {
                        return STATUS_ERROR;
                }
        }
        if(g_conf_verbose)
        {
                printf("Showing hostname list:\n");
                for(request_list_t::iterator i_r = l_request_list->begin();
                    i_r != l_request_list->end();
                    ++i_r)
                {
                        if(*i_r == NULL)
                        {
                                continue;
                        }
                        printf("%s\n", (*i_r)->m_host.c_str());
                }
        }
        // -------------------------------------------
        // Sigint handler
        // -------------------------------------------
        if(signal(SIGINT, sig_handler) == SIG_ERR)
        {
                printf("Error: can't catch SIGINT\n");
                return STATUS_ERROR;
        }
        // -------------------------------------------
        // evr loop
        // -------------------------------------------
        ns_hurl::evr_loop *l_evr_loop = NULL;
#if defined(__linux__)
        l_evr_loop = new ns_hurl::evr_loop(ns_hurl::EVR_LOOP_EPOLL, 512);
#elif defined(__FreeBSD__) || defined(__APPLE__)
        l_evr_loop = new ns_hurl::evr_loop(ns_hurl::EVR_LOOP_SELECT, 512);
#else
        l_evr_loop = new ns_hurl::evr_loop(ns_hurl::EVR_LOOP_SELECT, 512);
#endif

        // -------------------------------------------
        // Threaded Resolve...
        // -------------------------------------------
        ns_hurl::nresolver *l_resolver = new ns_hurl::nresolver();
        l_s = l_resolver->init(g_conf_dns_use_ai_cache,
                               g_conf_dns_ai_cache_file);
        if(l_s != HURL_STATUS_OK)
        {
                printf("Error: performing init with resolver\n");
                return STATUS_ERROR;
        }

#ifdef ASYNC_DNS_SUPPORT
        // -------------------------------------------
        // create context
        // -------------------------------------------
        ns_hurl::nresolver::adns_ctx *l_adns_ctx = NULL;
        l_adns_ctx = l_resolver->get_new_adns_ctx(l_evr_loop,
                                                  host_resolved_cb);
        if(!l_adns_ctx)
        {
                printf("Error: performing get_new_adns_ctx with resolver\n");
                return STATUS_ERROR;
        }

        l_resolver->set_max_parallel(100);
        l_resolver->set_timeout_s(5);
#endif

        // -------------------------------------------
        // queueing
        // -------------------------------------------
        //NDBG_PRINT("resolving addresses\n");
        g_conf_num_hosts = l_request_list->size();
        uint64_t l_last_time_ms = ns_hurl::get_time_ms();
        uint32_t l_size = l_request_list->size();
        request_list_t::iterator i_r = l_request_list->begin();
        while(!g_runtime_stop &&
              (g_dns_num_resolved < l_size))
        {
                for(;
                    (!g_runtime_stop) &&
                    //(g_dns_num_in_flight < 100) &&
                    (i_r != l_request_list->end());
                    ++i_r)
                {
                        // stats
                        ++g_dns_num_requested;
                        ++g_dns_num_in_flight;
                        if(!*i_r) continue;
                        request &l_r = *(*i_r);
                        l_s = l_resolver->lookup_tryfast(l_r.m_host,
                                                         l_r.m_port,
                                                         l_r.m_host_info);
                        if(l_s == HURL_STATUS_OK)
                        {
                                l_r.m_host_resolved = true;
                                --g_dns_num_in_flight;
                                ++g_dns_num_resolved;
                                continue;
                        }
                        // sync or async...

                        // if sync...
#ifdef ASYNC_DNS_SUPPORT
                        if(g_conf_dns_use_sync)
                        {
#endif
                                l_resolver->lookup_sync(l_r.m_host,
                                                        l_r.m_port,
                                                        l_r.m_host_info);
                                if(l_s != HURL_STATUS_OK)
                                {
                                        continue;
                                }
                                --g_dns_num_in_flight;
                                ++g_dns_num_resolved;
                                l_r.m_host_resolved = true;
#ifdef ASYNC_DNS_SUPPORT
                        }
                        else
                        {
                                //printf("lookup async: %s:%d\n", l_r.m_host.c_str(), l_r.m_port);
                                l_s = l_resolver->lookup_async(l_adns_ctx,
                                                               l_r.m_host,
                                                               l_r.m_port,
                                                               &l_r,
                                                               &l_r.m_adns_lookup_job_handle);
                                if(l_s != HURL_STATUS_OK)
                                {
                                        continue;
                                }
                        }
#endif
                }
                if(g_dns_num_in_flight)
                {
                        //NDBG_PRINT("running g_dns_num_in_flight: %u\n", g_dns_num_in_flight.v);
                        l_s = l_evr_loop->run();
                        if(l_s != HURL_STATUS_OK)
                        {
                                // TODO log error???
                        }
                }
                // every 100ms
                if(!g_conf_show_summary &&
                 (ns_hurl::get_delta_time_ms(l_last_time_ms) > 100))
                {
                        uint32_t l_dns_num_errors = g_dns_num_errors;
                        uint32_t l_dns_num_requested = g_dns_num_requested;
                        uint32_t l_dns_num_resolved = g_dns_num_resolved;
                        uint32_t l_dns_num_in_flight = g_dns_num_in_flight;
                        printf(": DNS requested: %8u || resolved: %8u/%8u || in_flight: %8u || errors: %8u\n",
                                        l_dns_num_requested,
                                        l_dns_num_resolved,
                                        l_size,
                                        l_dns_num_in_flight,
                                        l_dns_num_errors);
                        l_last_time_ms = ns_hurl::get_time_ms();
                }
        }
        if(l_evr_loop)
        {
                delete l_evr_loop;
                l_evr_loop = NULL;
        }
        if(l_resolver)
        {
                delete l_resolver;
                l_resolver = NULL;
        }
        // -------------------------------------------
        // TLS Init
        // -------------------------------------------
        SSL_CTX *l_ctx = NULL;
        if(g_conf_url_scheme == ns_hurl::SCHEME_TLS)
        {
                ns_hurl::tls_init();
                std::string l_unused;
                l_ctx = ns_hurl::tls_init_ctx(g_conf_tls_cipher_list,
                                              g_conf_tls_options,     // ctx options
                                             g_conf_tls_ca_file,      // ctx ca file
                                             g_conf_tls_ca_path,      // ctx ca path
                                             false,                   // is server?
                                             l_unused,                // tls key
                                             l_unused);               // tls crt
        }
        // -------------------------------------------
        // Q resolved...
        // -------------------------------------------
        request_queue_t *l_request_queue = new request_queue_t();
        for(request_list_t::iterator i_r = l_request_list->begin();
            (i_r != l_request_list->end());
            ++i_r)
        {
                if(!(*i_r))
                {
                        continue;
                }
                if((*i_r)->m_host_resolved)
                {
                        //((*i_r)->m_host_info.show());
                        l_request_queue->push(*i_r);
                }
                else
                {
                        ++g_req_num_errors;
                        ++g_sum_error;
                        ++g_sum_error_addr;
                }
        }

        // -------------------------------------------
        // Init
        // -------------------------------------------
        //NDBG_PRINT("initializing threads\n");
        uint32_t l_q_size = l_request_queue->size();
        for(uint32_t i_t = 0; i_t < g_conf_num_threads; ++i_t)
        {
                t_phurl *l_t_phurl = new t_phurl(g_conf_num_parallel);
                g_t_phurl_list.push_back(l_t_phurl);
                l_t_phurl->init();
                l_t_phurl->m_ctx = l_ctx;

                // TODO check status

                uint32_t l_dq_num;
                if(i_t == 0)
                {
                        l_dq_num = (l_q_size / g_conf_num_threads) +
                                   (l_q_size % g_conf_num_threads);
                }
                else
                {
                        l_dq_num = l_q_size / g_conf_num_threads;
                }
                while(!l_request_queue->empty() && l_dq_num)
                {
                        request *l_r = l_request_queue->front();
                        l_request_queue->pop();
                        if(l_r == NULL)
                        {
                                continue;
                        }
                        l_t_phurl->m_request_queue.push(l_r);
                        --l_dq_num;
                }
                g_req_num_pending += l_t_phurl->m_request_queue.size();
                l_t_phurl->m_num_pending = l_t_phurl->m_request_queue.size();
        }

        // -------------------------------------------
        // Run
        // -------------------------------------------
        //NDBG_PRINT("running\n");
        //g_start_time_ms = ns_hurl::get_time_ms();;
        for(t_phurl_list_t::iterator i_t = g_t_phurl_list.begin();
            i_t != g_t_phurl_list.end();
            ++i_t)
        {
                (*i_t)->run();
                // TODO Check status
        }

#ifdef ENABLE_PROFILER
        // Start Profiler
        if (!l_gprof_file.empty())
        {
                ProfilerStart(l_gprof_file.c_str());
        }
#endif

        // -------------------------------------------
        // Run command exec
        // -------------------------------------------
        if(g_conf_show_summary)
        {
                display_status_line();
        }
        l_s = command_exec(true);
        if(l_s != 0)
        {
                return STATUS_ERROR;
        }

#ifdef ENABLE_PROFILER
        if (!l_gprof_file.empty())
        {
                ProfilerStop();
        }
#endif
        //uint64_t l_end_time_ms = get_time_ms() - l_start_time_ms;
        // -------------------------------------------
        // Results...
        // -------------------------------------------
        if(!g_runtime_cancelled)
        {
                bool l_use_color = g_conf_color;
                if(!l_output_file.empty()) l_use_color = false;
                std::string l_responses_str;
                l_responses_str = dump_all_responses(*l_request_list,
                                                     l_use_color,
                                                     l_output_pretty,
                                                     l_output_mode,
                                                     l_output_part);
                if(l_output_file.empty())
                {
                        printf("%s\n", l_responses_str.c_str());
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
                                return STATUS_ERROR;
                        }

                        // Write
                        l_num_bytes_written = fwrite(l_responses_str.c_str(), 1, l_responses_str.length(), l_file_ptr);
                        if(l_num_bytes_written != (int32_t)l_responses_str.length())
                        {
                                printf("Error performing fwrite. Reason: %s\n", strerror(errno));
                                fclose(l_file_ptr);
                                return STATUS_ERROR;
                        }

                        // Close
                        l_s = fclose(l_file_ptr);
                        if(l_s != 0)
                        {
                                printf("Error performing fclose. Reason: %s\n", strerror(errno));
                                return STATUS_ERROR;
                        }
                }
        }
        // -------------------------------------------
        // Summary...
        // -------------------------------------------
        if(g_conf_show_summary)
        {
                display_summary();
        }
        // -------------------------------------------
        // Cleanup...
        // -------------------------------------------
        // TODO
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void display_summary(void)
{
        std::string l_header_str = "";
        std::string l_protocol_str = "";
        std::string l_cipher_str = "";
        std::string l_off_color = "";

        if(g_conf_color)
        {
                l_header_str = ANSI_COLOR_FG_CYAN;
                l_protocol_str = ANSI_COLOR_FG_GREEN;
                l_cipher_str = ANSI_COLOR_FG_YELLOW;
                l_off_color = ANSI_COLOR_OFF;
        }
        printf("****************** %sSUMMARY%s ******************** \n", l_header_str.c_str(), l_off_color.c_str());
        printf("| total hosts:                     %12u\n",g_conf_num_hosts);
        printf("| success:                         %12u\n",g_sum_success.v);
        printf("| error:                           %12u\n",g_sum_error.v);
        printf("| error address lookup:            %12u\n",g_sum_error_addr.v);
        printf("| error connectivity:              %12u\n",g_sum_error_conn.v);
        printf("| error unknown:                   %12u\n",g_sum_error_unknown.v);
        printf("| tls error cert hostname:         %12u\n",g_sum_error_tls_hostname.v);
        printf("| tls error cert self-signed:      %12u\n",g_sum_error_tls_self_signed.v);
        printf("| tls error cert expired:          %12u\n",g_sum_error_tls_expired.v);
        printf("| tls error cert issuer:           %12u\n",g_sum_error_tls_issuer.v);
        printf("| tls error other:                 %12u\n",g_sum_error_tls_other.v);
        // Sort
        typedef std::map<uint32_t, std::string> _sorted_map_t;
        _sorted_map_t l_sorted_map;
        printf("+--------------- %sSSL PROTOCOLS%s ---------------- \n", l_protocol_str.c_str(), l_off_color.c_str());
        l_sorted_map.clear();
        for(summary_map_t::iterator i_s = g_sum_info_tls_protocols.begin(); i_s != g_sum_info_tls_protocols.end(); ++i_s)
        l_sorted_map[i_s->second] = i_s->first;
        for(_sorted_map_t::reverse_iterator i_s = l_sorted_map.rbegin(); i_s != l_sorted_map.rend(); ++i_s)
        printf("| %-32s %12u\n", i_s->second.c_str(), i_s->first);
        printf("+--------------- %sSSL CIPHERS%s ------------------ \n", l_cipher_str.c_str(), l_off_color.c_str());
        l_sorted_map.clear();
        for(summary_map_t::iterator i_s = g_sum_info_tls_ciphers.begin(); i_s != g_sum_info_tls_ciphers.end(); ++i_s)
        l_sorted_map[i_s->second] = i_s->first;
        for(_sorted_map_t::reverse_iterator i_s = l_sorted_map.rbegin(); i_s != l_sorted_map.rend(); ++i_s)
        printf("| %-32s %12u\n", i_s->second.c_str(), i_s->first);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void display_status_line(void)
{
        // -------------------------------------------------
        // Get results from clients
        // -------------------------------------------------
        if(g_conf_color)
        {
                printf("Done: %s%8u%s Reqd: %s%8u%s Pendn: %s%8u%s Flight: %s%8u%s Error: %s%8u%s\n",
                                ANSI_COLOR_FG_GREEN, g_req_num_completed.v, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_YELLOW, g_req_num_requested.v, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_CYAN, g_req_num_pending.v, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_WHITE, g_req_num_in_flight.v, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_RED, g_req_num_errors.v, ANSI_COLOR_OFF);
        }
        else
        {
                printf("Done: %8u Reqd: %8u Pendn: %8u Flight: %8u Error: %8u\n",
                                g_req_num_completed.v,
                                g_req_num_requested.v,
                                g_req_num_pending.v,
                                g_req_num_in_flight.v,
                                g_req_num_errors.v);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string dump_all_responses_line_dl(request_list_t &a_request_list,
                                       bool a_color,
                                       bool a_pretty,
                                       int a_part_map)
{

        std::string l_responses_str = "";
        std::string l_host_color = "";
        std::string l_server_color = "";
        std::string l_id_color = "";
        std::string l_status_color = "";
        std::string l_header_color = "";
        std::string l_body_color = "";
        std::string l_off_color = "";
        char l_buf[1024*1024];
        if(a_color)
        {
                l_host_color = ANSI_COLOR_FG_BLUE;
                l_server_color = ANSI_COLOR_FG_RED;
                l_id_color = ANSI_COLOR_FG_CYAN;
                l_status_color = ANSI_COLOR_FG_MAGENTA;
                l_header_color = ANSI_COLOR_FG_GREEN;
                l_body_color = ANSI_COLOR_FG_YELLOW;
                l_off_color = ANSI_COLOR_OFF;
        }

        for(request_list_t::const_iterator i_rx = a_request_list.begin();
            i_rx != a_request_list.end();
            ++i_rx)
        {
                const request &l_rx = *(*i_rx);
                bool l_fbf = false;
                // Host
                if(a_part_map & PART_HOST)
                {
                        sprintf(l_buf, "\"%shost%s\": \"%s\"",
                                        l_host_color.c_str(), l_off_color.c_str(),
                                        l_rx.m_host.c_str());
                        ARESP(l_buf);
                        l_fbf = true;
                }

                // Server
                if(a_part_map & PART_SERVER)
                {

                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        sprintf(l_buf, "\"%sserver%s\": \"%s:%d\"",
                                        l_server_color.c_str(), l_server_color.c_str(),
                                        l_rx.m_host.c_str(),
                                        l_rx.m_port
                                        );
                        ARESP(l_buf);
                        l_fbf = true;

                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        sprintf(l_buf, "\"%sid%s\": \"%s\"",
                                        l_id_color.c_str(), l_id_color.c_str(),
                                        l_rx.m_id.c_str()
                                        );
                        ARESP(l_buf);
                        l_fbf = true;

                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        sprintf(l_buf, "\"%swhere%s\": \"%s\"",
                                        l_id_color.c_str(), l_id_color.c_str(),
                                        l_rx.m_where.c_str()
                                        );
                        ARESP(l_buf);
                        l_fbf = true;
                }

                if(!l_rx.m_resp)
                {
                        ARESP("\n");
                        continue;
                }
                // Status Code
                if(a_part_map & PART_STATUS_CODE)
                {
                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        const char *l_status_val_color = "";
                        ns_hurl::resp *l_resp = l_rx.m_resp;
                        if(a_color)
                        {
                                if(l_resp->get_status() == 200) l_status_val_color = ANSI_COLOR_FG_GREEN;
                                else l_status_val_color = ANSI_COLOR_FG_RED;
                        }
                        sprintf(l_buf, "\"%sstatus-code%s\": %s%d%s",
                                        l_status_color.c_str(), l_off_color.c_str(),
                                        l_status_val_color, l_resp->get_status(), l_off_color.c_str());
                        ARESP(l_buf);
                        l_fbf = true;
                }

                // Headers
                // TODO -only in json mode for now
                if(a_part_map & PART_HEADERS)
                {
                        // nuthin
                }

                // Body
                if(a_part_map & PART_BODY)
                {
                        ns_hurl::resp *l_resp = l_rx.m_resp;
                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        if(l_resp)
                        {
                                //NDBG_PRINT("RESPONSE SIZE: %ld\n", (*i_rx)->m_response_body.length());
                                ns_hurl::nbq *l_q = l_resp->get_body_q();
                                if(l_q && l_q->read_avail())
                                {
                                        char *l_buf;
                                        uint64_t l_len = l_q->read_avail();
                                        l_buf = (char *)malloc(sizeof(char)*l_len);
                                        l_q->read(l_buf, l_len);
                                        sprintf(l_buf, "\"%sbody%s\": %.*s",
                                                        l_body_color.c_str(), l_off_color.c_str(),
                                                        (int)l_len, l_buf);
                                        if(l_buf)
                                        {
                                                free(l_buf);
                                                l_buf = NULL;
                                        }
                                }
                                else
                                {
                                        sprintf(l_buf, "\"%sbody%s\": \"NO_RESPONSE\"",
                                                        l_body_color.c_str(), l_off_color.c_str());
                                }
                        }
                        else
                        {
                                sprintf(l_buf, "\"%sbody%s\": \"NO_RESPONSE\"",
                                                l_body_color.c_str(), l_off_color.c_str());
                        }
                        ARESP(l_buf);
                        l_fbf = true;
                }
                ARESP("\n");
        }
        return l_responses_str;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define JS_ADD_MEMBER(_key, _val)\
l_obj.AddMember(_key,\
                rapidjson::Value(_val, l_js_allocator).Move(),\
                l_js_allocator)
std::string dump_all_responses_json(request_list_t &a_request_list,
                                    int a_part_map)
{
        rapidjson::Document l_js_doc;
        l_js_doc.SetObject();
        rapidjson::Value l_js_array(rapidjson::kArrayType);
        rapidjson::Document::AllocatorType& l_js_allocator = l_js_doc.GetAllocator();

        for(request_list_t::const_iterator i_rx = a_request_list.begin();
            i_rx != a_request_list.end();
            ++i_rx)
        {
                const request &l_rx = *(*i_rx);
                rapidjson::Value l_obj;
                l_obj.SetObject();

                // Host
                if(a_part_map & PART_HOST)
                {
                        JS_ADD_MEMBER("host", l_rx.m_host.c_str());
                }

                // Server
                if(a_part_map & PART_SERVER)
                {
                        char l_server_buf[1024];
                        sprintf(l_server_buf, "%s:%d",
                                        l_rx.m_host.c_str(),
                                        l_rx.m_port);
                        JS_ADD_MEMBER("server", l_server_buf);
                        JS_ADD_MEMBER("id", l_rx.m_id.c_str());
                        JS_ADD_MEMBER("where", l_rx.m_where.c_str());
                        l_obj.AddMember("port", l_rx.m_port, l_js_allocator);
                }

                ns_hurl::resp *l_resp = l_rx.m_resp;
                if(!l_resp)
                {
                        l_obj.AddMember(rapidjson::Value("Error", l_js_allocator).Move(),
                                        rapidjson::Value(l_rx.m_error_str.c_str(), l_js_allocator).Move(),
                                        l_js_allocator);
                        l_obj.AddMember("status-code", 500, l_js_allocator);
                        l_js_array.PushBack(l_obj, l_js_allocator);
                        continue;
                }

                // Status Code
                if(a_part_map & PART_STATUS_CODE)
                {
                        l_obj.AddMember("status-code", l_resp->get_status(), l_js_allocator);
                }
                // Headers
                const ns_hurl::kv_map_list_t &l_headers = l_resp->get_headers();
                if(a_part_map & PART_HEADERS)
                {
                        for(ns_hurl::kv_map_list_t::const_iterator i_header = l_headers.begin();
                            i_header != l_headers.end();
                            ++i_header)
                        {
                                for(ns_hurl::str_list_t::const_iterator i_val = i_header->second.begin();
                                    i_val != i_header->second.end();
                                    ++i_val)
                                {
                                        l_obj.AddMember(rapidjson::Value(i_header->first.c_str(), l_js_allocator).Move(),
                                                        rapidjson::Value(i_val->c_str(), l_js_allocator).Move(),
                                                        l_js_allocator);
                                }
                        }
                }
                // ---------------------------------------------------
                // SSL connection info
                // ---------------------------------------------------
                // TODO Add flag -only add to output if flag
                // ---------------------------------------------------
                if(!l_rx.m_tls_info_cipher_str.empty())
                {
                        l_obj.AddMember(rapidjson::Value("Cipher", l_js_allocator).Move(),
                                        rapidjson::Value(l_rx.m_tls_info_cipher_str.c_str(), l_js_allocator).Move(),
                                        l_js_allocator);
                }
                if(!l_rx.m_tls_info_protocol_str.empty())
                {
                        l_obj.AddMember(rapidjson::Value("Protocol", l_js_allocator).Move(),
                                        rapidjson::Value(l_rx.m_tls_info_protocol_str.c_str(), l_js_allocator).Move(),
                                        l_js_allocator);
                }

                // Search for json
                bool l_content_type_json = false;
                ns_hurl::kv_map_list_t::const_iterator i_h = l_headers.find("Content-type");
                if(i_h != l_headers.end())
                {
                        for(ns_hurl::str_list_t::const_iterator i_val = i_h->second.begin();
                            i_val != i_h->second.end();
                            ++i_val)
                        {
                                if((*i_val) == "application/json")
                                {
                                        l_content_type_json = true;
                                }
                        }
                }

                // Body
                if(a_part_map & PART_BODY)
                {

                        //NDBG_PRINT("RESPONSE SIZE: %ld\n", (*i_rx)->m_response_body.length());
                        ns_hurl::nbq *l_q = l_resp->get_body_q();
                        if(l_q && l_q->read_avail())
                        {
                                // Append json
                                if(l_content_type_json)
                                {
                                        ns_hurl::nbq_stream l_q_s(*l_q);
                                        rapidjson::Document l_doc_body;
                                        l_doc_body.ParseStream(l_q_s);
                                        l_obj.AddMember("body",
                                                        rapidjson::Value(l_doc_body, l_js_allocator).Move(),
                                                        l_js_allocator);
                                }
                                else
                                {
                                        char *l_buf;
                                        uint64_t l_len = l_q->read_avail();
                                        l_buf = (char *)malloc(sizeof(char)*l_len+1);
                                        l_q->read(l_buf, l_len);
                                        l_buf[l_len] = '\0';
                                        JS_ADD_MEMBER("body", l_buf);
                                        if(l_buf)
                                        {
                                                free(l_buf);
                                                l_buf = NULL;
                                        }
                                }
                        }
                        else
                        {
                                JS_ADD_MEMBER("body", "NO_RESPONSE");
                        }
                }
                l_js_array.PushBack(l_obj, l_js_allocator);
        }
        // TODO -Can I just create an array -do I have to stick in a document?
        l_js_doc.AddMember("array", l_js_array, l_js_allocator);
        rapidjson::StringBuffer l_strbuf;
        rapidjson::Writer<rapidjson::StringBuffer> l_js_writer(l_strbuf);
        l_js_doc["array"].Accept(l_js_writer);
        //NDBG_PRINT("Document: \n%s\n", l_strbuf.GetString());
        std::string l_responses_str = l_strbuf.GetString();
        return l_responses_str;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string dump_all_responses(request_list_t &a_request_list,
                               bool a_color,
                               bool a_pretty,
                               output_type_t a_output_type,
                               int a_part_map)
{
        std::string l_responses_str = "";
        switch(a_output_type)
        {
        case OUTPUT_LINE_DELIMITED:
        {
                l_responses_str = dump_all_responses_line_dl(a_request_list, a_color, a_pretty, a_part_map);
                break;
        }
        case OUTPUT_JSON:
        {
                l_responses_str = dump_all_responses_json(a_request_list, a_part_map);
                break;
        }
        default:
        {
                break;
        }
        }
        return l_responses_str;
}
