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
#include "nghttp2/nghttp2.h"
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
#include "hurl/support/ndebug.h"
#include "hurl/support/file_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
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
//: constants
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
//: enums
//: ----------------------------------------------------------------------------
// ---------------------------------------
// output types
// ---------------------------------------
typedef enum {
        OUTPUT_LINE_DELIMITED,
        OUTPUT_JSON
} output_type_t;
// ---------------------------------------
// output types
// ---------------------------------------
typedef enum {
        PART_HOST = 1,
        PART_SERVER = 1 << 1,
        PART_STATUS_CODE = 1 << 2,
        PART_HEADERS = 1 << 3,
        PART_BODY = 1 << 4
} output_part_t;
//: ----------------------------------------------------------------------------
//: macros
//: ----------------------------------------------------------------------------
#ifndef _U_
#define _U_ __attribute__((unused))
#endif
#ifndef ARRLEN
#define ARRLEN(x) (sizeof(x) / sizeof(x[0]))
#endif
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
//: ----------------------------------------------------------------------------
//: request object/meta
//: ----------------------------------------------------------------------------
class request {
public:
        request():
                m_scheme(ns_hurl::SCHEME_TCP),
                m_host(),
                m_url(),
                m_url_path(),
                m_url_query(),
                m_verb("GET"),
                m_headers(),
                m_body_data(NULL),
                m_body_data_len(0),
                m_conf_tls_cipher_list(),
                m_conf_tls_options(),
                m_conf_tls_verify(),
                m_conf_tls_sni(),
                m_conf_tls_self_ok(),
                m_conf_tls_no_host_check(),
                m_conf_tls_ca_file(),
                m_conf_tls_ca_path(),
                m_port(0),
                m_expect_resp_body_flag(true),
                m_connect_only(false),
                m_keepalive(false),
                m_save(false),
                m_timeout_ms(10000),
                m_host_info(),
                m_data(NULL),
                m_tls_ctx(NULL)
        {};
        request(const request &a_r):
                m_scheme(a_r.m_scheme),
                m_host(a_r.m_host),
                m_url(a_r.m_url),
                m_url_path(a_r.m_url_path),
                m_url_query(a_r.m_url_query),
                m_verb(a_r.m_verb),
                m_headers(a_r.m_headers),
                m_body_data(a_r.m_body_data),
                m_body_data_len(a_r.m_body_data_len),
                m_conf_tls_cipher_list(a_r.m_conf_tls_cipher_list),
                m_conf_tls_options(a_r.m_conf_tls_options),
                m_conf_tls_verify(a_r.m_conf_tls_verify),
                m_conf_tls_sni(a_r.m_conf_tls_sni),
                m_conf_tls_self_ok(a_r.m_conf_tls_self_ok),
                m_conf_tls_no_host_check(a_r.m_conf_tls_no_host_check),
                m_conf_tls_ca_file(a_r.m_conf_tls_ca_file),
                m_conf_tls_ca_path(a_r.m_conf_tls_ca_path),
                m_port(a_r.m_port),
                m_expect_resp_body_flag(a_r.m_expect_resp_body_flag),
                m_connect_only(a_r.m_connect_only),
                m_keepalive(a_r.m_keepalive),
                m_save(a_r.m_save),
                m_timeout_ms(a_r.m_timeout_ms),
                m_host_info(a_r.m_host_info),
                m_data(a_r.m_data),
                m_tls_ctx(a_r.m_tls_ctx)
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
        ns_hurl::scheme_t m_scheme;
        std::string m_host;
        std::string m_url;
        std::string m_url_path;
        std::string m_url_query;
        std::string m_verb;
        ns_hurl::kv_map_list_t m_headers;
        char *m_body_data;
        uint32_t m_body_data_len;
        std::string m_conf_tls_cipher_list;
        long m_conf_tls_options;
        bool m_conf_tls_verify;
        bool m_conf_tls_sni;
        bool m_conf_tls_self_ok;
        bool m_conf_tls_no_host_check;
        std::string m_conf_tls_ca_file;
        std::string m_conf_tls_ca_path;
        uint16_t m_port;
        bool m_expect_resp_body_flag;
        bool m_connect_only;
        bool m_keepalive;
        bool m_save;
        uint32_t m_timeout_ms;
        ns_hurl::host_info m_host_info;
        void *m_data;
        SSL_CTX *m_tls_ctx;
private:
        // Disallow copy/assign
        request& operator=(const request &);
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
        // -------------------------------------------------
        // int path if empty
        // -------------------------------------------------
        if(m_url_path.empty())
        {
                m_url_path = "/";
        }
        // -------------------------------------------------
        // setup ctx
        // -------------------------------------------------
        //m_num_to_req = m_path_vector.size();
        //NDBG_PRINT("Showing parsed url.\n");
        //m_url.show();
        //NDBG_PRINT("Parsed url: %s\n", l_url_fixed.c_str());
        // -------------------------------------------------
        // init tls...
        // -------------------------------------------------
        if(m_scheme == ns_hurl::SCHEME_TLS)
        {
                ns_hurl::tls_init();
                std::string l_unused;
                m_tls_ctx = ns_hurl::tls_init_ctx(m_conf_tls_cipher_list, // ctx cipher list str
                                                  m_conf_tls_options,     // ctx options
                                                  m_conf_tls_ca_file,     // ctx ca file
                                                  m_conf_tls_ca_path,     // ctx ca path
                                                  false,                  // is server?
                                                  l_unused,               // tls key
                                                  l_unused);              // tls crt
                // modes from nghttp2 client example
                SSL_CTX_set_mode(m_tls_ctx, SSL_MODE_AUTO_RETRY);
                SSL_CTX_set_mode(m_tls_ctx, SSL_MODE_RELEASE_BUFFERS);
#ifdef HAS_NPN
                // set npn callback
                SSL_CTX_set_next_proto_select_cb(m_tls_ctx, alpn_select_next_proto_cb, this);
#endif
        }
        //m_num_to_req = m_path_vector.size();
        //NDBG_PRINT("Showing parsed url.\n");
        //m_url.show();
        //NDBG_PRINT("Parsed url: %s\n", l_url_fixed.c_str());
        // -------------------------------------------------
        // done
        // -------------------------------------------------
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: host
//: ----------------------------------------------------------------------------
typedef struct _host_struct {
        std::string m_host;
        std::string m_hostname;
        std::string m_id;
        std::string m_where;
        uint16_t m_port;
        bool m_host_resolved;
        std::string m_tls_info_cipher_str;
        std::string m_tls_info_protocol_str;
        std::string m_error_str;
        ns_hurl::http_status_t m_status;
        std::string m_alpn_str;
        // address resolution
#ifdef ASYNC_DNS_SUPPORT
        void *m_adns_lookup_job_handle;
#endif
        ns_hurl::host_info m_host_info;
        _host_struct():
                m_host(),
                m_hostname(),
                m_id(),
                m_where(),
                m_port(0),
                m_host_resolved(false),
                m_tls_info_cipher_str(),
                m_tls_info_protocol_str(),
                m_error_str(),
                m_status(ns_hurl::HTTP_STATUS_NONE),
                m_alpn_str(),
                // address resolution
#ifdef ASYNC_DNS_SUPPORT
                m_adns_lookup_job_handle(NULL),
#endif
                m_host_info()
        {}
        _host_struct(const _host_struct &a_r):
                m_host(a_r.m_host),
                m_hostname(a_r.m_hostname),
                m_id(a_r.m_id),
                m_where(a_r.m_where),
                m_port(a_r.m_port),
                m_host_resolved(a_r.m_host_resolved),
                m_tls_info_cipher_str(a_r.m_tls_info_cipher_str),
                m_tls_info_protocol_str(a_r.m_tls_info_protocol_str),
                m_error_str(a_r.m_error_str),
                m_status(a_r.m_status),
                m_alpn_str(a_r.m_alpn_str),
#ifdef ASYNC_DNS_SUPPORT
                m_adns_lookup_job_handle(a_r.m_adns_lookup_job_handle),
#endif
                m_host_info(a_r.m_host_info)
        {}
private:
        // Disallow copy/assign
        _host_struct& operator=(const _host_struct &);
} host_t;
typedef std::queue<host_t *> host_queue_t;
typedef std::list <host_t *> host_list_t;
//static int32_t s_create_request(request &a_request, ns_hurl::nbq &a_nbq);
//: ----------------------------------------------------------------------------
//: session
//: ----------------------------------------------------------------------------
class session {
public:
        // -------------------------------------------------
        // public members
        // -------------------------------------------------
        ns_hurl::nconn *m_nconn;
        t_phurl *m_t_phurl;
        ns_hurl::nbq *m_in_q;
        ns_hurl::nbq *m_out_q;
        host_t *m_host;
        uint64_t m_idx;
        bool m_goaway;
        uint32_t m_streams_closed;
        uint32_t m_body_offset;
        // -------------------------------------------------
        // public methods
        // -------------------------------------------------
        session(void):
                m_nconn(NULL),
                m_t_phurl(NULL),
                m_in_q(NULL),
                m_out_q(NULL),
                m_host(NULL),
                m_idx(0),
                m_goaway(false),
                m_streams_closed(0),
                m_body_offset(0)
#if 0
        ,
                m_last_active_ms(0),
                m_timeout_ms(10000)
#endif
        {}
        virtual ~session(void)
        {
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
        int32_t teardown(ns_hurl::http_status_t a_status);
        int32_t cancel_timer(void *a_timer);
        uint64_t get_idx(void) {return m_idx;}
        void set_idx(uint64_t a_idx) {m_idx = a_idx;}
        int32_t request_complete(void);
        void request_log_status(uint16_t a_status = 0);
#if 0
        void clear(void);
        uint32_t get_timeout_ms(void);
        void set_timeout_ms(uint32_t a_t_ms);
        uint64_t get_last_active_ms(void);
        void set_last_active_ms(uint64_t a_time_ms);
#endif
        // -------------------------------------------------
        // pure virtual
        // -------------------------------------------------
        virtual int32_t sconnected(void) = 0;
        virtual int32_t srequest(void) = 0;
        virtual int32_t sread(const uint8_t *a_buf, size_t a_len, size_t a_off) = 0;
        virtual int32_t swrite(void) = 0;
        virtual int32_t sdone(void) = 0;
        // -------------------------------------------------
        // public Static (class) methods
        // -------------------------------------------------
        static int32_t evr_fd_readable_cb(void *a_data) {return run_state_machine(a_data, ns_hurl::EVR_MODE_READ);}
        static int32_t evr_fd_writeable_cb(void *a_data){return run_state_machine(a_data, ns_hurl::EVR_MODE_WRITE);}
        static int32_t evr_fd_error_cb(void *a_data) {return run_state_machine(a_data, ns_hurl::EVR_MODE_ERROR);}
        static int32_t evr_fd_timeout_cb(void *a_data){return run_state_machine(a_data, ns_hurl::EVR_MODE_TIMEOUT);}
private:
        // -------------------------------------------------
        // private methods
        // -------------------------------------------------
        // Disallow copy/assign
        session& operator=(const session &);
        session(const session &);
        // -------------------------------------------------
        // private Static (class) methods
        // -------------------------------------------------
        static int32_t run_state_machine(void *a_data, ns_hurl::evr_mode_t a_conn_mode);
        // -------------------------------------------------
        // private members
        // -------------------------------------------------
#if 0
        uint64_t m_last_active_ms;
        uint32_t m_timeout_ms;
#endif
};
typedef std::list <session *> session_list_t;
//: ----------------------------------------------------------------------------
//: globals
//: ----------------------------------------------------------------------------
// -----------------------------------------------
// settings
// -----------------------------------------------
t_phurl_list_t g_t_phurl_list;
bool g_conf_color = true;
bool g_verbose = false;
bool g_conf_show_summary = false;
uint32_t g_conf_num_threads = 1;
uint32_t g_conf_num_parallel = 100;
uint32_t g_conf_timeout_ms = 10000;
bool g_conf_connect_only = false;
uint32_t g_conf_completion_time_ms = 10000;
float g_conf_completion_ratio = 100.0;
uint32_t g_conf_num_hosts = 0;
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
summary_map_t g_sum_info_alpn;
summary_map_t g_sum_info_alpn_full;
//: ----------------------------------------------------------------------------
//: t_phurl
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
               m_tls_ctx(NULL),
               m_host_queue(),
               m_evr_loop(NULL),
               m_num_parallel_max(a_max_parallel),
               m_num_in_progress(0),
               m_num_pending(0),
               m_request(NULL),
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
                ns_hurl::evr_event_t *l_t = static_cast<ns_hurl::evr_event_t *>(a_timer);
                return m_evr_loop->cancel_event(l_t);
        }
        session *session_create(ns_hurl::nconn *a_nconn);
        int32_t session_cleanup(session *a_ses, ns_hurl::nconn *a_nconn);
        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        sig_atomic_t m_stopped;
        pthread_t m_t_run_thread;
        ns_hurl::nbq *m_orphan_in_q;
        ns_hurl::nbq *m_orphan_out_q;
        SSL_CTX *m_tls_ctx;
        host_queue_t m_host_queue;
        ns_hurl::evr_loop *m_evr_loop;
        uint32_t m_num_parallel_max;
        uint32_t m_num_in_progress;
        uint32_t m_num_pending;
        request *m_request;
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
        ns_hurl::nconn *create_new_nconn(const request &a_request, host_t *a_host);
        int32_t host_start(host_t *a_host);
        int32_t host_dequeue(void);
        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        bool m_is_initd;
};
//: ****************************************************************************
//: ******************** N G H T T P 2   S U P P O R T *************************
//: ****************************************************************************
//: ----------------------------------------------------------------------------
//: \details: nghttp2_send_callback. Here we transmit the |data|, |length| bytes,
//:           to the network. Because we are using libevent bufferevent, we just
//:           write those bytes into bufferevent buffer
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static ssize_t ngxxx_send_cb(nghttp2_session *a_session _U_,
                             const uint8_t *a_data,
                             size_t a_length,
                             int a_flags _U_,
                             void *a_user_data)
{
        //NDBG_PRINT("SEND_CB\n");
        // TODO FIX!!!
        session *l_ses = (session *)a_user_data;
        //ns_hurl::mem_display(a_data, a_length, true);
        int64_t l_s;
        l_s = l_ses->m_out_q->write((const char *)a_data,(uint64_t)a_length);
        //NDBG_PRINT("%sWRITE%s: l_s: %d\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, (int)l_s);
        return (ssize_t)l_s;
}
//: ----------------------------------------------------------------------------
//: \details: nghttp2_on_frame_recv_callback: Called when nghttp2 library
//:           received a complete frame from the remote peer.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int ngxxx_frame_recv_cb(nghttp2_session *a_session,
                               const nghttp2_frame *a_frame,
                               void *a_user_data)
{
        //NDBG_PRINT("%sFRAME%s: TYPE[%6u] ID[%6d]\n",
        //           ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF,
        //           a_frame->hd.type,
        //           a_frame->hd.stream_id);
        session *l_ses = (session *)a_user_data;
        switch (a_frame->hd.type)
        {
        case NGHTTP2_GOAWAY:
        {
                l_ses->m_goaway = true;
        }
        default:
        {
                break;
        }
        }
        return 0;
}
//: ----------------------------------------------------------------------------
//: \details: nghttp2_on_frame_recv_callback: Called when nghttp2 library
//:           received a complete frame from the remote peer.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int ngxxx_frame_send_cb(nghttp2_session *a_session,
                               const nghttp2_frame *a_frame,
                               void *a_user_data)
{
        //NDBG_PRINT("%sFRAME%s: TYPE[%6u] ID[%6d] CAT[%d]\n",
        //           ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF,
        //           a_frame->hd.type,
        //           a_frame->hd.stream_id,
        //           a_frame->headers.cat);
        // TODO FIX!!!
#if 0
        request *l_request = (request *)a_user_data;
        switch (a_frame->hd.type)
        {
        case NGHTTP2_HEADERS:
        {
                if((a_frame->headers.cat == NGHTTP2_HCAT_RESPONSE) &&
                   (l_request->m_ngxxx_session_stream_id == a_frame->hd.stream_id))
                {
                        //fprintf(stderr, "All headers received\n");
                }
                break;
        }
        }
#endif
        return 0;
}
//: ----------------------------------------------------------------------------
//: \details: nghttp2_on_data_chunk_recv_callback: Called when DATA frame is
//:           received from the remote peer. In this implementation, if the frame
//:           is meant to the stream we initiated, print the received data in
//:           stdout, so that the user can redirect its output to the file
//:           easily.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int ngxxx_data_chunk_recv_cb(nghttp2_session *a_session _U_,
                                    uint8_t a_flags _U_,
                                    int32_t a_stream_id,
                                    const uint8_t *a_data,
                                    size_t a_len,
                                    void *a_user_data)
{
        //NDBG_PRINT("%sCHUNK%s: ID[%6d]\n",
        //           ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF,
        //           a_stream_id);
        if(g_verbose)
        {
        TRC_OUTPUT("%.*s", (int)a_len, a_data);
        }
        // TODO FIX!!!
#if 0
        request *l_request = (request *)a_user_data;
        if(l_request->m_ngxxx_session_stream_id == a_stream_id)
        {
                TRC_OUTPUT("%.*s", (int)a_len, a_data);
        }
#endif
        return 0;
}
//: ----------------------------------------------------------------------------
//: \details: nghttp2_on_stream_close_callback: Called when a stream is about to
//:           closed. This example program only deals with 1 HTTP request (1
//:           stream), if it is closed, we send GOAWAY and tear down the
//:           session
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int ngxxx_stream_close_cb(nghttp2_session *a_session,
                                 int32_t a_stream_id,
                                 uint32_t a_error_code,
                                 void *a_user_data)
{
        //NDBG_PRINT("%sCLOSE%s: ID[%6d]\n",
        //           ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
        //           a_stream_id);
        session *l_ses = (session *)a_user_data;
        // TODO check status
        int32_t l_s;
        l_s = l_ses->request_complete();
        if(l_s != HURL_STATUS_OK)
        {
                TRC_ERROR("performing request_complete\n");
                return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
        }
        // TODO -get request status from header cb
        l_ses->request_log_status(200);
        l_ses->m_goaway = true;
        return 0;
}
//: ----------------------------------------------------------------------------
//: \details: nghttp2_on_header_callback: Called when nghttp2 library emits
//:           single header name/value pair
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int ngxxx_header_cb(nghttp2_session *a_session _U_,
                           const nghttp2_frame *a_frame,
                           const uint8_t *a_name,
                           size_t a_namelen,
                           const uint8_t *a_value,
                           size_t a_valuelen,
                           uint8_t a_flags _U_,
                           void *a_user_data)
{
        //NDBG_PRINT("%sHEADER%s: \n", ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF);
        // TODO FIX!!!
        switch (a_frame->hd.type)
        {
        case NGHTTP2_HEADERS:
        {
                if(a_frame->headers.cat == NGHTTP2_HCAT_RESPONSE)
                {
                        if(g_verbose)
                        {
                        // Print response headers for the initiated request.
                        fprintf(stdout, "%s%.*s%s: %s%.*s%s\n",
                                ANSI_COLOR_FG_BLUE, (int)a_namelen, a_name, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_GREEN, (int)a_valuelen, a_value, ANSI_COLOR_OFF);
                        }
                        break;
                }
        }
        default:
        {

        }
        }
        return 0;
}
//: ----------------------------------------------------------------------------
//: \details: nghttp2_on_begin_headers_callback:
//:           Called when nghttp2 library gets started to receive header block.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int ngxxx_begin_headers_cb(nghttp2_session *a_session _U_,
                                  const nghttp2_frame *a_frame,
                                  void *a_user_data)
{
        //NDBG_PRINT("%sBEGIN_HEADERS%s: \n", ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF);
        // TODO FIX!!!
#if 0
        request *l_request = (request *)a_user_data;
        switch (a_frame->hd.type)
        {
        case NGHTTP2_HEADERS:
        {
                if((a_frame->headers.cat == NGHTTP2_HCAT_RESPONSE) &&
                  (l_request->m_ngxxx_session_stream_id == a_frame->hd.stream_id))
                {
                        //fprintf(stderr, "Response headers for stream ID=%d:\n", a_frame->hd.stream_id);
                }
                break;
        }
        }
#endif
        return 0;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static long int ngxxx_data_source_read_cb(nghttp2_session *a_session,
                                          int32_t a_stream_id,
                                          uint8_t *a_buf,
                                          size_t a_length,
                                          uint32_t *a_data_flags,
                                          nghttp2_data_source *a_source,
                                          void *a_user_data)
{
        //NDBG_PRINT("%sDATA_SOURCE_READ_CB%s: \n", ANSI_COLOR_FG_MAGENTA, ANSI_COLOR_OFF);
        // TODO FIX!!!
        // copy up to length into buffer
        session *l_ses = (session *)a_user_data;
        if(!l_ses)
        {
                return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
        }
        if(!a_data_flags)
        {
                return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
        }
        request *l_request = l_ses->m_t_phurl->m_request;
        if(!l_request)
        {
                return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
        }
        uint32_t l_len = l_request->m_body_data_len - l_ses->m_body_offset;
        if(l_len > a_length)
        {
                l_len = a_length;
        }
        memcpy(a_buf,
               (l_request->m_body_data + l_ses->m_body_offset),
               l_len);
        if(g_verbose)
        {
        TRC_OUTPUT("%.*s", l_len, (l_request->m_body_data + l_ses->m_body_offset));
        }
        l_ses->m_body_offset += l_len;
        if(l_ses->m_body_offset == l_request->m_body_data_len)
        {
                *a_data_flags |= NGHTTP2_DATA_FLAG_EOF;
                if(g_verbose)
                {
                TRC_OUTPUT("\n");
                }
        }
        return l_len;
}
//: ****************************************************************************
//: ************************ H T T P   S U P P O R T ***************************
//: ****************************************************************************
//: ----------------------------------------------------------------------------
//: http_session
//: ----------------------------------------------------------------------------
class http_session: public session {
public:
        // -------------------------------------------------
        // public methods
        // -------------------------------------------------
        http_session(void):
                session(),
                m_resp(NULL)
        {}
        ~http_session(void)
        {
                if(m_resp)
                {
                        delete m_resp;
                        m_resp = NULL;
                }
        }
        int32_t sconnected(void);
        int32_t srequest(void);
        int32_t sread(const uint8_t *a_buf, size_t a_len, size_t a_off);
        int32_t swrite(void);
        int32_t sdone(void);
        // -------------------------------------------------
        // public members
        // -------------------------------------------------
private:
        // -------------------------------------------------
        // private methods
        // -------------------------------------------------
        http_session& operator=(const http_session &);
        http_session(const http_session &);
        // -------------------------------------------------
        // private members
        // -------------------------------------------------
        ns_hurl::resp *m_resp;
};
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_session::sconnected(void)
{
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_session::srequest(void)
{
        if(!m_t_phurl)
        {
                TRC_ERROR("m_t_phurl == NULL\n");
                return HURL_STATUS_ERROR;
        }
        ++m_t_phurl->m_num_in_progress;
        m_out_q->reset_read();
        // TODO grab from path...
        std::string l_uri;
        l_uri = m_t_phurl->m_request->m_url_path;
        if(l_uri.empty())
        {
                l_uri = "/";
        }
        if(!(m_t_phurl->m_request->m_url_query.empty()))
        {
                l_uri += "?";
                l_uri += m_t_phurl->m_request->m_url_query;
        }
        // -------------------------------------------------
        // path
        // -------------------------------------------------
        // TODO grab from path...
        char l_buf[2048];
        //if(!(a_request.m_url_query.empty()))
        //{
        //        l_uri += "?";
        //        l_uri += a_request.m_url_query;
        //}
        //NDBG_PRINT("HOST: %s PATH: %s\n", a_reqlet.m_url.m_host.c_str(), l_uri.c_str());
        int l_len;
        l_len = snprintf(l_buf, sizeof(l_buf),
                        "%s %s HTTP/1.1",
                        m_t_phurl->m_request->m_verb.c_str(), l_uri.c_str());
        ns_hurl::nbq_write_request_line(*m_out_q, l_buf, l_len);
        ns_hurl::kv_map_list_t::const_iterator i_hdr;

#define STRN_CASE_CMP(_a,_b) (strncasecmp(_a, _b, strlen(_a)) == 0)

#define SET_IF_V1(_key) do { \
i_hdr = m_t_phurl->m_request->m_headers.find(_key);\
if(i_hdr != m_t_phurl->m_request->m_headers.end()) { \
        ns_hurl::nbq_write_header(*m_out_q,\
                                  _key, strlen(_key),\
                                  i_hdr->second.front().c_str(),  i_hdr->second.front().length());\
}\
} while(0)
        ns_hurl::nbq_write_header(*m_out_q,"Host", m_host->m_host.c_str());
        SET_IF_V1("User-Agent");
        SET_IF_V1("Accept");
        // -------------------------------------------------
        // Add repo headers
        // -------------------------------------------------
        for(ns_hurl::kv_map_list_t::const_iterator i_hl = m_t_phurl->m_request->m_headers.begin();
            i_hl != m_t_phurl->m_request->m_headers.end();
            ++i_hl)
        {
                if(STRN_CASE_CMP("Host", i_hdr->first.c_str()) ||
                   STRN_CASE_CMP("Accept", i_hdr->first.c_str()) ||
                   STRN_CASE_CMP("User-Agent", i_hdr->first.c_str()))
                {
                        continue;
                }
                if(i_hl->first.empty() || i_hl->second.empty()) { continue;}
                for(ns_hurl::str_list_t::const_iterator i_v = i_hl->second.begin();
                    i_v != i_hl->second.end();
                    ++i_v)
                {
                        ns_hurl::nbq_write_header(*m_out_q, i_hl->first.c_str(), i_hl->first.length(), i_v->c_str(), i_v->length());
                }
        }
        // -------------------------------------------------
        // body
        // -------------------------------------------------
        if(m_t_phurl->m_request->m_body_data &&
           m_t_phurl->m_request->m_body_data_len)
        {
                //NDBG_PRINT("Write: buf: %p len: %d\n", l_buf, l_len);
                ns_hurl::nbq_write_body(*m_out_q, m_t_phurl->m_request->m_body_data, m_t_phurl->m_request->m_body_data_len);
        }
        else
        {
                ns_hurl::nbq_write_body(*m_out_q, NULL, 0);
        }
        if(g_verbose)
        {
        TRC_OUTPUT("%s", ANSI_COLOR_FG_WHITE);
        TRC_OUTPUT("+------------------------------------------------------------------------------+\n");
        TRC_OUTPUT("|                                R E Q U E S T                                 |\n");
        TRC_OUTPUT("+------------------------------------------------------------------------------+\n");
        TRC_OUTPUT("%s", ANSI_COLOR_OFF);
        m_out_q->print();
        }
        // -------------------------------------------------
        // create resp
        // -------------------------------------------------
        if(!m_resp)
        {
                m_resp = new ns_hurl::resp();
        }
        m_resp->init(g_verbose);
        m_resp->m_http_parser->data = m_resp;
        m_resp->m_expect_resp_body_flag = m_t_phurl->m_request->m_expect_resp_body_flag;
        m_resp->set_q(m_in_q);
        m_in_q->reset_write();
        // -------------------------------------------------
        // display???
        // -------------------------------------------------
        if(g_verbose)
        {
        TRC_OUTPUT("%s", ANSI_COLOR_FG_CYAN);
        TRC_OUTPUT("+------------------------------------------------------------------------------+\n");
        TRC_OUTPUT("|                              R E S P O N S E                                 |\n");
        TRC_OUTPUT("+------------------------------------------------------------------------------+\n");
        TRC_OUTPUT("%s", ANSI_COLOR_OFF);
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_session::sread(const uint8_t *a_buf, size_t a_len, size_t a_off)
{
        ns_hurl::hmsg *l_hmsg = static_cast<ns_hurl::hmsg *>(m_resp);
        l_hmsg->m_cur_off = a_off;
        l_hmsg->m_cur_buf = reinterpret_cast<char *>(const_cast<uint8_t *>(a_buf));
        size_t l_parse_status = 0;
        //NDBG_PRINT("%sHTTP_PARSER%s: m_read_buf: %p, m_read_buf_idx: %d, l_bytes_read: %d\n",
        //                ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF,
        //                a_buf, (int)a_off, (int)a_len);
        l_parse_status = http_parser_execute(l_hmsg->m_http_parser,
                                             l_hmsg->m_http_parser_settings,
                                             reinterpret_cast<const char *>(a_buf),
                                             a_len);
        //NDBG_PRINT("STATUS: %lu\n", l_parse_status);
        if(l_parse_status < (size_t)a_len)
        {
                TRC_ERROR("Parse error.  Reason: %s: %s\n",
                           http_errno_name((enum http_errno)l_hmsg->m_http_parser->http_errno),
                           http_errno_description((enum http_errno)l_hmsg->m_http_parser->http_errno));
                return HURL_STATUS_ERROR;
        }
        if(m_resp->m_complete)
        {
                // ---------------------------------
                // show (optional)
                // ---------------------------------
                if(g_verbose)
                {
                        if(g_conf_color) TRC_OUTPUT("%s", ANSI_COLOR_FG_CYAN);
                        m_resp->show();
                        if(g_conf_color) TRC_OUTPUT("%s", ANSI_COLOR_OFF);
                }
                request_log_status(m_resp->get_status());
                // ---------------------------------
                // complete
                // ---------------------------------
                int32_t l_s;
                l_s = request_complete();
                if(l_s != HURL_STATUS_OK)
                {
                        TRC_ERROR("performing request_complete\n");
                        return HURL_STATUS_ERROR;
                }
                m_goaway = true;
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_session::swrite(void)
{
#if 0
        if (complete_) {
          return -1;
        }

        auto config = client_->worker->config;
        auto req_stat = client_->get_req_stat(stream_req_counter_);
        if (!req_stat) {
          return 0;
        }

        if (req_stat->data_offset < config->data_length) {
          auto req_stat = client_->get_req_stat(stream_req_counter_);
          auto &wb = client_->wb;

          // TODO unfortunately, wb has no interface to use with read(2)
          // family functions.
          std::array<uint8_t, 16_k> buf;

          ssize_t nread;
          while ((nread = pread(config->data_fd, buf.data(), buf.size(),
                                req_stat->data_offset)) == -1 &&
                 errno == EINTR)
            ;

          if (nread == -1) {
            return -1;
          }

          req_stat->data_offset += nread;

          wb.append(buf.data(), nread);

          if (client_->worker->config->verbose) {
            std::cout << "[send " << nread << " byte(s)]" << std::endl;
          }

          if (req_stat->data_offset == config->data_length) {
            // increment for next request
            stream_req_counter_ += 2;

            if (stream_resp_counter_ == stream_req_counter_) {
              // Response has already been received
              client_->on_stream_close(stream_resp_counter_ - 2, true,
                                       client_->final);
            }
          }
        }

        return 0;
#endif
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_session::sdone(void)
{
#if 0
        complete_ = true;
#endif
        return HURL_STATUS_OK;
}
//: ****************************************************************************
//: *************************** H 2   S U P P O R T ****************************
//: ****************************************************************************
//: ----------------------------------------------------------------------------
//: h2_session
//: ----------------------------------------------------------------------------
class h2_session: public session {
public:
        // -------------------------------------------------
        // public methods
        // -------------------------------------------------
        h2_session(void):
                session(),
                m_ngxxx_session(NULL)
        {}
        ~h2_session(void)
        {
                if(m_ngxxx_session)
                {
                        nghttp2_session_del(m_ngxxx_session);
                        m_ngxxx_session = NULL;
                }
        }
        int32_t sconnected(void);
        int32_t srequest(void);
        int32_t sread(const uint8_t *a_buf, size_t a_len, size_t a_off);
        int32_t swrite(void);
        int32_t sdone(void);
        // -------------------------------------------------
        // public members
        // -------------------------------------------------
private:
        // -------------------------------------------------
        // private methods
        // -------------------------------------------------
        h2_session& operator=(const h2_session &);
        h2_session(const h2_session &);
        // -------------------------------------------------
        // private members
        // -------------------------------------------------
        nghttp2_session *m_ngxxx_session;
};

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t h2_session::sconnected(void)
{
        // -------------------------------------------------
        // create session/stream
        // -------------------------------------------------
        //m_ngxxx_session_stream_id = -1;
        //m_ngxxx_session_stream_closed = false;
        // -------------------------------------------------
        // init session...
        // -------------------------------------------------
        nghttp2_session_callbacks *l_cb;
        nghttp2_session_callbacks_new(&l_cb);
        nghttp2_session_callbacks_set_send_callback(l_cb, ngxxx_send_cb);
        nghttp2_session_callbacks_set_on_frame_send_callback(l_cb, ngxxx_frame_send_cb);
        nghttp2_session_callbacks_set_on_frame_recv_callback(l_cb, ngxxx_frame_recv_cb);
        nghttp2_session_callbacks_set_on_stream_close_callback(l_cb, ngxxx_stream_close_cb);
        nghttp2_session_callbacks_set_on_data_chunk_recv_callback(l_cb, ngxxx_data_chunk_recv_cb);
        nghttp2_session_callbacks_set_on_header_callback(l_cb, ngxxx_header_cb);
        nghttp2_session_callbacks_set_on_begin_headers_callback(l_cb, ngxxx_begin_headers_cb);
        nghttp2_session_client_new(&(m_ngxxx_session), l_cb, this);
        nghttp2_session_callbacks_del(l_cb);
        // -------------------------------------------------
        // send SETTINGS
        // -------------------------------------------------
        nghttp2_settings_entry l_iv[1] = {
                { NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 1000 }
        };
        int32_t l_s;
        // client 24 bytes magic string will be sent by nghttp2 library
        //NDBG_PRINT("SUBMIT_SETTINGS\n");
        l_s = nghttp2_submit_settings(m_ngxxx_session, NGHTTP2_FLAG_NONE, l_iv, ARRLEN(l_iv));
        if(l_s != 0)
        {
                TRC_ERROR("performing nghttp2_submit_settings.  Reason: %s\n", nghttp2_strerror(l_s));
                return HURL_STATUS_ERROR;
        }
        return HURL_STATUS_OK;
#if 0
        int rv;

        // This is required with --disable-assert.
        (void)rv;

        nghttp2_option *opt;

        rv = nghttp2_option_new(&opt);
        assert(rv == 0);

        auto config = client_->worker->config;

        if (config->encoder_header_table_size != NGHTTP2_DEFAULT_HEADER_TABLE_SIZE) {
          nghttp2_option_set_max_deflate_dynamic_table_size(
              opt, config->encoder_header_table_size);
        }

        nghttp2_session_client_new2(&session_, callbacks, client_, opt);

        nghttp2_option_del(opt);

        std::array<nghttp2_settings_entry, 3> iv;
        size_t niv = 2;
        iv[0].settings_id = NGHTTP2_SETTINGS_ENABLE_PUSH;
        iv[0].value = 0;
        iv[1].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
        iv[1].value = (1 << config->window_bits) - 1;

        if (config->header_table_size != NGHTTP2_DEFAULT_HEADER_TABLE_SIZE) {
          iv[niv].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
          iv[niv].value = config->header_table_size;
          ++niv;
        }

        rv = nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, iv.data(), niv);

        assert(rv == 0);

        auto connection_window = (1 << config->connection_window_bits) - 1;
        nghttp2_session_set_local_window_size(session_, NGHTTP2_FLAG_NONE, 0,
                                              connection_window);

        client_->signal_write();
#endif
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t h2_session::srequest(void)
{
        //++m_t_phurl->m_stat.m_reqs;
        ++m_t_phurl->m_num_in_progress;
        std::string l_uri = m_t_phurl->m_request->m_url_path;
        if(l_uri.empty())
        {
                l_uri = "/";
        }
        if(!(m_t_phurl->m_request->m_url_query.empty()))
        {
                l_uri += "?";
                l_uri += m_t_phurl->m_request->m_url_query;
        }
        // -------------------------------------------------
        // send request
        // -------------------------------------------------
        int32_t l_id;
        //printf("[INFO] path      = %s\n", a_path.c_str());
        //printf("[INFO] authority = %s\n", a_host.c_str());
        // -------------------------------------------------
        // authority note:
        // -------------------------------------------------
        // is the concatenation of host and port with ":" in
        // between.
        // -------------------------------------------------
        nghttp2_nv *l_ngxxx_headers;
        uint16_t l_hdrs_len;
        // hdrs should be 4 + header size
        l_hdrs_len = 4 + m_t_phurl->m_request->m_headers.size();
        l_ngxxx_headers = (nghttp2_nv *)malloc(sizeof(nghttp2_nv)*l_hdrs_len);

#define SET_PSUEDO_HEADER(_idx, _key,_val) do { \
        l_ngxxx_headers[_idx].name = const_cast <uint8_t *>(reinterpret_cast<const uint8_t *>(_key));\
        l_ngxxx_headers[_idx].namelen = strlen(_key);\
        l_ngxxx_headers[_idx].value = const_cast <uint8_t *>(reinterpret_cast<const uint8_t *>(_val.c_str()));\
        l_ngxxx_headers[_idx].valuelen = _val.length();\
        l_ngxxx_headers[_idx].flags = NGHTTP2_NV_FLAG_NONE;\
} while(0)

#define SET_HEADER(_idx, _key,_val) do { \
        l_ngxxx_headers[_idx].name = const_cast <uint8_t *>(reinterpret_cast<const uint8_t *>(_key.c_str()));\
        l_ngxxx_headers[_idx].namelen = _key.length();\
        l_ngxxx_headers[_idx].value = const_cast <uint8_t *>(reinterpret_cast<const uint8_t *>(_val.c_str()));\
        l_ngxxx_headers[_idx].valuelen = _val.length();\
        l_ngxxx_headers[_idx].flags = NGHTTP2_NV_FLAG_NONE;\
} while(0)

        std::string l_scheme = "https";
        uint16_t l_hdr_idx = 0;
        // -----------------------------------------
        // required psuedo headers
        // -----------------------------------------
        SET_PSUEDO_HEADER(l_hdr_idx, ":method", m_t_phurl->m_request->m_verb); ++l_hdr_idx;
        SET_PSUEDO_HEADER(l_hdr_idx, ":path", l_uri); ++l_hdr_idx;
        SET_PSUEDO_HEADER(l_hdr_idx, ":scheme", l_scheme); ++l_hdr_idx;
        SET_PSUEDO_HEADER(l_hdr_idx, ":authority", m_host->m_host); ++l_hdr_idx;
        ns_hurl::kv_map_list_t::const_iterator i_hdr;
        // -----------------------------------------
        // std headers
        // -----------------------------------------
#define SET_IF(_key) do { \
        i_hdr = m_t_phurl->m_request->m_headers.find(_key);\
        if(i_hdr != m_t_phurl->m_request->m_headers.end()) { \
                SET_HEADER(l_hdr_idx, i_hdr->first, i_hdr->second.front()); \
                ++l_hdr_idx;\
        }\
} while(0)
        SET_IF("user-agent");
        SET_IF("accept");
        // -----------------------------------------
        // the rest...
        // -----------------------------------------
        for(i_hdr = m_t_phurl->m_request->m_headers.begin();
            i_hdr != m_t_phurl->m_request->m_headers.end();
            ++i_hdr)
        {
#define STRN_CASE_CMP(_a,_b) (strncasecmp(_a, _b, strlen(_a)) == 0)
                if(STRN_CASE_CMP("accept", i_hdr->first.c_str()) ||
                   STRN_CASE_CMP("user-agent", i_hdr->first.c_str()))
                {
                        continue;
                }
                SET_HEADER(l_hdr_idx, i_hdr->first, i_hdr->second.front());
                ++l_hdr_idx;
        }
        if(g_verbose)
        {
        TRC_OUTPUT("%s", ANSI_COLOR_FG_WHITE);
        TRC_OUTPUT("+------------------------------------------------------------------------------+\n");
        TRC_OUTPUT("|                                R E Q U E S T                                 |\n");
        TRC_OUTPUT("+------------------------------------------------------------------------------+\n");
        TRC_OUTPUT("%s", ANSI_COLOR_OFF);
        // print headers
        for(size_t i_h = 0;
            i_h < l_hdrs_len;
            ++i_h)
        {
                NDBG_OUTPUT("%s%.*s%s: %s%.*s%s\n",
                            ANSI_COLOR_FG_BLUE,  (int)l_ngxxx_headers[i_h].namelen,  l_ngxxx_headers[i_h].name,  ANSI_COLOR_OFF,
                            ANSI_COLOR_FG_GREEN, (int)l_ngxxx_headers[i_h].valuelen, l_ngxxx_headers[i_h].value, ANSI_COLOR_OFF);
        }
        NDBG_OUTPUT("\n");
        }
        //fprintf(stderr, "Request headers:\n");
        //NDBG_PRINT("SUBMIT_REQUEST\n");
        // -----------------------------------------
        // body
        // -----------------------------------------
        nghttp2_data_provider *l_ngxxx_data_tmp = NULL;
        nghttp2_data_provider l_ngxxx_data;
        if(m_t_phurl->m_request->m_body_data &&
           m_t_phurl->m_request->m_body_data_len)
        {
                l_ngxxx_data.source = {0};
                l_ngxxx_data.read_callback = ngxxx_data_source_read_cb;
                l_ngxxx_data_tmp = &(l_ngxxx_data);
        }
        l_id = nghttp2_submit_request(m_ngxxx_session, NULL, l_ngxxx_headers, l_hdrs_len, l_ngxxx_data_tmp, this);
        if (l_id < 0)
        {
                TRC_ERROR("performing nghttp2_submit_request.  Reason: %s\n", nghttp2_strerror(l_id));
                return HURL_STATUS_ERROR;
        }
        //printf("[INFO] Stream ID = %d\n", l_id);
        // TODO FIX!!!
        //m_ngxxx_session_stream_id = l_id;
        // -----------------------------------------
        // session send???
        // -----------------------------------------
        //NDBG_PRINT("SESSION_SEND\n");
        int l_s;
        l_s = nghttp2_session_send(m_ngxxx_session);
        //NDBG_PRINT("SESSION_SEND --DONE\n");
        if (l_s != 0)
        {
                TRC_ERROR("performing nghttp2_session_send.  Reason: %s\n", nghttp2_strerror(l_s));
                // TODO
                // delete_http2_session_data(session_data);
                return HURL_STATUS_ERROR;
        }
        if(l_ngxxx_headers)
        {
                free(l_ngxxx_headers);
                l_ngxxx_headers = NULL;
        }
        if(g_verbose)
        {
        TRC_OUTPUT("%s", ANSI_COLOR_FG_CYAN);
        TRC_OUTPUT("+------------------------------------------------------------------------------+\n");
        TRC_OUTPUT("|                              R E S P O N S E                                 |\n");
        TRC_OUTPUT("+------------------------------------------------------------------------------+\n");
        TRC_OUTPUT("%s", ANSI_COLOR_OFF);
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t h2_session::sread(const uint8_t *a_buf, size_t a_len, size_t a_off)
{
#if 0
        auto rv = nghttp2_session_mem_recv(session_, data, len);
        if (rv < 0) {
          return -1;
        }

        assert(static_cast<size_t>(rv) == len);

        if (nghttp2_session_want_read(session_) == 0 &&
            nghttp2_session_want_write(session_) == 0 && client_->wb.rleft() == 0) {
          return -1;
        }

        client_->signal_write();

        return 0;
#endif
        ssize_t l_rl;
        //NDBG_PRINT("nghttp2_session_mem_recv\n");
        l_rl = nghttp2_session_mem_recv(m_ngxxx_session,
                                        a_buf,
                                        a_len);
        if(l_rl < 0)
        {
                TRC_ERROR("performing nghttp2_session_mem_recv: %s", nghttp2_strerror((int) l_rl));;
                // TODO
                //delete_http2_session_data(session_data);
                return HURL_STATUS_ERROR;
        }
#if 0
        NDBG_PRINT("nghttp2_session_send\n");
        l_s = nghttp2_session_send(a_session.m_ngxxx_session);
        if(l_s != 0)
        {
                TRC_ERROR("performing nghttp2_session_send: %s", nghttp2_strerror((int) l_s));;
                // TODO
                //delete_http2_session_data(session_data);
                return HURL_STATUS_ERROR;
        }
#endif
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t h2_session::swrite(void)
{
        int l_s;
        //NDBG_PRINT("nghttp2_session_send\n");
        l_s = nghttp2_session_send(m_ngxxx_session);
        if(l_s != 0)
        {
                TRC_ERROR("performing nghttp2_session_send: %s", nghttp2_strerror((int) l_s));;
                // TODO
                //delete_http2_session_data(session_data);
                return HURL_STATUS_ERROR;
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t h2_session::sdone(void)
{
        int l_rv;
        l_rv = nghttp2_session_terminate_session(m_ngxxx_session, NGHTTP2_NO_ERROR);
        if(l_rv != 0)
        {
                TRC_ERROR("performing nghttp2_session_terminate_session\n");
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
        int32_t l_s;
        l_s = sdone();
        if(l_s != HURL_STATUS_OK)
        {
                TRC_ERROR("performing sdone\n");
                return HURL_STATUS_ERROR;
        }
        if(m_nconn->get_timer_obj())
        {
                m_t_phurl->m_evr_loop->cancel_event(m_nconn->get_timer_obj());
                m_nconn->set_timer_obj(NULL);
        }
        //NDBG_PRINT("m_host: %p\n", m_host);
        if(m_host)
        {
                m_host->m_status = a_status;
        }
        if(m_host && m_nconn)
        {
                char *l_alpn_result;
                uint32_t l_alpn_result_len = 0;
                m_nconn->get_alpn_result(&l_alpn_result, l_alpn_result_len);
                m_host->m_alpn_str = "";
                if(l_alpn_result_len)
                {
                        //ns_hurl::mem_display((const uint8_t *)l_alpn_result, l_alpn_result_len, true);
                        // parse...
                        for(uint32_t i_c = 0; i_c < l_alpn_result_len;)
                        {
                                uint16_t l_len = (uint32_t)l_alpn_result[i_c];
                                ++i_c;
                                char *l_buf = l_alpn_result+i_c;
                                std::string l_p = "";
                                for(uint32_t i_p = 0; i_p < l_len; ++i_p)
                                {
                                        l_p += *(l_buf+i_p);
                                }
                                //NDBG_PRINT("l_p: %s\n", l_p.c_str());
                                pthread_mutex_lock(&g_sum_info_mutex);
                                ++(g_sum_info_alpn[l_p]);
                                pthread_mutex_unlock(&g_sum_info_mutex);
                                m_host->m_alpn_str += l_p;
                                if(i_c + l_len < l_alpn_result_len)
                                {
                                        m_host->m_alpn_str += ",";
                                }
                                i_c += l_len;
                        }
                }
                else
                {
                        m_host->m_alpn_str = "NA";
                        pthread_mutex_lock(&g_sum_info_mutex);
                        ++(g_sum_info_alpn["NA"]);
                        pthread_mutex_unlock(&g_sum_info_mutex);
                }
                pthread_mutex_lock(&g_sum_info_mutex);
                ++(g_sum_info_alpn_full[m_host->m_alpn_str]);
                pthread_mutex_unlock(&g_sum_info_mutex);
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
                if(m_nconn &&
                   m_host)
                {
                        SSL *l_SSL = ns_hurl::nconn_get_SSL(*m_nconn);
                        if(l_SSL)
                        {
                                int32_t l_protocol_num = ns_hurl::get_tls_info_protocol_num(l_SSL);
                                m_host->m_tls_info_cipher_str = ns_hurl::get_tls_info_cipher_str(l_SSL);
                                m_host->m_tls_info_protocol_str = ns_hurl::get_tls_info_protocol_str(l_protocol_num);
                                pthread_mutex_lock(&g_sum_info_mutex);
                                ++(g_sum_info_tls_ciphers[m_host->m_tls_info_cipher_str]);
                                ++(g_sum_info_tls_protocols[m_host->m_tls_info_protocol_str]);
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
void session::request_log_status(uint16_t a_status)
{
        if(!m_t_phurl)
        {
                return;
        }
#if 0
        if((a_status >= 100) && (a_status < 200)) {/* TODO log 1xx's? */}
        else if((a_status >= 200) && (a_status < 300)){++m_t_phurl->m_stat.m_resp_status_2xx;}
        else if((a_status >= 300) && (a_status < 400)){++m_t_phurl->m_stat.m_resp_status_3xx;}
        else if((a_status >= 400) && (a_status < 500)){++m_t_phurl->m_stat.m_resp_status_4xx;}
        else if((a_status >= 500) && (a_status < 600)){++m_t_phurl->m_stat.m_resp_status_5xx;}
        ++m_t_phurl->m_status_code_count_map[a_status];
#endif
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t session::request_complete(void)
{
        //NDBG_PRINT("%sREQUEST_COMPLETE%s\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF);
        ++m_streams_closed;
        if(!m_t_phurl)
        {
                TRC_ERROR("m_t_phurl == NULL\n");
                return HURL_STATUS_ERROR;
        }
        if(!m_nconn)
        {
                TRC_ERROR("m_t_phurl == NULL\n");
                return HURL_STATUS_ERROR;
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
        //NDBG_PRINT("%sRUN%s a_conn_mode: %d a_data: %p\n",
        //           ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF,
        //           a_conn_mode, a_data);
        //NDBG_PRINT_BT();
        //CHECK_FOR_NULL_ERROR(a_data);
        // TODO -return OK for a_data == NULL
        if(!a_data)
        {
                return HURL_STATUS_OK;
        }
        ns_hurl::nconn* l_nconn = static_cast<ns_hurl::nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_ctx());
        t_phurl *l_t_phurl = static_cast<t_phurl *>(l_nconn->get_ctx());
        // -------------------------------------------------
        // ERROR
        // -------------------------------------------------
        if(a_conn_mode == ns_hurl::EVR_MODE_ERROR)
        {
                TRC_ERROR("EVR_MODE_ERROR\n");
                // ignore errors for free connections
                if(l_nconn->is_free())
                {
                        TRC_ERROR("call back for free connection\n");
                        return HURL_STATUS_OK;
                }
                if(l_t_phurl)
                {
                        // TODO FIX!!!
                        //++l_t_phurl->m_stat.m_upsv_errors;
                }
                session *l_ses = static_cast<session *>(l_nconn->get_data());
                if(l_ses)
                {
                        l_t_phurl->m_evr_loop->cancel_event(l_nconn->get_timer_obj());
                        l_nconn->set_timer_obj(NULL);
                        int32_t l_s;
                        l_s = l_ses->teardown(ns_hurl::HTTP_STATUS_BAD_GATEWAY);
                        if(l_s != HURL_STATUS_OK)
                        {
                                TRC_ERROR("performing session_cleanup\n");
                                // TODO -error???
                        }
                        delete l_ses;
                        l_ses = NULL;
                        // TODO ... -cleanup connection???
                        // TODO record???
                        return HURL_STATUS_DONE;
                }
                else
                {
                        l_ses = l_t_phurl->session_create(l_nconn);
                        if(!l_ses)
                        {
                                TRC_ERROR("performing session_create for host: %s.\n", l_nconn->get_label().c_str());
                        }
                        int32_t l_s;
                        l_s = l_ses->sconnected();
                        if(l_s != HURL_STATUS_OK)
                        {
                                TRC_ERROR("performing m_connected_cb for host: %s.\n", l_nconn->get_label().c_str());
                        }
                        l_s = l_ses->teardown(ns_hurl::HTTP_STATUS_BAD_GATEWAY);
                        if(l_s != HURL_STATUS_OK)
                        {
                                TRC_ERROR("performing session_cleanup\n");
                                // TODO -error???
                        }
                        delete l_ses;
                        l_ses = NULL;
                }
        }
        // -------------------------------------------------
        // TIMEOUT
        // -------------------------------------------------
        else if(a_conn_mode == ns_hurl::EVR_MODE_TIMEOUT)
        {
                TRC_ERROR("EVR_MODE_TIMEOUT\n");
                // ignore timeout for free connections
                if(l_nconn->is_free())
                {
                        TRC_ERROR("call back for free connection\n");
                        return HURL_STATUS_OK;
                }
                session *l_ses = static_cast<session *>(l_nconn->get_data());
                if(l_ses)
                {
                        int32_t l_s;
                        l_s = l_ses->teardown(ns_hurl::HTTP_STATUS_BAD_GATEWAY);
                        if(l_s != HURL_STATUS_OK)
                        {
                                TRC_ERROR("performing session_cleanup\n");
                                // TODO -error???
                        }
                        delete l_ses;
                        l_ses = NULL;
                        // TODO ... -cleanup connection???
                        // TODO record???
                }
                else
                {
                        l_ses = l_t_phurl->session_create(l_nconn);
                        if(!l_ses)
                        {
                                TRC_ERROR("performing session_create for host: %s.\n", l_nconn->get_label().c_str());
                        }
                        int32_t l_s;
                        l_s = l_ses->sconnected();
                        if(l_s != HURL_STATUS_OK)
                        {
                                TRC_ERROR("performing m_connected_cb for host: %s.\n", l_nconn->get_label().c_str());
                        }
                        l_s = l_ses->teardown(ns_hurl::HTTP_STATUS_BAD_GATEWAY);
                        if(l_s != HURL_STATUS_OK)
                        {
                                TRC_ERROR("performing session_cleanup\n");
                                // TODO -error???
                        }
                        delete l_ses;
                        l_ses = NULL;
                }
                return HURL_STATUS_DONE;
        }
        // -------------------------------------------------
        // READ
        // -------------------------------------------------
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
        // --------------------------------------------------
        // **************************************************
        // state machine
        // **************************************************
        // --------------------------------------------------
        //NDBG_PRINT("%sRUN_STATE_MACHINE%s: CONN[%p] STATE[%d] MODE: %d --START\n",
        //                ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF, l_nconn, l_nconn->get_state(), a_conn_mode);
state_top:
        //NDBG_PRINT("%sRUN_STATE_MACHINE%s: CONN[%p] STATE[%d] MODE: %d\n",
        //                ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF, l_nconn, l_nconn->get_state(), a_conn_mode);
        switch(l_nconn->get_state())
        {
        // -------------------------------------------------
        // STATE: FREE
        // -------------------------------------------------
        case ns_hurl::nconn::NC_STATE_FREE:
        {
                //NDBG_PRINT("NC_STATE_FREE\n");
                int32_t l_s;
                l_s = l_nconn->ncsetup();
                if(l_s != ns_hurl::nconn::NC_STATUS_OK)
                {
                        TRC_ERROR("performing ncsetup\n");
                        return HURL_STATUS_ERROR;
                }
                l_nconn->set_state(ns_hurl::nconn::NC_STATE_CONNECTING);
                // TODO FIX!!!
#if 0
                // Stats
                if(m_collect_stats_flag)
                {
                        m_connect_start_time_us = get_time_us();
                }
#endif
                goto state_top;
        }
        // -------------------------------------------------
        // STATE: CONNECTING
        // -------------------------------------------------
        case ns_hurl::nconn::NC_STATE_CONNECTING:
        {
                //NDBG_PRINT("NC_STATE_CONNECTING\n");
                int32_t l_s;
                //NDBG_PRINT("%sconnecting%s: host: %s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_nconn->get_label().c_str());
                l_s = l_nconn->ncconnect();
                if(l_s == ns_hurl::nconn::NC_STATUS_ERROR)
                {
                        // -----------------------------------------
                        // create session
                        // -----------------------------------------
                        session *l_ses = NULL;
                        l_ses = l_t_phurl->session_create(l_nconn);
                        l_s = l_ses->sconnected();
                        l_ses->teardown(ns_hurl::HTTP_STATUS_BAD_GATEWAY);
                        delete l_ses;
                        return HURL_STATUS_DONE;
                }
                if(l_nconn->is_connecting())
                {
                        //NDBG_PRINT("Still connecting...\n");
                        return HURL_STATUS_OK;
                }
                // TODO FIX!!!
                //NDBG_PRINT("%sconnected%s: label: %s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_nconn->get_label().c_str());
                //TRC_DEBUG("Connected: label: %s\n", l_nconn->get_label().c_str());
                // Returning client fd
                // If OK -change state to connected???
                l_nconn->set_state(ns_hurl::nconn::NC_STATE_CONNECTED);
                // -----------------------------------------
                // connected callback
                // -----------------------------------------
                if(g_verbose)
                {
                l_s = ns_hurl::show_tls_info(l_nconn);
                if(l_s != HURL_STATUS_OK)
                {
                        TRC_ERROR("performing show_tls_info\n");
                        return HURL_STATUS_ERROR;
                }
                }
                // -----------------------------------------
                // flip data pointer to session
                // -----------------------------------------
                host_t *l_host = static_cast<host_t *>(l_nconn->get_host_data());
                CHECK_FOR_NULL_ERROR(l_host);
                // -----------------------------------------
                // create session
                // -----------------------------------------
                session *l_ses = NULL;
                l_ses = l_t_phurl->session_create(l_nconn);
                if(!l_ses)
                {
                        TRC_ERROR("performing session_create for host: %s.\n", l_nconn->get_label().c_str());
                        return HURL_STATUS_ERROR;
                }
                l_ses->m_host = l_host;
                l_nconn->set_data(l_ses);
                // -----------------------------------------
                // on connected
                // -----------------------------------------
                l_s = l_ses->sconnected();
                if(l_s != HURL_STATUS_OK)
                {
                        TRC_ERROR("performing m_connected_cb for host: %s.\n", l_nconn->get_label().c_str());
                        return HURL_STATUS_ERROR;
                }
                // -----------------------------------------
                // bail out if connect only
                // -----------------------------------------
                if(g_conf_connect_only)
                {
                        l_nconn->set_state_done();
                        goto state_top;
                }
                //NDBG_PRINT("%sconnected%s: label: %s --created session\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_nconn->get_label().c_str());
                // -----------------------------------------
                // start first request
                // -----------------------------------------
                l_s = l_ses->srequest();
                if(l_s != HURL_STATUS_OK)
                {
                        TRC_ERROR("performing request_create for host: %s.\n", l_nconn->get_label().c_str());
                        return HURL_STATUS_ERROR;
                }
                a_conn_mode = ns_hurl::EVR_MODE_WRITE;
                // TODO FIX!!!
#if 0
                if(m_collect_stats_flag)
                {
                        m_stat.m_tt_connect_us = get_delta_time_us(m_connect_start_time_us);
                }
                if(m_connect_only)
                {
                        m_nc_state = NC_STATE_DONE;
                        return NC_STATUS_EOF;
                }
                if(a_out_q->read_avail())
                {
                        a_mode = EVR_MODE_WRITE;
                }
#endif
                goto state_top;
        }
        // -------------------------------------------------
        // STATE: CONNECTED
        // -------------------------------------------------
        case ns_hurl::nconn::NC_STATE_CONNECTED:
        {
                session *l_ses = static_cast<session *>(l_nconn->get_data());
                CHECK_FOR_NULL_ERROR(l_ses);
                CHECK_FOR_NULL_ERROR(l_t_phurl);
                switch(a_conn_mode)
                {
                // -----------------------------------------
                // read...
                // -----------------------------------------
                case ns_hurl::EVR_MODE_READ:
                {
                        //NDBG_PRINT("%sread%s: label: %s\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, l_nconn->get_label().c_str());
                        if(!l_ses)
                        {
                                TRC_ERROR("l_ses == NULL\n");
                        }
                        ns_hurl::nbq *l_in_q = NULL;
                        if(l_ses &&
                           l_ses->m_in_q)
                        {
                                l_in_q = l_ses->m_in_q;
                        }
                        else
                        {
                                TRC_ERROR("orphan q (in)\n");
                                l_in_q = l_t_phurl->m_orphan_in_q;
                        }
                        uint32_t l_read = 0;
                        int32_t l_s = ns_hurl::nconn::NC_STATUS_OK;
                        char *l_buf = NULL;
                        uint64_t l_off = l_in_q->get_cur_write_offset();
                        l_s = l_nconn->nc_read(l_in_q, &l_buf, l_read);
#if 0
                        if(g_stats)
                        {
                                if(m_stat.m_tt_first_read_us == 0)
                                {
                                        m_stat.m_tt_first_read_us = get_delta_time_us(m_request_start_time_us);
                                }
                        }
#endif
                        //NDBG_PRINT("nc_read: %d total: %lu\n", l_s, l_in_q->read_avail());
                        //NDBG_PRINT("l_s: %d\n", l_s);
                        switch(l_s){
                        // ---------------------------------
                        // NC_STATUS_EOF
                        // ---------------------------------
                        case ns_hurl::nconn::NC_STATUS_EOF:
                        {
                                l_nconn->set_state_done();
                                goto state_top;
                        }
                        // ---------------------------------
                        // NC_STATUS_ERROR
                        // ---------------------------------
                        case ns_hurl::nconn::NC_STATUS_ERROR:
                        {
                                l_nconn->set_state_done();
                                goto state_top;
                        }
                        // ---------------------------------
                        // NC_STATUS_AGAIN
                        // ---------------------------------
                        case ns_hurl::nconn::NC_STATUS_AGAIN:
                        {
                                // wait till next event
                                // TODO check for idle...
                                return HURL_STATUS_OK;
                        }
                        // ---------------------------------
                        // NC_STATUS_READ_UNAVAILABLE
                        // ---------------------------------
                        case ns_hurl::nconn::NC_STATUS_READ_UNAVAILABLE:
                        {
                                // TODO TRACE
                                return HURL_STATUS_ERROR;
                        }
                        // ---------------------------------
                        // NC_STATUS_OK
                        // ---------------------------------
                        case ns_hurl::nconn::NC_STATUS_OK:
                        {
                                if(!l_ses)
                                {
                                        return HURL_STATUS_ERROR;
                                }
                                ns_hurl::nbq *l_out_q = NULL;
                                if(l_ses &&
                                   l_ses->m_in_q)
                                {
                                        l_out_q = l_ses->m_out_q;
                                }
                                else
                                {
                                        l_out_q = l_t_phurl->m_orphan_out_q;
                                }
                                // -------------------------
                                // parse
                                // -------------------------
                                l_ses->m_streams_closed = 0;
                                l_s = l_ses->sread((const uint8_t *)l_buf, (size_t)l_read, (size_t)l_off);
                                if(l_s != HURL_STATUS_OK)
                                {
                                        TRC_ERROR("performing sread\n");
                                        l_nconn->set_state_done();
                                        goto state_top;
                                }
                                if(l_out_q->read_avail() != 0)
                                {
                                        // flip back to write and retry
                                        //NDBG_PRINT("flip to write l_out_q->read_avail: %d\n", (int)l_out_q->read_avail());
                                        a_conn_mode = ns_hurl::EVR_MODE_WRITE;
                                        goto state_top;
                                }
                                if(l_ses->m_streams_closed)
                                {
                                        l_nconn->set_state_done();
                                        goto state_top;
                                }
                                // -------------------------
                                // if complete and can
                                // request again...
                                // -------------------------
                                goto state_top;
                        }
                        // ---------------------------------
                        // default
                        // ---------------------------------
                        default:
                        {
                                return HURL_STATUS_ERROR;
                        }
                        }
                        return HURL_STATUS_ERROR;
                }
                // -----------------------------------------
                // write...
                // -----------------------------------------
                case ns_hurl::EVR_MODE_WRITE:
                {
                        //NDBG_PRINT("%swrite%s: label: %s\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, l_nconn->get_label().c_str());
                        if(!l_ses)
                        {
                                //NDBG_PRINT("l_ses == NULL\n");
                        }
                        int32_t l_s;
                        l_s = l_ses->swrite();
                        if(l_s != HURL_STATUS_OK)
                        {
                                TRC_ERROR("performing swrite\n");
                                return HURL_STATUS_ERROR;
                        }
                        ns_hurl::nbq *l_out_q = NULL;
                        if(l_ses &&
                           l_ses->m_out_q)
                        {
                                l_out_q = l_ses->m_out_q;
                        }
                        else
                        {
                                TRC_ERROR("orphan q (out)\n");
                                l_out_q = l_t_phurl->m_orphan_out_q;
                        }
                        if(!l_out_q ||
                           !l_out_q->read_avail())
                        {
                                // nothing to write
                                //NDBG_PRINT("nothing to write\n");
                                return HURL_STATUS_OK;
                        }
                        uint32_t l_written = 0;
                        l_s = ns_hurl::nconn::NC_STATUS_OK;
                        l_s = l_nconn->nc_write(l_out_q, l_written);
                        //NDBG_PRINT("wrote: %u bytes\n", l_written);
                        switch(l_s){
                        // ---------------------------------
                        // TODO
                        // ---------------------------------
                        case ns_hurl::nconn::NC_STATUS_EOF:
                        {
                                l_nconn->set_state_done();
                                goto state_top;
                        }
                        // ---------------------------------
                        // TODO
                        // ---------------------------------
                        case ns_hurl::nconn::NC_STATUS_ERROR:
                        {
                                l_nconn->set_state_done();
                                goto state_top;
                        }
                        // ---------------------------------
                        // TODO
                        // ---------------------------------
                        case ns_hurl::nconn::NC_STATUS_AGAIN:
                        {
                                // wait till next event
                                // TODO check for idle...
                                return HURL_STATUS_OK;
                        }
                        // ---------------------------------
                        // TODO
                        // ---------------------------------
                        case ns_hurl::nconn::NC_STATUS_OK:
                        {
                                // good to write again
                                goto state_top;
                                break;
                        }
                        // ---------------------------------
                        // TODO
                        // ---------------------------------
                        default:
                        {
                                // TODO???
                                return HURL_STATUS_ERROR;
                        }
                        }
                        return HURL_STATUS_ERROR;
                }
                // -----------------------------------------
                // TODO
                // -----------------------------------------
                default:
                {
                        return HURL_STATUS_ERROR;
                }
                }
                return HURL_STATUS_ERROR;
        }
        // -------------------------------------------------
        // STATE: DONE
        // -------------------------------------------------
        case ns_hurl::nconn::NC_STATE_DONE:
        {
                session *l_ses = static_cast<session *>(l_nconn->get_data());
                CHECK_FOR_NULL_ERROR(l_ses);
                CHECK_FOR_NULL_ERROR(l_t_phurl);
                int32_t l_s;
                l_s = l_ses->teardown(ns_hurl::HTTP_STATUS_OK);
                if(l_s != HURL_STATUS_OK)
                {
                        TRC_ERROR("performing teardown\n");
                        return HURL_STATUS_ERROR;
                }
                delete l_ses;
                l_ses = NULL;
                return HURL_STATUS_DONE;
        }
        // -------------------------------------------------
        // default
        // -------------------------------------------------
        default:
        {
                //NDBG_PRINT("default\n");
                TRC_ERROR("unexpected conn state %d\n", l_nconn->get_state());
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
void *t_phurl::t_run(void *a_nothing)
{
        int32_t l_s;
        l_s = init();
        if(l_s != HURL_STATUS_OK)
        {
                TRC_ERROR("performing init.\n");
                return NULL;
        }
        m_stopped = false;
        l_s = host_dequeue();
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
                l_s = host_dequeue();
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
int32_t t_phurl::host_dequeue(void)
{
        uint32_t l_dq = 0;
        while(!m_host_queue.empty() &&
              !m_stopped &&
              (m_num_in_progress < m_num_parallel_max))
        {
                host_t *l_h = m_host_queue.front();
                m_host_queue.pop();
                int32_t l_s;
                l_s = host_start(l_h);
                if(l_s != HURL_STATUS_OK)
                {
                        // Log error
                        TRC_ERROR("performing host_start\n");
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
int32_t t_phurl::host_start(host_t *a_host)
{
        //NDBG_PRINT("%srequest%s --HOST: %s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, a_request.m_host.c_str());
        // Try get idle from proxy pool
        ns_hurl::nconn *l_nconn = NULL;
        l_nconn = create_new_nconn(*m_request, a_host);
        // try get from idle list
        if(!l_nconn)
        {
                // TODO fatal???
                return STATUS_ERROR;
        }
        //m_nconn_set.insert(l_nconn);
        // -------------------------------------------------
        // start writing request
        // -------------------------------------------------
        //NDBG_PRINT("%sSTARTING REQUEST...%s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF);
        l_nconn->set_data(NULL);
        int32_t l_s;
        ++g_req_num_requested;
        ++g_req_num_in_flight;
        l_s = session::evr_fd_writeable_cb(l_nconn);
        if(l_s != HURL_STATUS_OK)
        {
                TRC_ERROR("performing evr_fd_writeable_cb\n");
                return HURL_STATUS_ERROR;
        }
        // -------------------------------------------------
        // idle timer
        // -------------------------------------------------
        ns_hurl::evr_event_t *l_timer;
        l_s = m_evr_loop->add_event(g_conf_timeout_ms,
                                    session::evr_fd_timeout_cb,
                                    l_nconn,
                                    &(l_timer));
        if(l_s != HURL_STATUS_OK)
        {
                return session::evr_fd_error_cb(l_nconn);
        }
        l_nconn->set_timer_obj(l_timer);
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: conn opt helper macro
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
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ns_hurl::nconn *t_phurl::create_new_nconn(const request &a_request, host_t *a_host)
{
        ns_hurl::nconn *l_nconn = NULL;
        if(a_request.m_scheme == ns_hurl::SCHEME_TLS)
        {
                l_nconn = new ns_hurl::nconn_tls();
                _SET_NCONN_OPT(l_nconn,ns_hurl::nconn_tls::OPT_TLS_CTX,m_tls_ctx,sizeof(m_tls_ctx));
                _SET_NCONN_OPT(l_nconn,ns_hurl::nconn_tls::OPT_TLS_CIPHER_STR,a_request.m_conf_tls_cipher_list.c_str(),a_request.m_conf_tls_cipher_list.length());
                bool l_val = a_request.m_conf_tls_verify;
                _SET_NCONN_OPT(l_nconn, ns_hurl::nconn_tls::OPT_TLS_VERIFY, &(l_val), sizeof(bool));
                l_val = a_request.m_conf_tls_self_ok;
                _SET_NCONN_OPT(l_nconn, ns_hurl::nconn_tls::OPT_TLS_VERIFY_ALLOW_SELF_SIGNED, &(l_val), sizeof(bool));
                l_val = a_request.m_conf_tls_no_host_check;
                _SET_NCONN_OPT(l_nconn, ns_hurl::nconn_tls::OPT_TLS_VERIFY_NO_HOST_CHECK, &(l_val), sizeof(bool));
                l_val = a_request.m_conf_tls_sni;
                _SET_NCONN_OPT(l_nconn, ns_hurl::nconn_tls::OPT_TLS_SNI, &(l_val), sizeof(bool));
                if(!a_host->m_hostname.empty())
                {
                        _SET_NCONN_OPT(l_nconn, ns_hurl::nconn_tls::OPT_TLS_HOSTNAME, a_host->m_hostname.c_str(), a_host->m_hostname.length());
                }
                else
                {
                        _SET_NCONN_OPT(l_nconn, ns_hurl::nconn_tls::OPT_TLS_HOSTNAME, a_host->m_host.c_str(), a_request.m_host.length());
                }
        }
        else
        {
                l_nconn = new ns_hurl::nconn_tcp();
        }
        l_nconn->set_label(a_host->m_host);
        l_nconn->set_ctx(this);
        l_nconn->set_num_reqs_per_conn(1);
#if 0
        l_nconn->set_collect_stats(false);
        l_nconn->set_connect_only(false);
#endif
        l_nconn->set_evr_loop(m_evr_loop);
        l_nconn->setup_evr_fd(session::evr_fd_readable_cb,
                              session::evr_fd_writeable_cb,
                              session::evr_fd_error_cb);
        l_nconn->set_host_data(a_host);
        l_nconn->set_host_info(a_host->m_host_info);
        return l_nconn;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
session *t_phurl::session_create(ns_hurl::nconn *a_nconn)
{
        if(!a_nconn)
        {
                // TODO fatal???
                TRC_ERROR("a_nconn == NULL\n");
                return NULL;
        }
        //NDBG_PRINT("%ssubr label%s: %s --HOST: %s\n",
        //                ANSI_COLOR_FG_RED, ANSI_COLOR_OFF,
        //                m_subr.get_label().c_str(), m_subr.get_host().c_str());
        // -------------------------------------------------
        // setup session
        // -------------------------------------------------
        session *l_ses = NULL;
        if(a_nconn->get_alpn() == ns_hurl::nconn::ALPN_HTTP_VER_V2)
        {
                l_ses = new h2_session();
        }
        else
        {
                l_ses = new http_session();
        }
        //NDBG_PRINT("Adding http_data: %p.\n", l_clnt_session);
        // -------------------------------------------------
        // init
        // -------------------------------------------------
        l_ses->m_t_phurl = this;
        // Setup clnt_session
        l_ses->m_nconn = a_nconn;
        // -------------------------------------------------
        // setup q's
        // -------------------------------------------------
        l_ses->m_in_q = new ns_hurl::nbq(8*1024);
        l_ses->m_out_q = new ns_hurl::nbq(8*1024);
        return l_ses;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_phurl::session_cleanup(session *a_ses, ns_hurl::nconn *a_nconn)
{
        if(a_ses)
        {
                if(a_ses->m_t_phurl)
                {
                        a_ses->m_nconn = NULL;
                        delete a_ses;
                }
        }
        if(a_nconn)
        {
                if(a_nconn->get_timer_obj())
                {
                        m_evr_loop->cancel_event(a_nconn->get_timer_obj());
                        a_nconn->set_timer_obj(NULL);
                }
                //m_nconn_set.erase(a_nconn);
                a_nconn->nc_cleanup();
                delete a_nconn;
                // TODO Log error???
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: Prototypes
//: ----------------------------------------------------------------------------
void display_status_line(void);
void display_summary(void);
std::string dump_all_responses(host_list_t &a_host_list,
                               bool a_color,
                               bool a_pretty,
                               output_type_t a_output_type,
                               int a_part_map);
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
                //g_start_time_ms = ns_hurl::get_time_ms();
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
int32_t add_line(host_list_t &ao_host_list, uint16_t a_port, FILE *a_file_ptr)
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
                        TRC_OUTPUT("Error: hostnames must be shorter than %d chars\n", MAX_READLINE_SIZE);
                        return STATUS_ERROR;
                }
                // read full line
                // Nuke endline
                l_readline[l_readline_len - 1] = '\0';
                std::string l_host(l_readline);
                l_host.erase( std::remove_if( l_host.begin(), l_host.end(), ::isspace ), l_host.end() );
                if(!l_host.empty())
                {
                        host_t *l_h = new host_t();
                        l_h->m_host = l_host;
                        l_h->m_port = a_port;
                        ao_host_list.push_back(l_h);
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
        host_t *l_h = static_cast<host_t *>(a_ctx);
        l_h->m_host_info = *a_host_info;
        l_h->m_host_resolved = true;
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
        fprintf(a_stream, "       OpenSSL Version: 0x%lX\n", OPENSSL_VERSION_NUMBER);
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
        //ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_ERROR);
        //ns_hurl::trc_log_file_open("/dev/stdout");
        if(isatty(fileno(stdout)) == 0)
        {
                g_conf_color = false;
        }
        pthread_mutex_init(&g_sum_info_mutex, NULL);
        // -------------------------------------------------
        // request settings
        // -------------------------------------------------
        // -------------------------------------------------
        // request settings
        // -------------------------------------------------
        request *l_request = new request();
        l_request->m_save = true;
        l_request->m_keepalive = false;
        //l_request->set_num_reqs_per_conn(1);
        // -------------------------------------------------
        // default headers
        // -------------------------------------------------
        std::string l_ua = "phurl/";
        l_ua += HURL_VERSION;
        l_request->set_header("Accept", "*/*");
        l_request->set_header("User-Agent", l_ua);
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
                        //      TRC_OUTPUT("Found unspecified argument: %s --assuming url...\n", l_url.c_str());
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
                                        TRC_OUTPUT("Error reading body data from file: %s\n", l_arg.c_str() + 1);
                                        return STATUS_ERROR;
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
                        l_request->m_conf_tls_cipher_list = l_arg;
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
                        l_request->m_conf_tls_options = l_tls_options;
                        break;
                }
                // -----------------------------------------
                // tls verify
                // -----------------------------------------
                case 'K':
                {
                        l_request->m_conf_tls_verify = true;
                        break;
                }
                // -----------------------------------------
                // tls sni
                // -----------------------------------------
                case 'N':
                {
                        l_request->m_conf_tls_sni = true;
                        break;
                }
                // -----------------------------------------
                // tls self signed
                // -----------------------------------------
                case 'B':
                {
                        l_request->m_conf_tls_self_ok = true;
                        break;
                }
                // -----------------------------------------
                // tls skip host check
                // -----------------------------------------
                case 'M':
                {
                        l_request->m_conf_tls_no_host_check = true;
                        break;
                }
                // -----------------------------------------
                // tls ca file
                // -----------------------------------------
                case 'F':
                {
                        l_request->m_conf_tls_ca_file = l_arg;
                        break;
                }
                // -----------------------------------------
                // tls ca path
                // -----------------------------------------
                case 'L':
                {
                        l_request->m_conf_tls_ca_path = l_arg;
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
                                TRC_OUTPUT("Error parallel must be at least 1\n");
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
                                TRC_OUTPUT("Error num-threads must be 0 or greater\n");
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
                                TRC_OUTPUT("Error breaking header string: %s -not in <HEADER>:<VAL> format?\n", l_arg.c_str());
                                return STATUS_ERROR;
                        }
                        l_s = l_request->set_header(l_key, l_val);
                        if (l_s != 0)
                        {
                                TRC_OUTPUT("Error performing set_header: %s\n", l_arg.c_str());
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
                                TRC_OUTPUT("Error verb string: %s too large try < 64 chars\n", l_arg.c_str());
                                return STATUS_ERROR;
                        }
                        l_request->m_verb = l_arg;
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
                                TRC_OUTPUT("Error connection timeout must be > 0\n");
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
                                TRC_OUTPUT("Error: completion ratio must be > 0.0 and < 100.0\n");
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
                        g_verbose = true;
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
        // -------------------------------------------------
        // init w/ url
        // -------------------------------------------------
        int32_t l_s;
        l_s = l_request->init_with_url(l_url);
        if(l_s != STATUS_OK)
        {
                TRC_OUTPUT("Error performing init_with_url: url: %s\n", l_url.c_str());
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
        host_list_t *l_host_list = new host_list_t();
        // Read from command
        if(!l_execute_line.empty())
        {
                FILE *l_fp;
                int32_t l_s = HURL_STATUS_OK;
                l_fp = popen(l_execute_line.c_str(), "r");
                // Error executing...
                if(l_fp == NULL)
                {
                }
                l_s = add_line(*l_host_list, l_request->m_port, l_fp);
                if(HURL_STATUS_OK != l_s)
                {
                        return STATUS_ERROR;
                }

                l_s = pclose(l_fp);
                // Error reported by pclose()
                if (l_s == -1)
                {
                        TRC_OUTPUT("Error: performing pclose\n");
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
                FILE * l_fp;
                int32_t l_s = HURL_STATUS_OK;
                l_fp = fopen(l_host_file_str.c_str(),"r");
                if (NULL == l_fp)
                {
                        TRC_OUTPUT("Error opening file: %s.  Reason: %s\n", l_host_file_str.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }
                l_s = add_line(*l_host_list, l_request->m_port, l_fp);
                if(HURL_STATUS_OK != l_s)
                {
                        return STATUS_ERROR;
                }
                //printf("ADD_FILE: DONE: %s\n", a_url_file.c_str());
                l_s = fclose(l_fp);
                if (0 != l_s)
                {
                        TRC_OUTPUT("Error performing fclose.  Reason: %s\n", strerror(errno));
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
                        TRC_OUTPUT("Error performing stat on file: %s.  Reason: %s\n", l_host_file_json_str.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }
                // Check if is regular file
                if(!(l_stat.st_mode & S_IFREG))
                {
                        TRC_OUTPUT("Error opening file: %s.  Reason: is NOT a regular file\n", l_host_file_json_str.c_str());
                        return STATUS_ERROR;
                }
                // -----------------------------------------
                // Open file...
                // -----------------------------------------
                FILE * l_file;
                l_file = fopen(l_host_file_json_str.c_str(),"r");
                if (NULL == l_file)
                {
                        TRC_OUTPUT("Error opening file: %s.  Reason: %s\n", l_host_file_json_str.c_str(), strerror(errno));
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
                        TRC_OUTPUT("Error performing fread.  Reason: %s [%d:%d]\n",
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
                        TRC_OUTPUT("Error reading json from file: %s.  Reason: data is not an array\n",
                                        l_host_file_json_str.c_str());
                        return STATUS_ERROR;
                }
                // rapidjson uses SizeType instead of size_t.
                for(rapidjson::SizeType i_record = 0; i_record < l_doc.Size(); ++i_record)
                {
                        if(!l_doc[i_record].IsObject())
                        {
                                TRC_OUTPUT("Error reading json from file: %s.  Reason: array member not an object\n",
                                                l_host_file_json_str.c_str());
                                return STATUS_ERROR;
                        }
                        host_t *l_h = new host_t();
                        // "host" : "coolhost.com:443",
                        // "hostname" : "coolhost.com",
                        // "id" : "DE4D",
                        // "where" : "my_house"
                        if(l_doc[i_record].HasMember("host")) l_h->m_host = l_doc[i_record]["host"].GetString();
                        //else l_host.m_host = "NO_HOST";
                        if(l_doc[i_record].HasMember("hostname")) l_h->m_hostname = l_doc[i_record]["hostname"].GetString();
                        //else l_host.m_hostname = "NO_HOSTNAME";
                        if(l_doc[i_record].HasMember("id")) l_h->m_id = l_doc[i_record]["id"].GetString();
                        //else l_host.m_id = "NO_ID";
                        if(l_doc[i_record].HasMember("where")) l_h->m_where = l_doc[i_record]["where"].GetString();
                        //else l_host.m_where = "NO_WHERE";
                        if(l_doc[i_record].HasMember("port")) l_h->m_port = l_doc[i_record]["port"].GetUint();
                        else l_h->m_port = l_request->m_port;
                        l_host_list->push_back(l_h);
                }
                // -----------------------------------------
                // Close file...
                // -----------------------------------------
                l_s = fclose(l_file);
                if (HURL_STATUS_OK != l_s)
                {
                        TRC_OUTPUT("Error performing fclose.  Reason: %s\n", strerror(errno));
                        return STATUS_ERROR;
                }
        }
        // Read from stdin
        else
        {
                int32_t l_s = HURL_STATUS_OK;
                l_s = add_line(*l_host_list, l_request->m_port, stdin);
                if(HURL_STATUS_OK != l_s)
                {
                        return STATUS_ERROR;
                }
        }
        if(g_verbose)
        {
                TRC_OUTPUT("Showing hostname list:\n");
                for(host_list_t::iterator i_h = l_host_list->begin();
                    i_h != l_host_list->end();
                    ++i_h)
                {
                        TRC_OUTPUT("%s\n", (*i_h)->m_host.c_str());
                }
        }
        // -------------------------------------------
        // Sigint handler
        // -------------------------------------------
        if(signal(SIGINT, sig_handler) == SIG_ERR)
        {
                TRC_OUTPUT("Error: can't catch SIGINT\n");
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
                TRC_OUTPUT("Error: performing init with resolver\n");
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
                TRC_OUTPUT("Error: performing get_new_adns_ctx with resolver\n");
                return STATUS_ERROR;
        }

        l_resolver->set_max_parallel(100);
        l_resolver->set_timeout_s(5);
#endif
        // -------------------------------------------
        // queueing
        // -------------------------------------------
        //NDBG_PRINT("resolving addresses\n");
        g_conf_num_hosts = l_host_list->size();
        uint64_t l_last_time_ms = ns_hurl::get_time_ms();
        uint32_t l_size = l_host_list->size();
        host_list_t::iterator i_h = l_host_list->begin();
        while(!g_runtime_stop &&
              (g_dns_num_resolved < l_size))
        {
                for(;
                    (!g_runtime_stop) &&
                    //(g_dns_num_in_flight < 100) &&
                    (i_h != l_host_list->end());
                    ++i_h)
                {
                        // stats
                        ++g_dns_num_requested;
                        ++g_dns_num_in_flight;
                        host_t *l_h = (*i_h);
                        l_s = l_resolver->lookup_tryfast(l_h->m_host,
                                                         l_h->m_port,
                                                         l_h->m_host_info);
                        if(l_s == HURL_STATUS_OK)
                        {
                                //NDBG_PRINT("lookup %stryfast%s: %s:%d\n",
                                //           ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF,
                                //           l_r.m_host.c_str(), l_r.m_port);
                                l_h->m_host_resolved = true;
                                --g_dns_num_in_flight;
                                ++g_dns_num_resolved;
                                continue;
                        }
                        // ---------------------------------
                        // sync
                        // ---------------------------------
#ifdef ASYNC_DNS_SUPPORT
                        if(g_conf_dns_use_sync)
                        {
#endif
                                //NDBG_PRINT("lookup sync:  %s:%d\n", l_r.m_host.c_str(), l_r.m_port);
                                l_resolver->lookup_sync(l_h->m_host,
                                                        l_h->m_port,
                                                        l_h->m_host_info);
                                if(l_s != HURL_STATUS_OK)
                                {
                                        continue;
                                }
                                --g_dns_num_in_flight;
                                ++g_dns_num_resolved;
                                l_h->m_host_resolved = true;
#ifdef ASYNC_DNS_SUPPORT
                        }
                        // ---------------------------------
                        // async
                        // ---------------------------------
                        else
                        {
                                //NDBG_PRINT("lookup async: %s:%d\n", l_r.m_host.c_str(), l_r.m_port);
                                l_s = l_resolver->lookup_async(l_adns_ctx,
                                                               l_h->m_host,
                                                               l_h->m_port,
                                                               l_h,
                                                               &(l_h->m_adns_lookup_job_handle));
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
                //if(!g_conf_show_summary &&
                if(
                 (ns_hurl::get_delta_time_ms(l_last_time_ms) > 100))
                {
                        uint32_t l_dns_num_errors = g_dns_num_errors;
                        uint32_t l_dns_num_requested = g_dns_num_requested;
                        uint32_t l_dns_num_resolved = g_dns_num_resolved;
                        uint32_t l_dns_num_in_flight = g_dns_num_in_flight;
                        TRC_OUTPUT(": DNS requested: %8u || resolved: %8u/%8u || in_flight: %8u || errors: %8u\n",
                                   l_dns_num_requested,
                                   l_dns_num_resolved,
                                   l_size,
                                   l_dns_num_in_flight,
                                   l_dns_num_errors);
                        l_last_time_ms = ns_hurl::get_time_ms();
                }
        }
        // -------------------------------------------
        // Q resolved...
        // -------------------------------------------
        host_queue_t *l_host_queue = new host_queue_t();
        for(host_list_t::iterator i_h = l_host_list->begin();
            (i_h != l_host_list->end());
            ++i_h)
        {
                if(*i_h == NULL)
                {
                        continue;
                }
                if((*i_h)->m_host_resolved)
                {
                        //((*i_r)->m_host_info.show());
                        l_host_queue->push(*i_h);
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
        uint32_t l_q_size = l_host_queue->size();
        for(uint32_t i_t = 0; i_t < g_conf_num_threads; ++i_t)
        {
                t_phurl *l_t_phurl = new t_phurl(g_conf_num_parallel);
                l_t_phurl->m_request = l_request;
                g_t_phurl_list.push_back(l_t_phurl);
                l_t_phurl->init();
                l_t_phurl->m_tls_ctx = l_request->m_tls_ctx;
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
                while(!l_host_queue->empty() && l_dq_num)
                {
                        host_t *l_h = l_host_queue->front();
                        l_host_queue->pop();
                        l_t_phurl->m_host_queue.push(l_h);
                        --l_dq_num;
                }
                g_req_num_pending += l_t_phurl->m_host_queue.size();
                l_t_phurl->m_num_pending = l_t_phurl->m_host_queue.size();
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
                l_responses_str = dump_all_responses(*l_host_list,
                                                     l_use_color,
                                                     l_output_pretty,
                                                     l_output_mode,
                                                     l_output_part);
                if(l_output_file.empty())
                {
                        TRC_OUTPUT("%s\n", l_responses_str.c_str());
                }
                else
                {
                        int32_t l_num_bytes_written = 0;
                        int32_t l_s = 0;
                        // Open
                        FILE *l_file_ptr = fopen(l_output_file.c_str(), "w+");
                        if(l_file_ptr == NULL)
                        {
                                TRC_OUTPUT("Error performing fopen. Reason: %s\n", strerror(errno));
                                return STATUS_ERROR;
                        }

                        // Write
                        l_num_bytes_written = fwrite(l_responses_str.c_str(), 1, l_responses_str.length(), l_file_ptr);
                        if(l_num_bytes_written != (int32_t)l_responses_str.length())
                        {
                                TRC_OUTPUT("Error performing fwrite. Reason: %s\n", strerror(errno));
                                fclose(l_file_ptr);
                                return STATUS_ERROR;
                        }

                        // Close
                        l_s = fclose(l_file_ptr);
                        if(l_s != 0)
                        {
                                TRC_OUTPUT("Error performing fclose. Reason: %s\n", strerror(errno));
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
#if 0
        if(l_evr_loop)
        {
                delete l_evr_loop;
                l_evr_loop = NULL;
        }
#endif
        if(l_resolver)
        {
#ifdef ASYNC_DNS_SUPPORT
                if(l_adns_ctx)
                {
                        l_resolver->destroy_async(l_adns_ctx);
                }
#endif
                delete l_resolver;
                l_resolver = NULL;
        }
        if(l_request->m_scheme == ns_hurl::SCHEME_TLS)
        {
                int32_t l_s;
                l_s = ns_hurl::tls_cleanup();
                if(l_s != HURL_STATUS_OK)
                {
                        TRC_ERROR("performing tls_cleanup.\n");
                }
        }
        if(l_request)
        {
                delete l_request;
                l_request = NULL;
        }
#if 0
        for(t_phurl_list_t::iterator i_t = g_t_phurl_list.begin();
            i_t != g_t_phurl_list.end();
            ++i_t)
        {
                if(!*i_t)
                {
                        continue;
                }
                delete *i_t;
                *i_t = NULL;
        }
        if(l_host_list)
        {
                for(host_list_t::iterator i_h = l_host_list->begin();
                    i_h != l_host_list->end();
                    ++i_h)
                {
                        if(*i_h)
                        {
                                delete *i_h;
                                *i_h = NULL;
                        }
                }
                delete l_host_list;
                l_host_list = NULL;
        }
        if(l_host_queue)
        {
                delete l_host_queue;
                l_host_queue = NULL;
        }
#endif
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
        TRC_OUTPUT("****************** %sSUMMARY%s ******************** \n", l_header_str.c_str(), l_off_color.c_str());
        TRC_OUTPUT("| total hosts:                     %12u\n",g_conf_num_hosts);
        TRC_OUTPUT("| success:                         %12u\n",g_sum_success.v);
        TRC_OUTPUT("| error:                           %12u\n",g_sum_error.v);
        TRC_OUTPUT("| error address lookup:            %12u\n",g_sum_error_addr.v);
        TRC_OUTPUT("| error connectivity:              %12u\n",g_sum_error_conn.v);
        TRC_OUTPUT("| error unknown:                   %12u\n",g_sum_error_unknown.v);
        TRC_OUTPUT("| tls error cert hostname:         %12u\n",g_sum_error_tls_hostname.v);
        TRC_OUTPUT("| tls error cert self-signed:      %12u\n",g_sum_error_tls_self_signed.v);
        TRC_OUTPUT("| tls error cert expired:          %12u\n",g_sum_error_tls_expired.v);
        TRC_OUTPUT("| tls error cert issuer:           %12u\n",g_sum_error_tls_issuer.v);
        TRC_OUTPUT("| tls error other:                 %12u\n",g_sum_error_tls_other.v);
        // Sort
        typedef std::map<uint32_t, std::string> _sorted_map_t;
        _sorted_map_t l_sorted_map;
        TRC_OUTPUT("+--------------- %sALPN PROTOCOLS%s --------------- \n", l_protocol_str.c_str(), l_off_color.c_str());
        l_sorted_map.clear();
        for(summary_map_t::iterator i_s = g_sum_info_alpn.begin(); i_s != g_sum_info_alpn.end(); ++i_s)
        l_sorted_map[i_s->second] = i_s->first;
        for(_sorted_map_t::reverse_iterator i_s = l_sorted_map.rbegin(); i_s != l_sorted_map.rend(); ++i_s)
        TRC_OUTPUT("| %-32s %12u\n", i_s->second.c_str(), i_s->first);
        TRC_OUTPUT("+--------------- %sALPN STRING%s ------------------ \n", l_protocol_str.c_str(), l_off_color.c_str());
        l_sorted_map.clear();
        for(summary_map_t::iterator i_s = g_sum_info_alpn_full.begin(); i_s != g_sum_info_alpn_full.end(); ++i_s)
        l_sorted_map[i_s->second] = i_s->first;
        for(_sorted_map_t::reverse_iterator i_s = l_sorted_map.rbegin(); i_s != l_sorted_map.rend(); ++i_s)
        TRC_OUTPUT("| %-32s %12u\n", i_s->second.c_str(), i_s->first);
        TRC_OUTPUT("+--------------- %sSSL PROTOCOLS%s ---------------- \n", l_protocol_str.c_str(), l_off_color.c_str());
        l_sorted_map.clear();
        for(summary_map_t::iterator i_s = g_sum_info_tls_protocols.begin(); i_s != g_sum_info_tls_protocols.end(); ++i_s)
        l_sorted_map[i_s->second] = i_s->first;
        for(_sorted_map_t::reverse_iterator i_s = l_sorted_map.rbegin(); i_s != l_sorted_map.rend(); ++i_s)
        TRC_OUTPUT("| %-32s %12u\n", i_s->second.c_str(), i_s->first);
        TRC_OUTPUT("+--------------- %sSSL CIPHERS%s ------------------ \n", l_cipher_str.c_str(), l_off_color.c_str());
        l_sorted_map.clear();
        for(summary_map_t::iterator i_s = g_sum_info_tls_ciphers.begin(); i_s != g_sum_info_tls_ciphers.end(); ++i_s)
        l_sorted_map[i_s->second] = i_s->first;
        for(_sorted_map_t::reverse_iterator i_s = l_sorted_map.rbegin(); i_s != l_sorted_map.rend(); ++i_s)
        TRC_OUTPUT("| %-32s %12u\n", i_s->second.c_str(), i_s->first);
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
                TRC_OUTPUT("Done: %s%8u%s Reqd: %s%8u%s Pendn: %s%8u%s Flight: %s%8u%s Error: %s%8u%s\n",
                                ANSI_COLOR_FG_GREEN, g_req_num_completed.v, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_YELLOW, g_req_num_requested.v, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_CYAN, g_req_num_pending.v, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_WHITE, g_req_num_in_flight.v, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_RED, g_req_num_errors.v, ANSI_COLOR_OFF);
        }
        else
        {
                TRC_OUTPUT("Done: %8u Reqd: %8u Pendn: %8u Flight: %8u Error: %8u\n",
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
std::string dump_all_responses_line_dl(host_list_t &a_host_list,
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
        for(host_list_t::const_iterator i_h = a_host_list.begin();
            i_h != a_host_list.end();
            ++i_h)
        {
                const host_t &l_h = *(*i_h);
                bool l_fbf = false;
                // Host
                if(a_part_map & PART_HOST)
                {
                        sprintf(l_buf, "\"%shost%s\": \"%s\"",
                                        l_host_color.c_str(), l_off_color.c_str(),
                                        l_h.m_host.c_str());
                        ARESP(l_buf);
                        l_fbf = true;
                }
                // Server
                if(a_part_map & PART_SERVER)
                {

                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        sprintf(l_buf, "\"%sserver%s\": \"%s:%d\"",
                                        l_server_color.c_str(), l_server_color.c_str(),
                                        l_h.m_host.c_str(),
                                        l_h.m_port
                                        );
                        ARESP(l_buf);
                        l_fbf = true;

                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        sprintf(l_buf, "\"%sid%s\": \"%s\"",
                                        l_id_color.c_str(), l_id_color.c_str(),
                                        l_h.m_id.c_str()
                                        );
                        ARESP(l_buf);
                        l_fbf = true;

                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        sprintf(l_buf, "\"%swhere%s\": \"%s\"",
                                        l_id_color.c_str(), l_id_color.c_str(),
                                        l_h.m_where.c_str()
                                        );
                        ARESP(l_buf);
                        l_fbf = true;
                }
                // Status Code
                if(a_part_map & PART_STATUS_CODE)
                {
                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        const char *l_status_val_color = "";
                        if(a_color)
                        {
                                if(l_h.m_status == 200) l_status_val_color = ANSI_COLOR_FG_GREEN;
                                else l_status_val_color = ANSI_COLOR_FG_RED;
                        }
                        sprintf(l_buf, "\"%sstatus-code%s\": %s%d%s",
                                        l_status_color.c_str(), l_off_color.c_str(),
                                        l_status_val_color, l_h.m_status, l_off_color.c_str());
                        ARESP(l_buf);
                        l_fbf = true;
                }
#if 0
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
#endif
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
std::string dump_all_responses_json(host_list_t &a_host_list,
                                    int a_part_map)
{
        rapidjson::Document l_js_doc;
        l_js_doc.SetObject();
        rapidjson::Value l_js_array(rapidjson::kArrayType);
        rapidjson::Document::AllocatorType& l_js_allocator = l_js_doc.GetAllocator();
        for(host_list_t::const_iterator i_h = a_host_list.begin();
            i_h != a_host_list.end();
            ++i_h)
        {
                const host_t &l_h = *(*i_h);
                rapidjson::Value l_obj;
                l_obj.SetObject();

                // Host
                if(a_part_map & PART_HOST)
                {
                        JS_ADD_MEMBER("host", l_h.m_host.c_str());
                }

                // Server
                if(a_part_map & PART_SERVER)
                {
                        char l_server_buf[1024];
                        sprintf(l_server_buf, "%s:%d",
                                        l_h.m_host.c_str(),
                                        l_h.m_port);
                        JS_ADD_MEMBER("server", l_server_buf);
                        JS_ADD_MEMBER("id", l_h.m_id.c_str());
                        JS_ADD_MEMBER("where", l_h.m_where.c_str());
                        l_obj.AddMember("port", l_h.m_port, l_js_allocator);
                }
                if(!l_h.m_error_str.empty())
                {
                        l_obj.AddMember(rapidjson::Value("Error", l_js_allocator).Move(),
                                        rapidjson::Value(l_h.m_error_str.c_str(), l_js_allocator).Move(),
                                        l_js_allocator);
                        l_obj.AddMember("status-code", 500, l_js_allocator);
                        l_js_array.PushBack(l_obj, l_js_allocator);
                        continue;
                }
                // Status Code
                if(a_part_map & PART_STATUS_CODE)
                {
                        l_obj.AddMember("status-code", l_h.m_status, l_js_allocator);
                }
#if 0
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
#endif
                // ---------------------------------------------------
                // SSL connection info
                // ---------------------------------------------------
                // TODO Add flag -only add to output if flag
                // ---------------------------------------------------
                l_obj.AddMember(rapidjson::Value("Alpn", l_js_allocator).Move(),
                                rapidjson::Value(l_h.m_alpn_str.c_str(), l_js_allocator).Move(),
                                l_js_allocator);
                if(!l_h.m_tls_info_cipher_str.empty())
                {
                        l_obj.AddMember(rapidjson::Value("Cipher", l_js_allocator).Move(),
                                        rapidjson::Value(l_h.m_tls_info_cipher_str.c_str(), l_js_allocator).Move(),
                                        l_js_allocator);
                }
                if(!l_h.m_tls_info_protocol_str.empty())
                {
                        l_obj.AddMember(rapidjson::Value("Protocol", l_js_allocator).Move(),
                                        rapidjson::Value(l_h.m_tls_info_protocol_str.c_str(), l_js_allocator).Move(),
                                        l_js_allocator);
                }
#if 0
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
#endif
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
std::string dump_all_responses(host_list_t &a_host_list,
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
                l_responses_str = dump_all_responses_line_dl(a_host_list, a_color, a_pretty, a_part_map);
                break;
        }
        case OUTPUT_JSON:
        {
                l_responses_str = dump_all_responses_json(a_host_list, a_part_map);
                break;
        }
        default:
        {
                break;
        }
        }
        return l_responses_str;
}
