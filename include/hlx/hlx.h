//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlx.h
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
#ifndef _HLX_H
#define _HLX_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
// For fixed size types
#include <stdint.h>
#include <math.h>
#include <netinet/in.h>
#include <stdio.h>

// For sig_atomic_t
#include <signal.h>

#include <string>
#include <map>
#include <list>
#include <vector>
#include <queue>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define HLX_SERVER_STATUS_OK 0
#define HLX_SERVER_STATUS_ERROR -1

//: ----------------------------------------------------------------------------
//: Extern Fwd Decl's
//: ----------------------------------------------------------------------------
struct ssl_ctx_st;
typedef ssl_ctx_st SSL_CTX;

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------
// Schemes
typedef enum scheme_enum {

        SCHEME_TCP = 0,
        SCHEME_SSL,
        SCHEME_NONE

} scheme_t;

// Status
typedef enum http_status_enum {

        HTTP_STATUS_OK = 200,
        HTTP_STATUS_NOT_FOUND = 404,

} http_status_t;

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class nbq;
class url_router;
class t_hlx;
class resolver;
class subreq;
class hlx;
class nconn;

//: ----------------------------------------------------------------------------
//: Structs
//: ----------------------------------------------------------------------------
// Offsets
typedef struct cr_struct
{
        const char *m_ptr;
        uint32_t m_len;

        cr_struct():
                m_ptr(NULL),
                m_len(0)
        {}

        void clear(void)
        {
                m_ptr = NULL;
                m_len = 0;
        }

} cr_t;

// Range
typedef struct range_struct
{

        uint32_t m_start;
        uint32_t m_end;

} range_t;

// Host
typedef struct host_struct {
        std::string m_host;
        std::string m_hostname;
        std::string m_id;
        std::string m_where;
        std::string m_url;
        uint16_t m_port;
        host_struct():
                m_host(),
                m_hostname(),
                m_id(),
                m_where(),
                m_url(),
                m_port(0)
        {};
} host_t;

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
#ifndef URL_PARAM_MAP_T
#define URL_PARAM_MAP_T
typedef std::map <std::string, std::string> url_param_map_t;
#endif
typedef std::list <host_t> host_list_t;
typedef std::list <cr_t> cr_list_t;
typedef std::list <std::string> str_list_t;
typedef std::map <std::string, str_list_t> kv_map_list_t;
typedef std::map<uint16_t, uint32_t > status_code_count_map_t;
typedef std::map <std::string, uint32_t> summary_map_t;
typedef std::queue <subreq *> subreq_queue_t;
typedef std::list <subreq *> subreq_list_t;
typedef std::list <std::string> server_list_t;

// Summary info
typedef struct summary_info_struct {
        uint32_t m_success;
        uint32_t m_error_addr;
        uint32_t m_error_conn;
        uint32_t m_error_unknown;
        uint32_t m_ssl_error_self_signed;
        uint32_t m_ssl_error_expired;
        uint32_t m_ssl_error_other;
        summary_map_t m_ssl_protocols;
        summary_map_t m_ssl_ciphers;

        summary_info_struct();
} summary_info_t;

//: ----------------------------------------------------------------------------
//: Stats
//: ----------------------------------------------------------------------------
// xstat
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
        double stdev() const { return sqrt(var()); }

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

