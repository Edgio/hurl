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
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>

// For sig_atomic_t
#include <signal.h>

// For strcasecmp
#include <string.h>

#include <string>
#include <map>
#include <list>
#include <vector>
#include <queue>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define HLX_STATUS_OK 0
#define HLX_STATUS_ERROR -1

//: ----------------------------------------------------------------------------
//: Extern Fwd Decl's
//: ----------------------------------------------------------------------------
struct ssl_ctx_st;
typedef ssl_ctx_st SSL_CTX;

struct ssl_st;
typedef struct ssl_st SSL;

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------
// Schemes
typedef enum scheme_enum {
        SCHEME_TCP = 0,
        SCHEME_TLS,
        SCHEME_NONE
} scheme_t;

// Status
typedef enum http_status_enum {

        HTTP_STATUS_CONTINUE                            = 100,
        HTTP_STATUS_SWITCHING_PROTOCOLS                 = 101,
        HTTP_STATUS_PROCESSING                          = 102, /* WEBDAV */

        HTTP_STATUS_OK                                  = 200,
        HTTP_STATUS_CREATED                             = 201,
        HTTP_STATUS_ACCEPTED                            = 202,
        HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION       = 203,
        HTTP_STATUS_NO_CONTENT                          = 204,
        HTTP_STATUS_RESET_CONTENT                       = 205,
        HTTP_STATUS_PARTIAL_CONTENT                     = 206,
        HTTP_STATUS_MULTI_STATUS                        = 207, /* WEBDAV */

        HTTP_STATUS_MULTIPLE_CHOICES                    = 300,
        HTTP_STATUS_MOVED_PERMANENTLY                   = 301,
        HTTP_STATUS_FOUND                               = 302,
        HTTP_STATUS_SEE_OTHER                           = 303,
        HTTP_STATUS_NOT_MODIFIED                        = 304,
        HTTP_STATUS_USE_PROXY                           = 305,
        HTTP_STATUS_UNUSED                              = 306,
        HTTP_STATUS_TEMPORARY_REDIRECT                  = 307,

        HTTP_STATUS_BAD_REQUEST                         = 400,
        HTTP_STATUS_UNAUTHORIZED                        = 401,
        HTTP_STATUS_PAYMENT_REQUIRED                    = 402,
        HTTP_STATUS_FORBIDDEN                           = 403,
        HTTP_STATUS_NOT_FOUND                           = 404,
        HTTP_STATUS_METHOD_NOT_ALLOWED                  = 405,
        HTTP_STATUS_NOT_ACCEPTABLE                      = 406,
        HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED       = 407,
        HTTP_STATUS_REQUEST_TIMEOUT                     = 408,
        HTTP_STATUS_CONFLICT                            = 409,
        HTTP_STATUS_GONE                                = 410,
        HTTP_STATUS_LENGTH_REQUIRED                     = 411,
        HTTP_STATUS_PRECONDITION_FAILED                 = 412,
        HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE            = 413,
        HTTP_STATUS_REQUEST_URI_TOO_LONG                = 414,
        HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE              = 415,
        HTTP_STATUS_REQUESTED_RANGE_NOT_SATISFIABLE     = 416,
        HTTP_STATUS_EXPECTATION_FAILED                  = 417,
        HTTP_STATUS_UNPROCESSABLE_ENTITY                = 422, /* WEBDAV */
        HTTP_STATUS_LOCKED                              = 423, /* WEBDAV */
        HTTP_STATUS_FAILED_DEPENDENCY                   = 424, /* WEBDAV */
        HTTP_STATUS_UPGRADE_REQUIRED                    = 426, /* TLS */

        HTTP_STATUS_INTERNAL_SERVER_ERROR               = 500,
        HTTP_STATUS_NOT_IMPLEMENTED                     = 501,
        HTTP_STATUS_BAD_GATEWAY                         = 502,
        HTTP_STATUS_SERVICE_NOT_AVAILABLE               = 503,
        HTTP_STATUS_GATEWAY_TIMEOUT                     = 504,
        HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED          = 505,
        HTTP_STATUS_INSUFFICIENT_STORAGE                = 507, /* WEBDAV */
        HTTP_STATUS_BANDWIDTH_LIMIT_EXCEEDED            = 509,

} http_status_t;

// ---------------------------------------
// Subreq types
// ---------------------------------------
typedef enum {

        SUBR_TYPE_NONE = 0,
        SUBR_TYPE_DUPE = 1

} subr_type_t;

