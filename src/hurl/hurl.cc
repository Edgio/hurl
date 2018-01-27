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
//: includes
//: ----------------------------------------------------------------------------
#include "tinymt64.h"
#include "nghttp2/nghttp2.h"
#include "hurl/status.h"
#include "hurl/dns/nlookup.h"
#include "hurl/nconn/nconn.h"
#include "hurl/nconn/nconn_tcp.h"
#include "hurl/nconn/nconn_tls.h"
#include "hurl/support/obj_pool.h"
#include "hurl/support/tls_util.h"
#include "hurl/support/ndebug.h"
#include "hurl/support/time_util.h"
#include "hurl/support/trace.h"
#include "hurl/support/string_util.h"
#include "hurl/support/atomic.h"
#include "hurl/support/nbq.h"
#include "hurl/support/file_util.h"
#include "hurl/evr/evr.h"
#include "hurl/http/cb.h"
#include "hurl/http/resp.h"
#include "hurl/http/api_resp.h"
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
#include <set>
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
// openssl
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>
// Json output
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
//: ----------------------------------------------------------------------------
//: constants
//: ----------------------------------------------------------------------------
#define NB_ENABLE  1
#define NB_DISABLE 0
#define LOCAL_ADDR_V4_MIN 0x7f000001
#define LOCAL_ADDR_V4_MAX 0x7ffffffe
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
static const std::string &get_path(void *a_rand);
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
        // TODO fix stat calcs
#if 0
        // Stats
        xstat_t m_stat_us_connect;
        xstat_t m_stat_us_first_response;
        xstat_t m_stat_us_end_to_end;
#endif
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
                // TODO fix stat calcs
#if 0
                m_stat_us_connect(),
                m_stat_us_first_response(),
                m_stat_us_end_to_end(),
#endif
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
                // TODO fix stat calcs
#if 0
                m_stat_us_connect.clear();
                m_stat_us_connect.clear();
                m_stat_us_first_response.clear();
                m_stat_us_end_to_end.clear();