// All Stat Aggregation..
typedef struct t_stat_struct
{
        // Stats
        xstat_t m_stat_us_connect;
        xstat_t m_stat_us_first_response;
        xstat_t m_stat_us_end_to_end;

        // Totals
        uint64_t m_total_bytes;
        uint64_t m_total_reqs;

        // Connection stats
        uint64_t m_num_resolved;
        uint64_t m_num_conn_started;
        uint32_t m_cur_conn_count;
        uint64_t m_num_conn_completed;
        uint64_t m_num_reqs;
        uint64_t m_num_idle_killed;
        uint64_t m_num_errors;
        uint64_t m_num_bytes_read;
        uint64_t m_num_bytes_written;

        status_code_count_map_t m_status_code_count_map;

        t_stat_struct():
                m_stat_us_connect(),
                m_stat_us_first_response(),
                m_stat_us_end_to_end(),
                m_total_bytes(0),
                m_total_reqs(0),
                m_num_resolved(0),
                m_num_conn_started(0),
                m_cur_conn_count(0),
                m_num_conn_completed(0),
                m_num_reqs(0),
                m_num_idle_killed(0),
                m_num_errors(0),
                m_num_bytes_read(0),
                m_num_bytes_written(0),
                m_status_code_count_map()
        {}
        void clear()
        {
                // Stats
                m_stat_us_connect.clear();
                m_stat_us_connect.clear();
                m_stat_us_first_response.clear();
                m_stat_us_end_to_end.clear();

                // Totals
                m_total_bytes = 0;
                m_total_reqs = 0;

                // Client stats
                m_num_resolved = 0;
                m_num_conn_started = 0;
                m_cur_conn_count = 0;
                m_num_conn_completed = 0;
                m_num_reqs = 0;
                m_num_idle_killed = 0;
                m_num_errors = 0;
                m_num_bytes_read = 0;
                m_num_bytes_written = 0;

                m_status_code_count_map.clear();
        }
} t_stat_t;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class http_req
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        http_req();
        ~http_req();
        void clear(void);
        void show(void);
        void show_headers(void);

        // Get parsed results
        // TODO -copy for now -zero copy later???
        const kv_map_list_t &get_headers(void);
        const std::string &get_body(void);
        void set_q(nbq *a_q) { m_q = a_q;}
        nbq *get_q(void) { return m_q;}
        void set_conn(nconn *a_nconn) { m_nconn = a_nconn;}
        nconn *get_conn(void) { return m_nconn;}

        const std::string &get_url_path(void);

        // Write parts
        int32_t write_request_line(char *a_buf, uint32_t a_len);
        int32_t write_header(const char *a_key, const char *a_val);
        int32_t write_header(const char *a_key, uint32_t a_key_len, const char *a_val, uint32_t a_val_len);
        int32_t write_body(const char *a_body, uint32_t a_body_len);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------

        // ---------------------------------------
        // raw http request offsets
        // ---------------------------------------
        cr_t m_p_url;
        cr_t m_p_status;
        cr_list_t m_p_h_list_key;
        cr_list_t m_p_h_list_val;
        cr_t m_p_body;
        int m_http_major;
        int m_http_minor;
        int m_method;

        // ---------------------------------------
        // ...
        // ---------------------------------------
        uint16_t m_status;
        bool m_complete;
        bool m_supports_keep_alives;
        subreq *m_subreq;

private:
        // Disallow copy/assign
        http_req& operator=(const http_req &);
        http_req(const http_req &);

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        int32_t parse_url(void);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        kv_map_list_t m_headers;
        std::string m_body;
        nbq *m_q;
        nconn *m_nconn;

        // Parsed members
        bool m_url_parsed;
        std::string m_parsed_url_path;

};

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class http_resp
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        http_resp();
        ~http_resp();
        void clear(void);
        void show(void);
        void show_headers(void);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        void set_status(uint16_t a_code) {m_status = a_code;}
        uint16_t get_status(void) {return m_status;}
        void set_body(const std::string & a_body) {m_body = a_body;}

        // Get parsed results
        // TODO -copy for now -zero copy later???
        const kv_map_list_t &get_headers(void);
        int32_t get_body(char **a_buf, uint32_t &a_len);

        void set_q(nbq *a_q) { m_q = a_q;}
        nbq *get_q(void) { return m_q;}

        // Write parts
        int32_t write_status(http_status_t a_status);
        int32_t write_header(const char *a_key, const char *a_val);
        int32_t write_header(const char *a_key, uint32_t a_key_len, const char *a_val, uint32_t a_val_len);
        int32_t write_body(const char *a_body, uint32_t a_body_len);

        // ---------------------------------------
        // raw http request offsets
        // ---------------------------------------
        cr_t m_p_status;
        cr_list_t m_p_h_list_key;
        cr_list_t m_p_h_list_val;
        cr_t m_p_body;
        bool m_complete;

        const char *m_ssl_info_protocol_str;
        const char *m_ssl_info_cipher_str;

private:
        // Disallow copy/assign
        http_resp& operator=(const http_resp &);
        http_resp(const http_resp &);

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        uint16_t m_status;
        kv_map_list_t m_headers;
        std::string m_body;
        nbq *m_q;
};