// ---------------------------------------
// Handler status
// ---------------------------------------
typedef enum {

        H_RESP_NONE = 0,
        H_RESP_DONE,
        H_RESP_DEFERRED,
        H_RESP_SERVER_ERROR,
        H_RESP_CLIENT_ERROR

} h_resp_t;

// ---------------------------------------
// Connection status
// ---------------------------------------
typedef enum {

        CONN_STATUS_NONE                        =  1,
        CONN_STATUS_OK                          =  0,
        CONN_STATUS_ERROR_INTERNAL              = -1,   // generic internal failure
        CONN_STATUS_ERROR_ADDR_LOOKUP_FAILURE   = -1,   // failed to resolve, explicit
        CONN_STATUS_ERROR_ADDR_LOOKUP_TIMEOUT   = -2,   // failed to resolve, timed out
        CONN_STATUS_ERROR_CONNECT               = -3,   // resolved but failed to TCP connect, explicit
        // CONN_STATUS_ERROR_CONNECT_TIMEOUT       = -4,   // resolved but failed to TCP connect, timed out
        CONN_STATUS_ERROR_CONNECT_TLS           = -5,   // TCP connected but TLS error, generic
        CONN_STATUS_ERROR_CONNECT_TLS_HOST      = -6,   // TCP connected but TLS error, specific error for hostname validation failure because SSL makes us do it ourselves
        CONN_STATUS_ERROR_SEND                  = -7,   // connected but send error, explicit
        // CONN_STATUS_ERROR_SEND_TIMEOUT          = -8,   // connected but send error, timed out
        CONN_STATUS_ERROR_RECV                  = -9,   // connected, sent, error receiving, explicit
        // CONN_STATUS_ERROR_RECV_TIMEOUT          = -10,  // connected, sent, error receiving, timed out
        CONN_STATUS_ERROR_TIMEOUT               = -11,   // got a timeout waiting for something, generic.  TODO: deprecate after independent connect/send/recv timeout support

} conn_status_t;

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class nbq;
class url_router;
class t_hlx;
class nconn;
class hconn;
class t_conf;
class lsnr;
class subr;
class api_resp;
struct t_stat_struct;
typedef t_stat_struct t_stat_t;
struct host_info;
class nresolver;

//: ----------------------------------------------------------------------------
//: Structs
//: ----------------------------------------------------------------------------
// Offsets
typedef struct cr_struct
{
        uint64_t m_off;
        uint64_t m_len;
        cr_struct():
                m_off(0),
                m_len(0)
        {}
        void clear(void)
        {
                m_off = 0;
                m_len = 0;
        }
} cr_t;

//: ----------------------------------------------------------------------------
//: \details: Host info
//: ----------------------------------------------------------------------------
struct host_info {
        struct sockaddr_storage m_sa;
        int m_sa_len;
        int m_sock_family;
        int m_sock_type;
        int m_sock_protocol;
        uint64_t m_expires_s;

        host_info();
        void show(void);
};

//: ----------------------------------------------------------------------------
//: \details: atomic support -in lieu of C++11 support
//: \author:  Robert Peters
//: ----------------------------------------------------------------------------
template <class T>
class atomic_gcc_builtin
{
public:
        // see http://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Atomic-Builtins.html
        atomic_gcc_builtin() : v(0) {}
        atomic_gcc_builtin(T i) : v(i) {}
        atomic_gcc_builtin& operator=(T rhs)
        {
                (void) __sync_lock_test_and_set (&v, rhs);
                return *this;
        }
        operator T() const
        {
                __sync_synchronize();
                return v;
        }
#pragma GCC diagnostic push
        // ignored because inc/dec prefix operators should really return const refs
#pragma GCC diagnostic ignored "-Weffc++"
        T operator ++() { return __sync_add_and_fetch(&v, T(1)); }
        T operator --() { return __sync_sub_and_fetch(&v, T(1)); }
        // back to default behaviour
#pragma GCC diagnostic pop
        T operator +=(T i) { return __sync_add_and_fetch(&v, i); }
        T operator -=(T i) { return __sync_sub_and_fetch(&v, i); }
        bool exchange(T oldval, T newval)
        {
                return __sync_bool_compare_and_swap(&v, oldval, newval);
        }
        volatile T v;
};

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
struct case_i_comp {
    bool operator() (const std::string& lhs, const std::string& rhs) const {
        return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
    }
};