#endif
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
//: types
//: ----------------------------------------------------------------------------
typedef struct range_struct {
        uint32_t m_start;
        uint32_t m_end;
} range_t;
typedef std::vector <range_t> range_vector_t;
class t_hurl;
typedef std::list <t_hurl *> t_hurl_list_t;
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
                m_conf_tls_sni(true),
                m_conf_tls_self_ok(),
                m_conf_tls_no_host_check(),
                m_conf_tls_ca_file(),
                m_conf_tls_ca_path(),
                m_port(0),
                m_expect_resp_body_flag(true),
                m_connect_only(false),
                m_keepalive(false),
                m_save(false),
                m_h1(false),
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
                m_h1(a_r.m_h1),
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
        bool m_h1;
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
//: session
//: ----------------------------------------------------------------------------
class session {
public:
        // -------------------------------------------------
        // public members
        // -------------------------------------------------
        ns_hurl::nconn *m_nconn;
        t_hurl *m_t_hurl;
        ns_hurl::evr_event_t *m_timer_obj;
        ns_hurl::nbq *m_in_q;
        ns_hurl::nbq *m_out_q;
        request *m_request;
        uint64_t m_idx;
        bool m_goaway;
        uint32_t m_streams_closed;
        uint32_t m_body_offset;
        // -------------------------------------------------
        // public methods
        // -------------------------------------------------
        session(void):
                m_nconn(NULL),
                m_t_hurl(NULL),
                m_timer_obj(NULL),
                m_in_q(NULL),
                m_out_q(NULL),
                m_request(NULL),
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
#if 0
//: ----------------------------------------------------------------------------
//: stream
//: ----------------------------------------------------------------------------
class stream {
public:
        // -------------------------------------------------
        // public members
        // -------------------------------------------------
        ns_hurl::nconn *m_nconn;
        t_hurl *m_t_hurl;
        request *m_request;
        nghttp2_session *m_ngxxx_session;
        ns_hurl::nbq *m_in_q;
        ns_hurl::nbq *m_out_q;
        // -------------------------------------------------
        // public methods
        // -------------------------------------------------
        stream(void):
                m_nconn(NULL),
                m_t_hurl(NULL),
                m_request(NULL),
                m_ngxxx_session(NULL),
                m_in_q(NULL),
                m_out_q(NULL)
        {
        }
        ~stream(void)
        {
        }
private:
        // -------------------------------------------------
        // private methods
        // -------------------------------------------------
        // Disallow copy/assign
        stream& operator=(const stream &);
        stream(const stream &);
        // -------------------------------------------------
        // private members
        // -------------------------------------------------
};
#endif
//: ----------------------------------------------------------------------------
//: t_hurl
//: ----------------------------------------------------------------------------
class t_hurl
{
public:
        // -------------------------------------------------
        // Types
        // -------------------------------------------------
        typedef std::set <ns_hurl::nconn *> nconn_set_t;
        typedef std::list <ns_hurl::nconn *> nconn_list_t;
        typedef ns_hurl::obj_pool <session> session_pool_t;
        // -------------------------------------------------
        // public methods
        // -------------------------------------------------
        t_hurl(const request &a_request,
               uint32_t a_max_parallel,
               int32_t a_num_to_request):
               m_stopped(true),
               m_t_run_thread(),
               m_nconn_set(),
               m_stat(),
               m_status_code_count_map(),
               m_num_in_progress(0),
               m_orphan_in_q(NULL),
               m_orphan_out_q(NULL),
               m_request(a_request),
               m_evr_loop(NULL),
               m_is_initd(false),
               m_num_conn_parallel_max(a_max_parallel),
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
                for(nconn_set_t::iterator i_n = m_nconn_set.begin();
                    i_n != m_nconn_set.end();
                    ++i_n)
                {
                        if(*i_n)
                        {
                                delete *i_n;
                        }
                }
                m_nconn_set.clear();
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
                ns_hurl::evr_event_t *l_t = static_cast<ns_hurl::evr_event_t *>(a_timer);
                return m_evr_loop->cancel_event(l_t);
        }
        int32_t session_cleanup(session *a_ses, ns_hurl::nconn *a_nconn);
        session *session_create(ns_hurl::nconn *a_nconn);
        bool can_request(void)
        {
                //NDBG_PRINT("m_num_conn_parallel_max: %d\n",(int)m_num_conn_parallel_max);
                //NDBG_PRINT("m_num_in_progress:  %d\n",(int)m_num_in_progress);
                //NDBG_PRINT("m_num_to_request:   %d\n",(int)m_num_to_request);
                //NDBG_PRINT("m_stat.m_reqs:      %d\n",(int)m_stat.m_reqs);
                if(!g_test_finished &&
                   !m_stopped &&
                   (m_num_in_progress < m_num_conn_parallel_max) &&
                   ((m_num_to_request < 0) ||
                    ((uint32_t)m_num_to_request > m_stat.m_reqs)))
                {
                        //NDBG_PRINT("CAN:                TRUE\n");
                        return true;
                }
                //NDBG_PRINT("CAN:                FALSE\n");
                return false;
        }
        // -------------------------------------------------
        // public members
        // -------------------------------------------------
        sig_atomic_t m_stopped;
        pthread_t m_t_run_thread;
        nconn_set_t m_nconn_set;
        t_stat_cntr_t m_stat;
        status_code_count_map_t m_status_code_count_map;
        uint32_t m_num_in_progress;
        ns_hurl::nbq *m_orphan_in_q;
        ns_hurl::nbq *m_orphan_out_q;
        request m_request;
private:
        // -------------------------------------------------
        // private methods
        // -------------------------------------------------
        // Disallow copy/assign
        t_hurl& operator=(const t_hurl &);
        t_hurl(const t_hurl &);
        //Helper for pthreads
        static void *t_run_static(void *a_context)
        {
                return reinterpret_cast<t_hurl *>(a_context)->t_run(NULL);
        }
        ns_hurl::nconn *create_conn(void);
        int32_t request_start(ns_hurl::nconn *a_nconn);
        int32_t conn_start(void);
        // -------------------------------------------------
        // private members
        // -------------------------------------------------
        ns_hurl::evr_loop *m_evr_loop;
        bool m_is_initd;
        uint32_t m_num_conn_parallel_max;
        int32_t m_num_to_request;
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
        if((l_ses->m_nconn->can_reuse() == false) ||
           (l_ses->m_request->m_keepalive == false))
        {
                l_ses->m_goaway = true;
        }
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
        request *l_request = l_ses->m_request;
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
        if(!m_t_hurl)
        {
                TRC_ERROR("m_t_hurl == NULL\n");
                return HURL_STATUS_ERROR;
        }
        ++m_t_hurl->m_stat.m_reqs;
        ++m_t_hurl->m_num_in_progress;
        m_out_q->reset_read();
        if(!g_path_multi &&
           m_out_q->read_avail())
        {
                goto setup_resp;
        }
        {
        // TODO grab from path...
        std::string l_uri;
        l_uri = get_path(g_path_rand_ptr);
        if(l_uri.empty())
        {
                l_uri = "/";
        }
        if(!(m_request->m_url_query.empty()))
        {
                l_uri += "?";
                l_uri += m_request->m_url_query;
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
                        m_request->m_verb.c_str(), l_uri.c_str());
        ns_hurl::nbq_write_request_line(*m_out_q, l_buf, l_len);
        m_request->set_header("Host", m_request->m_host);

        ns_hurl::kv_map_list_t::const_iterator i_hdr;
        bool l_specd_host = false;

#define STRN_CASE_CMP(_a,_b) (strncasecmp(_a, _b, strlen(_a)) == 0)

#define SET_IF_V1(_key) do { \
i_hdr = m_request->m_headers.find(_key);\
if(i_hdr != m_request->m_headers.end()) { \
        ns_hurl::nbq_write_header(*m_out_q,\
                                  i_hdr->first.c_str(), i_hdr->first.length(),\
                                  i_hdr->second.front().c_str(),  i_hdr->second.front().length());\
        if (strcasecmp(i_hdr->first.c_str(), "host") == 0)\
        {\
                l_specd_host = true;\
        }\
}\
} while(0)
        SET_IF_V1("host");
        SET_IF_V1("user-agent");
        SET_IF_V1("accept");
        // -------------------------------------------------
        // Add repo headers
        // -------------------------------------------------
        for(ns_hurl::kv_map_list_t::const_iterator i_hl = m_request->m_headers.begin();
            i_hl != m_request->m_headers.end();
            ++i_hl)
        {
                if(STRN_CASE_CMP("host", i_hl->first.c_str()) ||
                   STRN_CASE_CMP("accept", i_hl->first.c_str()) ||
                   STRN_CASE_CMP("user-agent", i_hl->first.c_str()))
                {
                        continue;
                }
                if(i_hl->first.empty() || i_hl->second.empty()) { continue;}
                for(ns_hurl::str_list_t::const_iterator i_v = i_hl->second.begin();
                    i_v != i_hl->second.end();
                    ++i_v)
                {
                        ns_hurl::nbq_write_header(*m_out_q, i_hl->first.c_str(), i_hl->first.length(), i_v->c_str(), i_v->length());
                        if (strcasecmp(i_hl->first.c_str(), "host") == 0)
                        {
                                l_specd_host = true;
                        }
                }
        }
        // -------------------------------------------------
        // Default Host if unspecified
        // -------------------------------------------------
        if(!l_specd_host)
        {
                ns_hurl::nbq_write_header(*m_out_q,
                                          "Host", strlen("Host"),
                                          m_request->m_host.c_str(), m_request->m_host.length());
        }
        // -------------------------------------------------
        // body
        // -------------------------------------------------
        if(m_request->m_body_data && m_request->m_body_data_len)
        {
                //NDBG_PRINT("Write: buf: %p len: %d\n", l_buf, l_len);
                ns_hurl::nbq_write_body(*m_out_q, m_request->m_body_data, m_request->m_body_data_len);
        }
        else
        {
                ns_hurl::nbq_write_body(*m_out_q, NULL, 0);
        }
        }