//: ----------------------------------------------------------------------------
//: http_request_handler
//: ----------------------------------------------------------------------------
class http_request_handler
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        http_request_handler(void) {};
        virtual ~http_request_handler(){};

        virtual int32_t do_get(hlx &a_hlx, nconn &a_nconn, http_req &a_request, const url_param_map_t &a_url_param_map, http_resp &ao_resp) = 0;
        virtual int32_t do_post(hlx &a_hlx, nconn &a_nconn, http_req &a_request, const url_param_map_t &a_url_param_map, http_resp &ao_resp) = 0;
        virtual int32_t do_put(hlx &a_hlx, nconn &a_nconn, http_req &a_request, const url_param_map_t &a_url_param_map, http_resp &ao_resp) = 0;
        virtual int32_t do_delete(hlx &a_hlx, nconn &a_nconn, http_req &a_request, const url_param_map_t &a_url_param_map, http_resp &ao_resp) = 0;
        virtual int32_t do_default(hlx &a_hlx, nconn &a_nconn, http_req &a_request, const url_param_map_t &a_url_param_map, http_resp &ao_resp) = 0;

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        int32_t get_file(nconn &a_nconn, http_req &a_request, const std::string &a_path, http_resp &ao_resp);
        int32_t send_not_found(nconn &a_nconn, const http_req &a_request, const char *a_resp_str, http_resp &ao_resp);
private:
        // Disallow copy/assign
        http_request_handler& operator=(const http_request_handler &);
        http_request_handler(const http_request_handler &);
};

//: ----------------------------------------------------------------------------
//: default_http_request_handler
//: ----------------------------------------------------------------------------
class default_http_request_handler: public http_request_handler
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        default_http_request_handler(void);
        ~default_http_request_handler();

        int32_t do_get(hlx &a_hlx, nconn &a_nconn, http_req &a_request, const url_param_map_t &a_url_param_map, http_resp &ao_resp);
        int32_t do_post(hlx &a_hlx, nconn &a_nconn, http_req &a_request, const url_param_map_t &a_url_param_map, http_resp &ao_resp);
        int32_t do_put(hlx &a_hlx, nconn &a_nconn, http_req &a_request, const url_param_map_t &a_url_param_map, http_resp &ao_resp);
        int32_t do_delete(hlx &a_hlx, nconn &a_nconn, http_req &a_request, const url_param_map_t &a_url_param_map, http_resp &ao_resp);
        int32_t do_default(hlx &a_hlx, nconn &a_nconn, http_req &a_request, const url_param_map_t &a_url_param_map, http_resp &ao_resp);
private:
        // Disallow copy/assign
        default_http_request_handler& operator=(const default_http_request_handler &);
        default_http_request_handler(const default_http_request_handler &);
};

//: ----------------------------------------------------------------------------
//: hlx
//: ----------------------------------------------------------------------------
class listener
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        listener(uint16_t a_port=12345, scheme_t a_scheme = SCHEME_TCP);
        ~listener();
        int32_t add_endpoint(const std::string &a_endpoint, const http_request_handler *a_handler);
        int32_t get_fd(void) const { return m_fd;}
        scheme_t get_scheme(void) const { return m_scheme;}
        url_router *get_url_router(void) const { return m_url_router;}
        int32_t init(void);

        // TODO migrate here from hlx object
#if 0
        void set_ssl_cipher_list(const std::string &a_cipher_list);
        void set_ssl_ca_path(const std::string &a_ssl_ca_path);
        void set_ssl_ca_file(const std::string &a_ssl_ca_file);
        int set_ssl_options(const std::string &a_ssl_options_str);
        int set_ssl_options(long a_ssl_options);
        void set_tls_key(const std::string &a_tls_key) {m_tls_key = a_tls_key;}
        void set_tls_crt(const std::string &a_tls_crt) {m_tls_crt = a_tls_crt;}
#endif

private:
        // Disallow copy/assign
        listener& operator=(const listener &);
        listener(const listener &);

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        scheme_t m_scheme;
        uint16_t m_port;

        // TODO migrate here from hlx object
#if 0
        SSL_CTX* m_ssl_ctx;
        std::string m_tls_key;
        std::string m_tls_crt;
        std::string m_ssl_cipher_list;
        std::string m_ssl_options_str;
        long m_ssl_options;
        std::string m_ssl_ca_file;
        std::string m_ssl_ca_path;
#endif

        url_router *m_url_router;
        int32_t m_fd;
        bool m_is_initd;
};

//: ----------------------------------------------------------------------------
//: hlx
//: ----------------------------------------------------------------------------
class hlx
{
public:
        //: --------------------------------------------------------------------
        //: Types
        //: --------------------------------------------------------------------
        typedef std::list <t_hlx *> t_hlx_list_t;
        typedef std::list <listener *> listener_list_t;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        hlx();
        ~hlx();