#ifndef URL_PARAM_MAP_T
#define URL_PARAM_MAP_T
typedef std::map <std::string, std::string> url_pmap_t;
#endif
typedef std::list <t_stat_t> t_stat_list_t;
typedef std::list <cr_t> cr_list_t;
typedef std::list <std::string> str_list_t;
typedef std::map <std::string, str_list_t, case_i_comp> kv_map_list_t;
typedef std::queue <subr *> subr_queue_t;
typedef atomic_gcc_builtin<uint64_t> uint64_atomic_t;
typedef std::map <std::string, std::string> query_map_t;

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
        typedef std::list <lsnr *> lsnr_list_t;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        hlx();
        ~hlx();

        // General
        void set_verbose(bool a_val);
        void set_color(bool a_val);
        void set_stats(bool a_val);

        // Settings
        void set_num_threads(uint32_t a_num_threads);
        void set_num_parallel(uint32_t a_num_parallel);
        void set_num_reqs_per_conn(int32_t a_num_reqs_per_conn);
        void set_start_time_ms(uint64_t a_start_time_ms);
        void set_collect_stats(bool a_val);
        void set_timeout_ms(uint32_t a_val);

        // Server name
        void set_server_name(const std::string &a_name);
        const std::string &get_server_name(void) const;

        // Socket options
        void set_sock_opt_no_delay(bool a_val);
        void set_sock_opt_send_buf_size(uint32_t a_send_buf_size);
        void set_sock_opt_recv_buf_size(uint32_t a_recv_buf_size);

        // Address info cache
        void set_use_ai_cache(bool a_val);
        void set_ai_cache(const std::string &a_ai_cache);

        // Listeners
        int32_t register_lsnr(lsnr *a_lsnr);

        // Subrequests
        uint64_t get_next_subr_uuid(void);

        // Helper for apps
        t_hlx_list_t &get_t_hlx_list(void);

        // Running...
        int32_t init_run(void); // init for running but don't start
        int32_t run(void);
        int32_t stop(void);
        int32_t wait_till_stopped(void);
        bool is_running(void);

        // Stats
        void get_stat(t_stat_t &ao_stat);
        void get_stat(const t_stat_list_t &a_stat_list, t_stat_t &ao_stat);
        void get_thread_stat(t_stat_list_t &ao_stat);
        void display_stat(void);

        // TLS config
        // Server ctx
        void set_tls_server_ctx_cipher_list(const std::string &a_cipher_list);
        int set_tls_server_ctx_options(const std::string &a_tls_options_str);
        int set_tls_server_ctx_options(long a_tls_options);
        void set_tls_server_ctx_key(const std::string &a_tls_key);
        void set_tls_server_ctx_crt(const std::string &a_tls_crt);

        // Client ctx
        void set_tls_client_ctx_cipher_list(const std::string &a_cipher_list);
        void set_tls_client_ctx_ca_path(const std::string &a_tls_ca_path);
        void set_tls_client_ctx_ca_file(const std::string &a_tls_ca_file);
        int set_tls_client_ctx_options(const std::string &a_tls_options_str);
        int set_tls_client_ctx_options(long a_tls_options);

        // Resolver
        nresolver *get_nresolver(void);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        hlx& operator=(const hlx &);
        hlx(const hlx &);

        int init(void);
        int init_t_hlx_list(void);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        t_conf *m_t_conf;
        bool m_stats;
        uint32_t m_num_threads;
        lsnr_list_t m_lsnr_list;
        nresolver *m_nresolver;
        bool m_use_ai_cache;
        std::string m_ai_cache;
        uint64_t m_start_time_ms;
        t_hlx_list_t m_t_hlx_list;
        bool m_is_initd;
        uint64_atomic_t m_cur_subr_uid;
        std::string m_server_name;
};

//: ----------------------------------------------------------------------------
//: \details: http message obj -abstraction of http reqeust / response
//: ----------------------------------------------------------------------------
class hmsg
{
public:
        // -------------------------------------------------
        // Public types
        // -------------------------------------------------
        // hobj type
        typedef enum type_enum {
                TYPE_NONE = 0,
                TYPE_RQST,
                TYPE_RESP
        } type_t;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        hmsg();
        virtual ~hmsg();

        // Getters
        type_t get_type(void) const;
        nbq *get_q(void) const;
        const char *get_body_data();
        uint64_t get_body_len() const;
        void get_headers(kv_map_list_t *ao_headers) const;
        const kv_map_list_t &get_headers();
        uint64_t get_idx(void) const;
        void set_idx(uint64_t a_idx);

        // Setters
        void set_q(nbq *a_q);

        virtual void clear(void);