setup_resp:
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
        m_resp->m_expect_resp_body_flag = m_request->m_expect_resp_body_flag;
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
                        if(g_color) TRC_OUTPUT("%s", ANSI_COLOR_FG_CYAN);
                        m_resp->show();
                        if(g_color) TRC_OUTPUT("%s", ANSI_COLOR_OFF);
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
                // ---------------------------------
                // check for done
                // ---------------------------------
                if((m_nconn->can_reuse() == false) ||
                   (m_request->m_keepalive == false) ||
                   (m_resp->m_supports_keep_alives == false))
                {
                        m_goaway = true;
                }
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
        ++m_t_hurl->m_stat.m_reqs;
        ++m_t_hurl->m_num_in_progress;
        // TODO grab from path...
        std::string l_uri;
        l_uri = get_path(g_path_rand_ptr);
        if(l_uri.empty())
        {
                l_uri = "/";
        }
        if(!(m_request->m_url_query.empty()))
        {
                l_uri += "?";
                l_uri += m_request->m_url_query;
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
        l_hdrs_len = 4 + m_request->m_headers.size();
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
        SET_PSUEDO_HEADER(l_hdr_idx, ":method", m_request->m_verb); ++l_hdr_idx;
        SET_PSUEDO_HEADER(l_hdr_idx, ":path", l_uri); ++l_hdr_idx;
        SET_PSUEDO_HEADER(l_hdr_idx, ":scheme", l_scheme); ++l_hdr_idx;
        SET_PSUEDO_HEADER(l_hdr_idx, ":authority", m_request->m_host); ++l_hdr_idx;
        ns_hurl::kv_map_list_t::const_iterator i_hdr;
        // -----------------------------------------
        // std headers
        // -----------------------------------------
#define SET_IF(_key) do { \
        i_hdr = m_request->m_headers.find(_key);\
        if(i_hdr != m_request->m_headers.end()) { \
                SET_HEADER(l_hdr_idx, i_hdr->first, i_hdr->second.front()); \
                ++l_hdr_idx;\
        }\
} while(0)
        SET_IF("user-agent");
        SET_IF("accept");
        // -----------------------------------------
        // the rest...
        // -----------------------------------------
        for(i_hdr = m_request->m_headers.begin();
            i_hdr != m_request->m_headers.end();
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
        if(m_request->m_body_data &&
           m_request->m_body_data_len)
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
#if 0
        nghttp2_session_terminate_session(session_, NGHTTP2_NO_ERROR);
#endif
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
        if((a_status >= 100) && (a_status < 200)) {/* TODO log 1xx's? */}
        else if((a_status >= 200) && (a_status < 300)){++m_t_hurl->m_stat.m_resp_status_2xx;}
        else if((a_status >= 300) && (a_status < 400)){++m_t_hurl->m_stat.m_resp_status_3xx;}
        else if((a_status >= 400) && (a_status < 500)){++m_t_hurl->m_stat.m_resp_status_4xx;}
        else if((a_status >= 500) && (a_status < 600)){++m_t_hurl->m_stat.m_resp_status_5xx;}
        ++m_t_hurl->m_status_code_count_map[a_status];
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
        if(!m_t_hurl)
        {
                TRC_ERROR("m_t_hurl == NULL\n");
                return HURL_STATUS_ERROR;
        }
        if(!m_nconn)
        {
                TRC_ERROR("m_t_hurl == NULL\n");
                return HURL_STATUS_ERROR;
        }
        //NDBG_PRINT("m_t_hurl->m_num_in_progress: %d\n", (int)m_t_hurl->m_num_in_progress);
        m_nconn->bump_num_requested();
        if(m_t_hurl->m_num_in_progress)
        {
                --m_t_hurl->m_num_in_progress;
                ++m_t_hurl->m_stat.m_resp;
        }
        // -------------------------------------------------
        // sleep before next request
        // -------------------------------------------------
        if(g_rate_delta_us &&
           !g_test_finished)
        {
                usleep(g_rate_delta_us*g_num_threads);
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
        t_hurl *l_t_hurl = static_cast<t_hurl *>(l_nconn->get_ctx());
        // -------------------------------------------------
        // ERROR
        // -------------------------------------------------
        if(a_conn_mode == ns_hurl::EVR_MODE_ERROR)
        {
                NDBG_PRINT("EVR_MODE_ERROR\n");
                // ignore errors for free connections
                if(l_nconn->is_free())
                {
                        TRC_ERROR("call back for free connection\n");
                        return HURL_STATUS_OK;
                }
                session *l_ses = static_cast<session *>(l_nconn->get_data());
                if(l_ses)
                {
                        l_ses->cancel_timer(l_ses->m_timer_obj);
                        // TODO Check status
                        l_ses->m_timer_obj = NULL;
                        int32_t l_s;
                        l_s = l_t_hurl->session_cleanup(l_ses, l_nconn);
                        if(l_s != HURL_STATUS_OK)
                        {
                                TRC_ERROR("performing session_cleanup\n");
                                // TODO -error???
                        }
                        // TODO record???
                        return HURL_STATUS_DONE;
                }
                else
                {
                        int32_t l_s;
                        l_s = l_t_hurl->session_cleanup(NULL, l_nconn);
                        if(l_s != HURL_STATUS_OK)
                        {
                                TRC_ERROR("performing session_cleanup\n");
                                // TODO -error???
                        }
                        return HURL_STATUS_DONE;
                }
        }
        // -------------------------------------------------
        // TIMEOUT
        // -------------------------------------------------
        else if(a_conn_mode == ns_hurl::EVR_MODE_TIMEOUT)
        {
                NDBG_PRINT("EVR_MODE_TIMEOUT\n");
                // ignore timeout for free connections
                if(l_nconn->is_free())
                {
                        TRC_ERROR("call back for free connection\n");
                        return HURL_STATUS_OK;
                }
                session *l_ses = static_cast<session *>(l_nconn->get_data());
                // calc time since last active
                if(l_ses &&
                   l_t_hurl)
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
                        // Kill program
                        g_test_finished = true;
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
                        TRC_ERROR("performing ncconnect for host: %s.\n", l_nconn->get_label().c_str());
                        // Kill program
                        g_test_finished = true;
                        return HURL_STATUS_ERROR;
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
                // create session
                // -----------------------------------------
                session *l_ses = NULL;
                l_ses = l_t_hurl->session_create(l_nconn);
                if(!l_ses)
                {
                        TRC_ERROR("performing session_create for host: %s.\n", l_nconn->get_label().c_str());
                        return HURL_STATUS_ERROR;
                }
                // -----------------------------------------
                // on connected
                // -----------------------------------------
                l_s = l_ses->sconnected();
                if(l_s != HURL_STATUS_OK)
                {
                        TRC_ERROR("performing m_connected_cb for host: %s.\n", l_nconn->get_label().c_str());
                        return HURL_STATUS_ERROR;
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
                CHECK_FOR_NULL_ERROR(l_t_hurl);
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
                                l_in_q = l_t_hurl->m_orphan_in_q;
                        }
                        uint32_t l_read = 0;
                        int32_t l_s = ns_hurl::nconn::NC_STATUS_OK;
                        char *l_buf = NULL;
                        uint64_t l_off = l_in_q->get_cur_write_offset();
                        l_s = l_nconn->nc_read(l_in_q, &l_buf, l_read);
                        l_t_hurl->m_stat.m_bytes_read += l_read;
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
                                        l_out_q = l_t_hurl->m_orphan_out_q;
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
                                        //NDBG_PRINT("l_ses->m_goaway:         %d\n", l_ses->m_goaway);
                                        //NDBG_PRINT("l_nconn->can_reuse():    %d\n", l_nconn->can_reuse());
                                        //NDBG_PRINT("l_t_hurl->can_request(): %d\n", l_t_hurl->can_request());
                                        if(l_ses->m_goaway ||
                                            !l_nconn->can_reuse() ||
                                            !l_t_hurl->can_request())
                                        {
                                                //NDBG_PRINT("done\n");
                                                l_nconn->set_state_done();
                                                goto state_top;
                                        }
                                        //NDBG_PRINT("m_streams_closed: %u\n", l_ses->m_streams_closed);
                                        l_ses->srequest();
                                        a_conn_mode = ns_hurl::EVR_MODE_WRITE;
                                        l_ses->m_streams_closed = 0;
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
                                NDBG_PRINT("l_ses == NULL\n");
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
                                l_out_q = l_t_hurl->m_orphan_out_q;
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
                        l_t_hurl->m_stat.m_bytes_written += l_written;
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
                //NDBG_PRINT("NC_STATE_DONE\n");
                //TRC_ERROR("unexpected state DONE\n");
                session *l_ses = static_cast<session *>(l_nconn->get_data());
                int32_t l_s;
                l_s = l_t_hurl->session_cleanup(l_ses, l_nconn);
                if(l_s != HURL_STATUS_OK)
                {
                        TRC_ERROR("performing session_cleanup\n");
                        // TODO -error???
                }
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
        // -------------------------------------------------
        // Run server
        // -------------------------------------------------
        while((!m_stopped) &&
              ((m_num_to_request < 0) || ((uint32_t)m_num_to_request > m_stat.m_resp)))
        {
                // Subrequests
                l_s = conn_start();
                if(l_s != HURL_STATUS_OK)
                {
                        TRC_ERROR("performing subr_try_deq\n");
                        //return NULL;
                }
                //NDBG_PRINT("Running.\n");
                l_s = m_evr_loop->run();
                if(l_s != HURL_STATUS_OK)
                {
                        TRC_ERROR("performing evr_loop->run()\n");
                        // TODO log run failure???
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
int32_t t_hurl::conn_start(void)
{
        while(m_nconn_set.size() < m_num_conn_parallel_max)
        {
                // -----------------------------------------
                // create connection
                // -----------------------------------------
                ns_hurl::nconn *l_nconn = NULL;
                //NDBG_PRINT("%sCREATING NEW CONNECTION%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                l_nconn = create_conn();
                if(!l_nconn)
                {
                        // TODO fatal???
                        TRC_ERROR("l_nconn == NULL\n");
                        return HURL_STATUS_ERROR;
                }
                l_nconn->set_label(m_request.m_host);
                m_nconn_set.insert(l_nconn);
                // -----------------------------------------
                // start writing request
                // -----------------------------------------
                //NDBG_PRINT("%sSTARTING REQUEST...%s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF);
                l_nconn->set_data(NULL);
                int32_t l_s;
                l_s = session::evr_fd_writeable_cb(l_nconn);
                if(l_s != HURL_STATUS_OK)
                {
                        TRC_ERROR("performing evr_fd_writeable_cb\n");
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
ns_hurl::nconn *t_hurl::create_conn(void)
{
        // -------------------------------------------------
        // get new connection
        // -------------------------------------------------
        ns_hurl::nconn *l_nconn = NULL;
        if(m_request.m_scheme == ns_hurl::SCHEME_TLS)
        {
                l_nconn = new ns_hurl::nconn_tls();
                l_nconn->set_opt(ns_hurl::nconn_tls::OPT_TLS_CTX,
                                 m_request.m_tls_ctx,
                                 sizeof(m_request.m_tls_ctx));
                bool l_t = true;
                l_nconn->set_opt(ns_hurl::nconn_tls::OPT_TLS_SNI,
                                 &l_t,
                                 sizeof(l_t));
                l_nconn->set_opt(ns_hurl::nconn_tls::OPT_TLS_HOSTNAME,
                                 m_request.m_host.c_str(),
                                 m_request.m_host.length());
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
session *t_hurl::session_create(ns_hurl::nconn *a_nconn)
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
        if((a_nconn->get_alpn() == ns_hurl::nconn::ALPN_HTTP_VER_V2) &&
           (!m_request.m_h1))
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
        l_ses->m_t_hurl = this;
        l_ses->m_timer_obj = NULL;
        l_ses->m_request = &m_request;
        // Setup clnt_session
        l_ses->m_nconn = a_nconn;
        a_nconn->set_data(l_ses);
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
int32_t t_hurl::session_cleanup(session *a_ses, ns_hurl::nconn *a_nconn)
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
                        delete a_ses;
                }
        }
        if(a_nconn)
        {
                m_nconn_set.erase(a_nconn);
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
                TRC_OUTPUT("%s.%s.%d: Error performing bind. Reason: %s.\n",
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
                TRC_OUTPUT("HURL_STATUS_ERROR: Bad range start[%u] > end[%u]\n", l_start, l_end);
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
                        TRC_OUTPUT("HURL_STATUS_ERROR: Bad range for path: %s at pos: %zu\n", a_path, l_range_start_pos);
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
                        TRC_OUTPUT("HURL_STATUS_ERROR: performing convert_exp_to_range(%s)\n", l_range_exp.c_str());
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
                TRC_OUTPUT("SUBSTR: %s\n", i_substr->c_str());
        }

        for(range_vector_t::iterator i_range = ao_range_vector.begin();
                        i_range != ao_range_vector.end();
                        ++i_range)
        {
                TRC_OUTPUT("RANGE: %u -- %u\n", i_range->m_start, i_range->m_end);
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
                                TRC_OUTPUT("HURL_STATUS_ERROR: Performing parse_path(%s)\n", l_p);
                                return HURL_STATUS_ERROR;
                        }
                }
                // If empty path explode
                if(l_range_vector.size())
                {
                        l_s = path_exploder(std::string(""), l_path_substr_vector, 0, l_range_vector, 0);
                        if(l_s != HURL_STATUS_OK)
                        {
                                TRC_OUTPUT("HURL_STATUS_ERROR: Performing explode_path(%s)\n", l_p);
                                return HURL_STATUS_ERROR;
                        }
                        // DEBUG show paths
                        //printf(" -- Displaying Paths --\n");
                        //uint32_t i_path_cnt = 0;
                        //for(path_vector_t::iterator i_path = g_path_vector.begin();
                        //              i_path != g_path_vector.end();
                        //              ++i_path, ++i_path_cnt)
                        //{
                        //      TRC_OUTPUT(": [%6d]: %s\n", i_path_cnt, i_path->c_str());
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
                                TRC_OUTPUT("HURL_STATUS_ERROR: Bad value[%s] for key[%s]\n", l_val.c_str(), l_key.c_str());
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
                        TRC_OUTPUT("HURL_STATUS_ERROR: Unrecognized key[%s]\n", l_key.c_str());
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
                // TODO fix stat calcs
#if 0
                add_stat(ao_total.m_stat_us_connect , (*i_t)->m_stat.m_stat_us_connect);
                add_stat(ao_total.m_stat_us_first_response , (*i_t)->m_stat.m_stat_us_first_response);
                add_stat(ao_total.m_stat_us_end_to_end , (*i_t)->m_stat.m_stat_us_end_to_end);
#endif
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
        fprintf(a_stream, "Usage: hurl [http[s]://]hostname[:port]/path [options]\n");
        fprintf(a_stream, "Options are:\n");
        fprintf(a_stream, "  -h, --help           Display this help and exit.\n");
        fprintf(a_stream, "  -V, --version        Display the version number and exit.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Run Options:\n");
        fprintf(a_stream, "  -w, --no_wildcards   Don't wildcard the url.\n");
        fprintf(a_stream, "  -M, --mode           Request mode -if multipath [random(default) | sequential].\n");
        fprintf(a_stream, "  -d, --data           HTTP body data -supports curl style @ file specifier\n");
        fprintf(a_stream, "  -p, --parallel       Num parallel. Default: 100.\n");
        fprintf(a_stream, "  -f, --fetches        Num fetches.\n");
        fprintf(a_stream, "  -N, --calls          Number of requests per connection (or stream if H2)\n");
        fprintf(a_stream, "  -1, --h1             Force http 1.x\n");
        // TODO FIX!!!
#if 0
        fprintf(a_stream, "  -s, --streams        Number of streams per connection (H2 option)\n");
#endif
        fprintf(a_stream, "  -t, --threads        Number of parallel threads. Default: 1\n");
        fprintf(a_stream, "  -H, --header         Request headers -can add multiple ie -H<> -H<>...\n");
        fprintf(a_stream, "  -X, --verb           Request command -HTTP verb to use -GET/PUT/etc. Default GET\n");
        fprintf(a_stream, "  -l, --seconds        Run for <N> seconds .\n");
        fprintf(a_stream, "  -A, --rate           Max Request Rate -per sec.\n");
        fprintf(a_stream, "  -T, --timeout        Timeout (seconds).\n");
        fprintf(a_stream, "  -x, --no_stats       Don't collect stats -faster.\n");
        fprintf(a_stream, "  -I, --addr_seq       Sequence over local address range.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Display Options:\n");
        fprintf(a_stream, "  -v, --verbose        Verbose logging\n");
        fprintf(a_stream, "  -c, --no_color       Turn off colors\n");
        fprintf(a_stream, "  -C, --responses      Display http(s) response codes instead of request statistics\n");
        fprintf(a_stream, "  -L, --responses_per  Display http(s) response codes per interval instead of request statistics\n");
        fprintf(a_stream, "  -U, --update         Update output every N ms. Default 500ms.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Results Options:\n");
        fprintf(a_stream, "  -j, --json           Display results in json\n");
        fprintf(a_stream, "  -o, --output         Output results to file <FILE> -default to stdout\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Debug Options:\n");
        fprintf(a_stream, "  -r, --trace          Turn on tracing (error/warn/debug/verbose/all)\n");
#ifdef ENABLE_PROFILER
        fprintf(a_stream, "  -G, --gprofile       Google profiler output file\n");
#endif
        fprintf(a_stream, "  \n");
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
        //ns_hurl::trc_log_file_open("/dev/stdout");
        //ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_ERROR);
        ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_NONE);
        ns_hurl::tls_init();
        // TODO check result...
        if(isatty(fileno(stdout)))
        {
                g_color = true;
        }
        // -------------------------------------------------
        // request settings
        // -------------------------------------------------
        request *l_request = new request();
        l_request->m_save = false;
        l_request->m_keepalive = true;
        //l_request->set_num_reqs_per_conn(1);
        // -------------------------------------------------
        // default headers
        // -------------------------------------------------
        std::string l_ua = "hurl/";
        l_ua += HURL_VERSION;
        l_request->set_header("Accept", "*/*");
        l_request->set_header("User-Agent", l_ua);
        // -------------------------------------------------
        // default tls config
        // -------------------------------------------------
        // defaults from nghttp2 client example
        l_request->m_conf_tls_options =
                SSL_OP_ALL |
                SSL_OP_NO_SSLv2 |
                SSL_OP_NO_SSLv3 |
                SSL_OP_NO_COMPRESSION |
                SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION;
        // -------------------------------------------------
        // init rand...
        // -------------------------------------------------
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
                { "calls",          1, 0, 'N' },
                { "h1",             0, 0, '1' },
                { "streams",        1, 0, 's' },
                { "threads",        1, 0, 't' },
                { "header",         1, 0, 'H' },
                { "verb",           1, 0, 'X' },
                { "rate",           1, 0, 'A' },
                { "mode",           1, 0, 'M' },
                { "seconds",        1, 0, 'l' },
                { "timeout",        1, 0, 'T' },
                { "no_stats",       0, 0, 'x' },
                { "addr_seq",       1, 0, 'I' },
                { "verbose",        0, 0, 'v' },
                { "no_color",       0, 0, 'c' },
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
        char l_short_arg_list[] = "hVwd:p:f:N:1t:H:X:A:M:l:T:xI:vcCLjo:U:r:G:";
#else
        char l_short_arg_list[] = "hVwd:p:f:N:1t:H:X:A:M:l:T:xI:vcCLjo:U:r:";
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
                                        TRC_OUTPUT("Error reading body data from file: %s\n", l_arg.c_str() + 1);
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
                        int32_t l_val = atoi(optarg);
                        if (l_val < 1)
                        {
                                TRC_OUTPUT("Error num parallel must be at least 1\n");
                                return HURL_STATUS_ERROR;
                        }
                        l_num_parallel = l_val;
                        break;
                }
                // -----------------------------------------
                // fetches
                // -----------------------------------------
                case 'f':
                {
                        int32_t l_val = atoi(optarg);
                        if (l_val < 1)
                        {
                                TRC_OUTPUT("Error fetches must be at least 1\n");
                                return HURL_STATUS_ERROR;
                        }
                        l_num_to_request = l_val;
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
                                TRC_OUTPUT("Error num-calls must be at least 1");
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
                // force client to use http 1.x
                // -----------------------------------------
                case '1':
                {
                       l_request->m_h1 = true;
                }
                // -----------------------------------------
                // number of streams per connection
                // -----------------------------------------
                case 's':
                {
                        // TODO
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
                                TRC_OUTPUT("Error num-threads must be 0 or greater\n");
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
                                TRC_OUTPUT("Error breaking header string: %s -not in <HEADER>:<VAL> format?\n", l_arg.c_str());
                                return HURL_STATUS_ERROR;
                        }
                        l_s = l_request->set_header(l_key, l_val);
                        if (l_s != 0)
                        {
                                TRC_OUTPUT("Error performing set_header: %s\n", l_arg.c_str());
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
                                TRC_OUTPUT("Error verb string: %s too large try < 64 chars\n", l_arg.c_str());
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
                        int l_val = atoi(optarg);
                        if (l_val < 1)
                        {
                                TRC_OUTPUT("Error: rate must be at least 1\n");
                                //print_usage(stdout, -1);
                                return HURL_STATUS_ERROR;
                        }
                        g_rate_delta_us = 1000000 / l_val;
                        break;
                }
                // -----------------------------------------
                // Mode
                // -----------------------------------------
                case 'M':
                {
                        std::string l_val = optarg;
                        if(l_val == "sequential") { g_path_order_random = false; }
                        else if(l_val == "random"){ g_path_order_random = true;}
                        else
                        {
                                TRC_OUTPUT("Error: Mode must be [roundrobin|sequential|random]\n");
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
                        int l_val = atoi(optarg);
                        if (l_val < 1)
                        {
                                TRC_OUTPUT("Error: seconds must be at least 1\n");
                                //print_usage(stdout, -1);
                                return HURL_STATUS_ERROR;
                        }
                        l_run_time_ms = l_val*1000;
                        break;
                }
                // -----------------------------------------
                // timeout
                // -----------------------------------------
                case 'T':
                {
                        //NDBG_PRINT("arg: --fetches: %s\n", optarg);
                        //g_end_type = END_FETCHES;
                        int l_val = atoi(optarg);
                        if (l_val < 1)
                        {
                                TRC_OUTPUT("timeout must be > 0\n");
                                //print_usage(stdout, -1);
                                return HURL_STATUS_ERROR;
                        }
                        l_request->m_timeout_ms = l_val*1000;
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
                // address sequency
                // -----------------------------------------
                case 'I':
                {
                        // TODO
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
                        int l_val = atoi(optarg);
                        if (l_val < 1)
                        {
                                TRC_OUTPUT("Error: Update interval must be > 0 ms\n");
                                //print_usage(stdout, -1);
                                return HURL_STATUS_ERROR;
                        }
                        l_interval_ms = l_val;
                        break;
                }
                // -----------------------------------------
                // trace
                // -----------------------------------------
                case 'r':
                {

#define ELIF_TRACE_STR(_level) else if(strncasecmp(_level, l_arg.c_str(), sizeof(_level)) == 0)

                        bool l_val = false;
                        if(0) {}
                        ELIF_TRACE_STR("error") { ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_ERROR); l_val = true; }
                        ELIF_TRACE_STR("warn") { ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_WARN); l_val = true; }
                        ELIF_TRACE_STR("debug") { ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_DEBUG); l_val = true; }
                        ELIF_TRACE_STR("verbose") { ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_VERBOSE); l_val = true; }
                        ELIF_TRACE_STR("all") { ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_ALL); l_val = true; }
                        else
                        {
                                ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_NONE);
                        }
                        if(l_val)
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
                TRC_OUTPUT("Error: can't catch SIGINT\n");
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
                TRC_OUTPUT("Error: performing init_with_url: %s\n", l_url.c_str());
                return HURL_STATUS_ERROR;
        }
        // -------------------------------------------
        // resolve
        // -------------------------------------------
        ns_hurl::host_info l_host_info;
        l_s = ns_hurl::nlookup(l_request->m_host, l_request->m_port, l_host_info);
        if(l_s != HURL_STATUS_OK)
        {
                TRC_OUTPUT("Error: resolving: %s:%d\n", l_request->m_host.c_str(), l_request->m_port);
                return HURL_STATUS_ERROR;
        }
        l_request->m_host_info = l_host_info;
        // -------------------------------------------
        // paths...
        // -------------------------------------------
        std::string l_raw_path = l_request->m_url_path;
        //printf("l_raw_path: %s\n",l_raw_path.c_str());
        if(l_wildcarding)
        {
                int32_t l_s;
                l_s = special_effects_parse(l_raw_path);
                if(l_s != HURL_STATUS_OK)
                {
                        TRC_OUTPUT("Error performing special_effects_parse with path: %s\n", l_raw_path.c_str());
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
        if(!g_verbose)
        {
                if(g_reqs_per_conn < 0)
                {
                        fprintf(stdout, "Running %d threads %d parallel connections per thread with infinite requests per connection\n",
                                g_num_threads, l_num_parallel);
                }
                else
                {
                        fprintf(stdout, "Running %d threads %d parallel connections per thread with %" PRIu64 " requests per connection\n",
                                        g_num_threads, l_num_parallel, g_reqs_per_conn);
                }
        }
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
                if(g_verbose)
                {
                        // skip stats
                        continue;
                }
                if(l_first_time)
                {
                if(l_show_response_codes)
                {
                TRC_OUTPUT("+-----------+-------------+-----------+-----------+-----------+-----------+-----------+-----------+\n");
                if(l_show_per_interval)
                {
                TRC_OUTPUT("| %s%9s%s / %s%11s%s / %s%9s%s / %s%9s%s / %s%9s%s | %s%9s%s | %s%9s%s | %s%9s%s | \n",
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
                TRC_OUTPUT("| %s%9s%s / %s%11s%s / %s%9s%s / %s%9s%s / %s%9s%s | %s%9s%s | %s%9s%s | %s%9s%s | \n",
                                l_c_fg_white, "Elapsed", l_c_off,
                                l_c_fg_white, "Req/s", l_c_off,
                                l_c_fg_white, "Cmpltd", l_c_off,
                                l_c_fg_white, "Errors", l_c_off,
                                l_c_fg_green, "200s", l_c_off,
                                l_c_fg_cyan, "300s", l_c_off,
                                l_c_fg_magenta, "400s", l_c_off,
                                l_c_fg_red, "500s", l_c_off);
                }
                TRC_OUTPUT("+-----------+-------------+-----------+-----------+-----------+-----------+-----------+-----------+\n");
                }
                else
                {
                TRC_OUTPUT("+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+\n");
                TRC_OUTPUT("| %s%9s%s / %s%9s%s | %s%9s%s | %s%9s%s | %s%12s%s | %9s | %11s | %9s |\n",
                                l_c_fg_green, "Completed", l_c_off,
                                l_c_fg_blue, "Requested", l_c_off,
                                l_c_fg_magenta, "IdlKil", l_c_off,
                                l_c_fg_red, "Errors", l_c_off,
                                l_c_fg_yellow, "kBytes Recvd", l_c_off,
                                "Elapsed",
                                "Req/s",
                                "MB/s");
                TRC_OUTPUT("+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+\n");
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
                TRC_OUTPUT("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %s%9.2f%s | %s%9.2f%s | %s%9.2f%s | %s%9.2f%s |\n",
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
                TRC_OUTPUT("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %s%9u%s | %s%9u%s | %s%9u%s | %s%9u%s |\n",
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
                TRC_OUTPUT("| %s%9" PRIu64 "%s / %s%9" PRIi64 "%s | %s%9" PRIu64 "%s | %s%9" PRIu64 "%s | %s%12.2f%s | %8.2fs | %10.2fs | %8.2fs |\n",
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

                // TODO fix stat calcs
#if 0
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
#endif
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
                // TODO fix stat calcs
#if 0
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
#endif
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
                        TRC_OUTPUT("%s\n", l_out_str.c_str());
                }
        }
        else
        {
                int32_t l_num_bytes_written = 0;
                int32_t l_s = 0;
                FILE *l_file_ptr = fopen(l_output_file.c_str(), "w+");
                if(l_file_ptr == NULL)
                {
                        TRC_OUTPUT("Error performing fopen. Reason: %s\n", strerror(errno));
                        return HURL_STATUS_ERROR;
                }
                l_num_bytes_written = fwrite(l_out_str.c_str(), 1, l_out_str.length(), l_file_ptr);
                if(l_num_bytes_written != (int32_t)l_out_str.length())
                {
                        TRC_OUTPUT("Error performing fwrite. Reason: %s\n", strerror(errno));
                        fclose(l_file_ptr);
                        return HURL_STATUS_ERROR;
                }
                l_s = fclose(l_file_ptr);
                if(l_s != 0)
                {
                        TRC_OUTPUT("Error performing fclose. Reason: %s\n", strerror(errno));
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
        // TODO delete SSL_CTX...
        if(l_ctx)
        {
                SSL_CTX_free(l_ctx);
                l_ctx = NULL;
        }
#endif
        return 0;
}