        // General
        void set_verbose(bool a_val) {m_verbose = a_val;}
        void set_color(bool a_val) {m_color = a_val;}
        void set_stats(bool a_val);

        // Settings
        void set_num_threads(uint32_t a_num_threads) {m_num_threads = a_num_threads;}
        void set_num_parallel(uint32_t a_num_parallel) { m_num_parallel = a_num_parallel;}
        void set_num_reqs_per_conn(int32_t a_num_reqs_per_conn) {m_num_reqs_per_conn = a_num_reqs_per_conn;}
        void set_start_time_ms(uint64_t a_start_time_ms) {m_start_time_ms = a_start_time_ms;}
        int32_t add_listener(listener *a_listener);
        subreq &create_subreq(const char *a_label);
        subreq &create_subreq(const subreq &a_subreq);

        int32_t add_subreq(subreq &a_subreq);

        void set_collect_stats(bool a_val);
        void set_use_persistent_pool(bool a_val);
        void set_show_summary(bool a_val);

        // Socket options
        void set_sock_opt_no_delay(bool a_val);
        void set_sock_opt_send_buf_size(uint32_t a_send_buf_size);
        void set_sock_opt_recv_buf_size(uint32_t a_recv_buf_size);

        // Split requests
        void set_split_requests_by_thread(bool a_val);
        void set_use_ai_cache(bool a_val);
        void set_ai_cache(const std::string &a_ai_cache);

        // TODO migrate to listener
#if 1
        void set_ssl_cipher_list(const std::string &a_cipher_list);
        void set_ssl_ca_path(const std::string &a_ssl_ca_path);
        void set_ssl_ca_file(const std::string &a_ssl_ca_file);
        int set_ssl_options(const std::string &a_ssl_options_str);
        int set_ssl_options(long a_ssl_options);
        void set_tls_key(const std::string &a_tls_key) {m_tls_key = a_tls_key;}
        void set_tls_crt(const std::string &a_tls_crt) {m_tls_crt = a_tls_crt;}
#endif

        // Running...
        int32_t run(void);
        int32_t stop(void);
        int32_t wait_till_stopped(void);
        bool is_running(void);

        // Responses
        http_resp *create_response(nconn &a_nconn);
        int32_t queue_response(nconn &a_nconn, http_resp *a_resp);

        // Stats
        void get_stats(t_stat_t &ao_all_stats);
        int32_t get_stats_json(char *l_json_buf, uint32_t l_json_buf_max_len);
        void get_summary_info(summary_info_t &ao_summary_info);

private:
        // Disallow copy/assign
        hlx& operator=(const hlx &);
        hlx(const hlx &);

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        int init(void);
        int init_t_hlx_list(void);
        void add_to_total_stat_agg(t_stat_t &ao_stat_agg, const t_stat_t &a_add_total_stat);
        void add_subreq_t(subreq &a_subreq);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        bool m_verbose;
        bool m_color;
        bool m_stats;
        uint32_t m_num_threads;
        int32_t m_num_parallel;
        int64_t m_num_reqs_per_conn;
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;
        bool m_split_requests_by_thread;
        listener_list_t m_listener_list;
        subreq_queue_t m_subreq_queue;

        // TODO migrate to listener
#if 1
        SSL_CTX* m_ssl_server_ctx;
        std::string m_tls_key;
        std::string m_tls_crt;
        std::string m_ssl_cipher_list;
        std::string m_ssl_options_str;
        long m_ssl_options;
        std::string m_ssl_ca_file;
        std::string m_ssl_ca_path;
#endif
        SSL_CTX *m_ssl_client_ctx;
        resolver *m_resolver;
        bool m_use_ai_cache;
        std::string m_ai_cache;
        bool m_collect_stats;
        bool m_use_persistent_pool;
        bool m_show_summary;
        uint64_t m_start_time_ms;
        t_hlx_list_t m_t_hlx_list;
        int m_evr_loop_type;
        bool m_is_initd;
};

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef struct host_info_struct {
        struct sockaddr_in m_sa;
        int m_sa_len;
        int m_sock_family;
        int m_sock_type;
        int m_sock_protocol;

        host_info_struct():
                m_sa(),
                m_sa_len(16),
                m_sock_family(AF_INET),
                m_sock_type(SOCK_STREAM),
                m_sock_protocol(IPPROTO_TCP)
        {
                m_sa.sin_family = AF_INET;
        };

        void show(void)
        {
                printf("+-----------+\n");
                printf("| Host Info |\n");
                printf("+-----------+-------------------------\n");
                printf(": m_sock_family:   %d\n", m_sock_family);
                printf(": m_sock_type:     %d\n", m_sock_type);
                printf(": m_sock_protocol: %d\n", m_sock_protocol);
                printf(": m_sa_len:        %d\n", m_sa_len);
                printf("+-------------------------------------\n");
        };

} host_info_t;