        // Debug
        virtual void show() = 0;

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // ---------------------------------------
        // raw http request offsets
        // ---------------------------------------
        cr_list_t m_p_h_list_key;
        cr_list_t m_p_h_list_val;
        cr_t m_p_body;
        int m_http_major;
        int m_http_minor;

        // ---------------------------------------
        // ...
        // ---------------------------------------
        //uint16_t m_status;
        bool m_complete;
        bool m_supports_keep_alives;

protected:
        // -------------------------------------------------
        // Protected members
        // -------------------------------------------------
        type_t m_type;
        nbq *m_q;
        uint64_t m_idx;

        // populated on demand by getters
        kv_map_list_t *m_headers;
        char *m_body;
        uint64_t m_body_len;

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        hmsg(const hmsg &);
        hmsg& operator=(const hmsg &);

};

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class rqst : public hmsg
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        rqst();
        ~rqst();

        void clear(void);

        const std::string &get_url_path();
        const std::string &get_url_query();
        const query_map_t &get_url_query_map();
        const std::string &get_url_fragment();

        // Debug
        void show();

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------

        // ---------------------------------------
        // raw http request offsets
        // ---------------------------------------
        cr_t m_p_url;
        int m_method;

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        int32_t parse_uri(void);
        int32_t parse_query(const std::string &a_query, query_map_t &ao_query_map);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        bool m_url_parsed;
        std::string m_url;
        std::string m_url_path;
        std::string m_url_query;
        query_map_t m_url_query_map;
        std::string m_url_fragment;
};

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class resp : public hmsg
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        resp();
        ~resp();

        // Getters
        uint16_t get_status(void);

        // Setters
        void set_status(uint16_t a_code);

        void clear(void);

        // Debug
        void show();

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // ---------------------------------------
        // raw http request offsets
        // ---------------------------------------
        cr_t m_p_status;

        // TODO REMOVE
        const char *m_tls_info_protocol_str;
        const char *m_tls_info_cipher_str;

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        resp& operator=(const resp &);
        resp(const resp &);


        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        uint16_t m_status;
};

//: ----------------------------------------------------------------------------
//: rqst_h
//: ----------------------------------------------------------------------------
class rqst_h
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        rqst_h(void) {};
        virtual ~rqst_h(){};

        virtual h_resp_t do_get(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap) = 0;
        virtual h_resp_t do_post(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap) = 0;
        virtual h_resp_t do_put(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap) = 0;
        virtual h_resp_t do_delete(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap) = 0;
        virtual h_resp_t do_default(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap) = 0;

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        h_resp_t send_not_found(hconn &a_hconn, const rqst &a_rqst);
        h_resp_t send_not_implemented(hconn &a_hconn, const rqst &a_rqst);
        h_resp_t send_internal_server_error(hconn &a_hconn, const rqst &a_rqst);
        h_resp_t send_bad_request(hconn &a_hconn, const rqst &a_rqst);
        h_resp_t send_json_resp(hconn &a_hconn, const rqst &a_rqst,
                                http_status_t a_status, const char *a_json_resp);
        h_resp_t send_json_resp_err(hconn &a_hconn, const rqst &a_rqst,
                                    http_status_t a_status);

private:
        // Disallow copy/assign
        rqst_h& operator=(const rqst_h &);
        rqst_h(const rqst_h &);
};

//: ----------------------------------------------------------------------------
//: default_rqst_h
//: ----------------------------------------------------------------------------
class default_rqst_h: public rqst_h
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        default_rqst_h(void);
        ~default_rqst_h();

        h_resp_t do_get(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap);
        h_resp_t do_post(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap);
        h_resp_t do_put(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap);
        h_resp_t do_delete(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap);
        h_resp_t do_default(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap);
private:
        // Disallow copy/assign
        default_rqst_h& operator=(const default_rqst_h &);
        default_rqst_h(const default_rqst_h &);
};

//: ----------------------------------------------------------------------------
//: hlx
//: ----------------------------------------------------------------------------
class lsnr
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        lsnr(uint16_t a_port=12345, scheme_t a_scheme = SCHEME_TCP);
        ~lsnr();
        int32_t register_endpoint(const std::string &a_endpoint, const rqst_h *a_handler);
        int32_t get_fd(void) const { return m_fd;}
        scheme_t get_scheme(void) const { return m_scheme;}
        url_router *get_url_router(void) const { return m_url_router;}
        int32_t init(void);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        lsnr& operator=(const lsnr &);
        lsnr(const lsnr &);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        scheme_t m_scheme;
        uint16_t m_port;

        url_router *m_url_router;
        int32_t m_fd;
        bool m_is_initd;
};

//: ----------------------------------------------------------------------------
//: subr
//: ----------------------------------------------------------------------------
class subr
{
public:
        // -------------------------------------------------
        // Public types
        // -------------------------------------------------
        // ---------------------------------------
        // Callbacks
        // ---------------------------------------
        typedef int32_t (*error_cb_t)(subr &, nconn &);
        typedef int32_t (*completion_cb_t)(subr &, nconn &, resp &);
        typedef int32_t (*create_req_cb_t)(subr &, nbq &);

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        subr(void);
        subr(const subr &a_subr);
        ~subr();

        // Getters
        subr_type_t get_type() const;
        scheme_t get_scheme(void) const;
        bool get_save(void) const;
        bool get_connect_only(void) const;
        const std::string &get_host(void) const;
        const std::string &get_hostname(void) const;
        const std::string &get_id(void) const;
        const std::string &get_where(void) const;
        uint16_t get_port(void) const;
        int32_t get_num_to_request(void) const;
        uint32_t get_num_requested(void) const;
        uint32_t get_num_completed(void) const;
        bool get_keepalive(void) const;
        bool get_is_done(void) const;
        bool get_is_pending_done(void) const;
        bool get_is_multipath(void) const;
        error_cb_t get_error_cb(void) const;
        completion_cb_t get_completion_cb(void) const;
        create_req_cb_t get_create_req_cb(void) const;
        int32_t get_timeout_ms(void) const;
        void *get_data(void) const;
        bool get_detach_resp(void) const;
        uint64_t get_uid(void) const;
        hconn *get_requester_hconn(void) const;
        const host_info &get_host_info(void) const;
        t_hlx *get_t_hlx(void) const;
        uint64_t get_start_time_ms(void) const;
        uint64_t get_end_time_ms(void) const;
        bool get_tls_verify(void) const;
        bool get_tls_sni(void) const;
        bool get_tls_self_ok(void) const;
        bool get_tls_no_host_check(void) const;
        const std::string &get_label(void);

        // Setters
        void set_scheme(scheme_t a_scheme);
        void set_save(bool a_val);
        void set_connect_only(bool a_val);
        void set_is_multipath(bool a_val);
        void set_num_to_request(int32_t a_val);
        void set_keepalive(bool a_val);
        void set_error_cb(error_cb_t a_cb);
        void set_completion_cb(completion_cb_t a_cb);
        void set_create_req_cb(create_req_cb_t a_cb);
        void set_type(subr_type_t a_type);
        void set_timeout_ms(int32_t a_val);
        void set_host(const std::string &a_val);
        void set_hostname(const std::string &a_val);
        void set_id(const std::string &a_val);
        void set_where(const std::string &a_val);
        void set_port(uint16_t a_val);
        void set_data(void *a_data);
        void set_detach_resp(bool a_val);
        void set_uid(uint64_t a_uid);
        void set_requester_hconn(hconn *a_hconn);
        void set_host_info(const host_info &a_host_info);
        void set_t_hlx(t_hlx *a_t_hlx);
        void set_start_time_ms(uint64_t a_val);
        void set_end_time_ms(uint64_t a_val);
        void set_tls_verify(bool a_val);
        void set_tls_sni(bool a_val);
        void set_tls_self_ok(bool a_val);
        void set_tls_no_host_check(bool a_val);

        // Request Parts
        // Getters
        const std::string &get_verb(void);
        const std::string &get_path(void);
        const std::string &get_query(void);
        const std::string &get_fragment(void);
        const kv_map_list_t &get_headers(void);
        const char *get_body_data(void);
        uint32_t get_body_len(void);

        // Setters
        void set_verb(const std::string &a_verb);
        void set_query(const std::string &a_query);
        void set_path(const std::string &a_path);
        void set_fragment(const std::string &a_grament);
        void set_headers(const kv_map_list_t &a_headers_list);
        int set_header(const std::string &a_header);
        int set_header(const std::string &a_key, const std::string &a_val);
        void set_body_data(const char *a_ptr, uint32_t a_len);

        // Clear
        void clear_headers(void);
        void reset_label(void);

        // Stats
        void bump_num_requested(void);
        void bump_num_completed(void);

        // Initialize
        int32_t init_with_url(const std::string &a_url);