//: ----------------------------------------------------------------------------
//: subreq
//: ----------------------------------------------------------------------------
class subreq {

public:
        // ---------------------------------------
        // Subreq types
        // ---------------------------------------
        typedef enum {

                SUBREQ_TYPE_NONE = 0,
                SUBREQ_TYPE_FANOUT = 1,
                SUBREQ_TYPE_DUPE = 1 << 1

        } subreq_type_t;

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

        // ---------------------------------------
        // Callbacks
        // ---------------------------------------
        typedef int32_t (*error_cb_t)(nconn &, subreq &);
        typedef int32_t (*completion_cb_t)(nconn &, subreq &, http_resp &);
        typedef int32_t (*create_req_cb_t)(subreq &, http_req &);

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        subreq(std::string a_id);
        subreq(const subreq &a_subreq);
        ~subreq();
        void reset(void);
        int32_t init_with_url(const std::string &a_url);
        int32_t resolve(resolver *a_resolver);
        bool is_resolved(void) {return m_is_resolved_flag;}
        void set_host(const std::string &a_host);
        void set_port(uint16_t &a_port) { m_port = a_port;}
        void set_response(uint16_t a_response_status, const char *a_response);
        void set_save_response(bool a_val) {m_save_response = a_val;}
        void set_verb(const std::string &a_verb) {m_verb = a_verb;}
        void set_timeout_s(int32_t a_timeout_s) {m_timeout_s = a_timeout_s;}
        void set_error_cb(error_cb_t a_cb) {m_error_cb = a_cb;}
        void set_completion_cb(completion_cb_t a_cb) {m_completion_cb = a_cb;}
        void set_child_completion_cb(completion_cb_t a_cb) {m_child_completion_cb = a_cb;}
        void set_create_req_cb(create_req_cb_t a_cb) {m_create_req_cb = a_cb;}
        void set_num_to_request(int32_t a_val) {m_num_to_request = a_val;}
        int32_t get_num_to_request(void) {return m_num_to_request;}
        void set_num_reqs_per_conn(int32_t a_val) {m_num_reqs_per_conn = a_val;}
        int32_t get_num_reqs_per_conn(void) {return m_num_reqs_per_conn;}
        void bump_num_requested(void) {++m_num_requested;}
        int32_t get_num_requested(void) {return m_num_requested;}
        void bump_num_completed(void) {++m_num_completed;}
        int32_t get_num_completed(void) {return m_num_completed;}
        void set_connect_only(bool a_val) {m_connect_only = a_val;}
        bool get_connect_only(void) {return m_connect_only;}
        void set_keepalive(bool a_val);
        bool get_keepalive(void);

        // Headers
        int set_header(const std::string &a_header);
        int set_header(const std::string &a_key, const std::string &a_val);
        void clear_headers(void);

        // Host list
        int set_host_list(const host_list_t &a_host_list);
        int set_server_list(const server_list_t &a_server_list);

        // Resp
        void set_resp(http_resp *a_resp) {m_resp = a_resp;}
        http_resp *get_resp(void) {return m_resp;}

        // Requester connection
        void set_req_conn(nconn *a_nconn) {m_req_nconn = a_nconn;}
        nconn *get_req_conn(void) {return m_req_nconn;}

        // Requester response
        void set_req_resp(http_resp *a_resp) {m_req_resp = a_resp;}
        http_resp *get_req_resp(void) {return m_req_resp;}

        // Host list
        const host_list_t &get_host_list(void) const {return m_host_list;}

        bool is_done(void)
        {
                if((m_num_to_request == -1) || m_num_completed < (int32_t)m_num_to_request) return false;
                else return true;
        }

        bool is_pending_done(void)
        {
                if((m_num_to_request == -1) || m_num_requested < (int32_t)m_num_to_request) return false;
                else return true;
        }