        // Cancel
        int32_t cancel(void);

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------
        static int32_t create_request(subr &a_subr, nbq &ao_q);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow assign
        subr& operator=(const subr &);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        subr_type_t m_type;
        scheme_t m_scheme;
        std::string m_host;
        uint16_t m_port;
        std::string m_server_label;
        bool m_save;
        bool m_connect_only;
        bool m_is_multipath;
        int32_t m_timeout_ms;
        std::string m_path;
        std::string m_query;
        std::string m_fragment;
        std::string m_userinfo;
        std::string m_hostname;
        std::string m_verb;
        bool m_keepalive;
        std::string m_id;
        std::string m_where;
        kv_map_list_t m_headers;
        const char *m_body_data;
        uint32_t m_body_data_len;
        int32_t m_num_to_request;
        uint32_t m_num_requested;
        uint32_t m_num_completed;
        error_cb_t m_error_cb;
        completion_cb_t m_completion_cb;
        create_req_cb_t m_create_req_cb;
        void *m_data;
        bool m_detach_resp;
        uint64_t m_uid;
        hconn *m_requester_hconn;
        host_info m_host_info;
        t_hlx *m_t_hlx;
        uint64_t m_start_time_ms;
        uint64_t m_end_time_ms;
        bool m_tls_verify;
        bool m_tls_sni;
        bool m_tls_self_ok;
        bool m_tls_no_host_check;
};

//: ----------------------------------------------------------------------------
//: api_resp
//: ----------------------------------------------------------------------------
class api_resp
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        api_resp(void);
        ~api_resp();

        // Request Parts
        // Getters

        // Setters
        void set_status(http_status_t a_status);
        int set_header(const std::string &a_header);
        int set_header(const std::string &a_key, const std::string &a_val);
        int set_headerf(const std::string &a_key, const char* fmt, ...) __attribute__((format(__printf__,3,4)));;
        void set_body_data(const char *a_ptr, uint32_t a_len);
        void add_std_headers(http_status_t a_status, const char *a_content_type,
                             uint64_t a_len, const rqst &a_rqst,
                             const hlx &a_hlx);
        // Clear
        void clear_headers(void);

        // Serialize to q for sending.
        int32_t serialize(nbq &ao_q);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        api_resp& operator=(const api_resp &);
        api_resp(const api_resp &);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        http_status_t m_status;
        kv_map_list_t m_headers;
        const char *m_body_data;
        uint32_t m_body_data_len;
};

//: ----------------------------------------------------------------------------
//: nbq_utils
//: ----------------------------------------------------------------------------
int32_t nbq_write_request_line(nbq &ao_q, const char *a_buf, uint32_t a_len);
int32_t nbq_write_status(nbq &ao_q, http_status_t a_status);
int32_t nbq_write_header(nbq &ao_q,
                         const char *a_key_buf, uint32_t a_key_len,
                         const char *a_val_buf, uint32_t a_val_len);
int32_t nbq_write_header(nbq &ao_q, const char *a_key_buf, const char *a_val_buf);
int32_t nbq_write_body(nbq &ao_q, const char *a_buf, uint32_t a_len);

//: ----------------------------------------------------------------------------
//: nconn_utils
//: ----------------------------------------------------------------------------
int nconn_get_fd(nconn &a_nconn);
SSL *nconn_get_SSL(nconn &a_nconn);
long nconn_get_last_SSL_err(nconn &a_nconn);
conn_status_t nconn_get_status(nconn &a_nconn);
const std::string &nconn_get_last_error_str(nconn &a_nconn);

//: ----------------------------------------------------------------------------
//: hnconn_utils
//: ----------------------------------------------------------------------------
// Subrequests
subr &create_subr(hconn &a_hconn);
subr &create_subr(hconn &a_hconn, const subr &a_subr);
int32_t queue_subr(hconn &a_hconn, subr &a_subr);
int32_t add_subr_t_hlx(void *a_t_hlx, subr &a_subr);

// API Responses
api_resp &create_api_resp(hconn &a_hconn);
int32_t queue_api_resp(hconn &a_hconn, api_resp &a_api_resp);
int32_t queue_resp(hconn &a_hconn);
void create_json_resp_str(http_status_t a_status, std::string &ao_resp_str);

// Timer
typedef int32_t (*timer_cb_t)(void *);
int32_t add_timer(hconn &a_hconn, uint32_t a_ms,
                  timer_cb_t a_cb, void *a_data,
                  void **ao_timer);
int32_t cancel_timer(hconn &a_hconn, void *a_timer);

// Helper
hlx *get_hlx(hconn &a_hconn);

} //namespace ns_hlx {

#endif