        std::string dump_all_responses(bool a_color,
                                       bool a_pretty,
                                       output_type_t a_output_type,
                                       int a_part_map);
        void get_summary_info(summary_info_t &ao_summary_info);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        static int32_t create_request(subreq &a_subreq, http_req &a_req);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        std::string m_id;
        subreq_type_t m_type;
        std::string m_host;
        std::string m_path;
        std::string m_query;
        std::string m_fragment;
        std::string m_userinfo;
        std::string m_hostname;
        uint16_t m_port;
        std::string m_verb;
        int32_t m_timeout_s;
        bool m_keepalive;
        bool m_connect_only;
        error_cb_t m_error_cb;
        completion_cb_t m_completion_cb;
        completion_cb_t m_child_completion_cb;
        create_req_cb_t m_create_req_cb;
        std::string m_where;
        scheme_t m_scheme;
        host_info_t m_host_info;
        kv_map_list_t m_headers;
        bool m_save_response;
        char *m_body_data;
        uint32_t m_body_data_len;
        bool m_multipath;
        summary_info_t m_summary_info;
        subreq *m_parent;
        pthread_mutex_t m_parent_mutex;
        subreq_list_t m_child_list;

#if 0
        // TODO Cookies!!!
        void set_uri(const std::string &a_uri);
        const kv_list_map_t &get_uri_decoded_query(void);
#endif

private:

        // -------------------------------------------
        // Private methods
        // -------------------------------------------
        //DISALLOW_COPY_AND_ASSIGN(subreq)
        subreq& operator=(const subreq &a_subreq);

        std::string dump_all_responses_line_dl(bool a_color,bool a_pretty,int a_part_map);
        std::string dump_all_responses_json(int a_part_map);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        bool m_is_resolved_flag;

        // Resp object
        http_resp *m_resp;

        // Requester connection
        nconn *m_req_nconn;
        http_resp *m_req_resp;

        int32_t m_num_reqs_per_conn;
        int32_t m_num_to_request;
        sig_atomic_t m_num_requested;
        sig_atomic_t m_num_completed;

        // Host list
        host_list_t m_host_list;

};

//: ----------------------------------------------------------------------------
//: \details: Update stat with new value
//: \return:  n/a
//: \param:   ao_stat stat to be updated
//: \param:   a_val value to update stat with
//: ----------------------------------------------------------------------------
inline void update_stat(xstat_t &ao_stat, double a_val)
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
inline void add_stat(xstat_t &ao_stat, const xstat_t &a_from_stat)
{
        ao_stat.m_num += a_from_stat.m_num;
        ao_stat.m_sum_x += a_from_stat.m_sum_x;
        ao_stat.m_sum_x2 += a_from_stat.m_sum_x2;
        if(a_from_stat.m_min < ao_stat.m_min)
                ao_stat.m_min = a_from_stat.m_min;
        if(a_from_stat.m_max > ao_stat.m_max)
                ao_stat.m_max = a_from_stat.m_max;
}

//: ----------------------------------------------------------------------------
//: \details: Clear stat
//: \return:  n/a
//: \param:   ao_stat stat to be cleared
//: ----------------------------------------------------------------------------
inline void clear_stat(xstat_t &ao_stat)
{
        ao_stat.m_sum_x = 0.0;
        ao_stat.m_sum_x2 = 0.0;
        ao_stat.m_min = 0.0;
        ao_stat.m_max = 0.0;
        ao_stat.m_num = 0;
}

//: ----------------------------------------------------------------------------
//: \details: Show stat
//: \return:  n/a
//: \param:   ao_stat stat display
//: ----------------------------------------------------------------------------
inline void show_stat(const xstat_t &ao_stat)
{
        printf("Stat: Mean: %4.2f, StdDev: %4.2f, Min: %4.2f, Max: %4.2f Num: %lu\n",
               ao_stat.mean(),
               ao_stat.stdev(),
               ao_stat.m_min,
               ao_stat.m_max,
               ao_stat.m_num);
}

//: ----------------------------------------------------------------------------
//: Example...
//: ----------------------------------------------------------------------------
#if 0
int main(void)
{
    xstat_t l_s;
    update_stat(l_s, 30.0);
    update_stat(l_s, 15.0);
    update_stat(l_s, 22.0);
    update_stat(l_s, 10.0);
    update_stat(l_s, 24.0);
    show_stat(l_s);
    return 0;
}
#endif

} //namespace ns_hlx {

#endif


