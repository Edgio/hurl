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
        HTTP_STATUS_OK = 200,
        HTTP_STATUS_NOT_FOUND = 404,
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
        H_RESP_ERROR
} h_resp_t;

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class nbq;
class url_router;
class t_hlx;
class resolver;
class hlx;
class nconn;
class hconn;
class rqst;
class resp;
class t_conf;
class lsnr;
class subr;
class api_resp;
struct t_stat_struct;
typedef t_stat_struct t_stat_t;

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
#ifndef URL_PARAM_MAP_T
#define URL_PARAM_MAP_T
typedef std::map <std::string, std::string> url_pmap_t;
#endif
typedef std::list <cr_t> cr_list_t;
typedef std::list <std::string> str_list_t;
typedef std::map <std::string, str_list_t> kv_map_list_t;
typedef std::queue <subr *> subr_queue_t;
typedef atomic_gcc_builtin<uint64_t> uint64_atomic_t;

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
        void set_use_persistent_pool(bool a_val);

        // Socket options
        void set_sock_opt_no_delay(bool a_val);
        void set_sock_opt_send_buf_size(uint32_t a_send_buf_size);
        void set_sock_opt_recv_buf_size(uint32_t a_recv_buf_size);

        // Address info cache
        void set_use_ai_cache(bool a_val);
        void set_ai_cache(const std::string &a_ai_cache);

        // Listeners
        int32_t add_lsnr(lsnr *a_lsnr);

        // Subrequests
        subr &create_subr();
        subr &create_subr(const subr &a_subr);
        int32_t queue_subr(hconn *a_hconn, subr &a_subr);

        // API Responses
        api_resp &create_api_resp(void);
        int32_t queue_api_resp(hconn &a_hconn, api_resp &a_api_resp);

        // Running...
        int32_t run(void);
        int32_t stop(void);
        int32_t wait_till_stopped(void);
        bool is_running(void);

        // Stats
        void get_stats(t_stat_t &ao_all_stats);
        int32_t get_stats_json(char *l_json_buf, uint32_t l_json_buf_max_len);

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
        void add_subr_t(subr &a_subr);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        t_conf *m_t_conf;

        bool m_stats;
        uint32_t m_num_threads;
        lsnr_list_t m_lsnr_list;
        subr_queue_t m_subr_queue;

        // Resolver settings
        bool m_use_ai_cache;
        std::string m_ai_cache;
        uint64_t m_start_time_ms;
        t_hlx_list_t m_t_hlx_list;
        t_hlx_list_t::iterator m_t_hlx_subr_iter;
        bool m_is_initd;

        // subr id's
        uint64_atomic_t m_cur_subr_uid;

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
        type_t get_type(void);
        nbq *get_q(void);
        char *get_body_allocd(char **ao_buf, uint64_t &ao_len);
        kv_map_list_t *get_headers_allocd();

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

        const std::string &get_path();

        // Debug
        void show();

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        std::string m_url;

        // ---------------------------------------
        // raw http request offsets
        // ---------------------------------------
        cr_t m_p_url;
        int m_method;

private:
        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        std::string m_path;
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

        virtual h_resp_t do_get(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap) = 0;
        virtual h_resp_t do_post(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap) = 0;
        virtual h_resp_t do_put(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap) = 0;
        virtual h_resp_t do_delete(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap) = 0;
        virtual h_resp_t do_default(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap) = 0;

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        h_resp_t get_file(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const std::string &a_path);
        h_resp_t send_not_found(hlx &a_hlx, hconn &a_hconn, const rqst &a_rqst);

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

        h_resp_t do_get(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap);
        h_resp_t do_post(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap);
        h_resp_t do_put(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap);
        h_resp_t do_delete(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap);
        h_resp_t do_default(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap);
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
        int32_t add_endpoint(const std::string &a_endpoint, const rqst_h *a_handler);
        int32_t get_fd(void) const { return m_fd;}
        scheme_t get_scheme(void) const { return m_scheme;}
        url_router *get_url_router(void) const { return m_url_router;}
        int32_t init(void);

private:
        // Disallow copy/assign
        lsnr& operator=(const lsnr &);
        lsnr(const lsnr &);

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------

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
        typedef int32_t (*error_cb_t)(hlx &, subr &, nconn &);
        typedef int32_t (*completion_cb_t)(hlx &, subr &, nconn &, resp &);
        typedef int32_t (*create_req_cb_t)(hlx &, subr &, nbq &);

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        subr(void);
        subr(const subr &a_subr);
        ~subr();

        // Getters
        subr_type_t get_type();
        const std::string &get_label(void);
        scheme_t get_scheme(void);
        bool get_save(void);
        bool get_connect_only(void);
        const std::string &get_host(void);
        const std::string &get_hostname(void);
        const std::string &get_id(void);
        const std::string &get_where(void);
        uint16_t get_port(void);
        int32_t get_num_to_request(void);
        uint32_t get_num_requested(void);
        uint32_t get_num_completed(void);
        bool get_keepalive(void);
        bool get_is_done(void);
        bool get_is_pending_done(void);
        bool get_is_multipath(void);
        error_cb_t get_error_cb(void);
        completion_cb_t get_completion_cb(void);
        create_req_cb_t get_create_req_cb(void);
        int32_t get_timeout_s(void);
        void *get_data(void);
        bool get_detach_resp(void);
        uint64_t get_uid(void);
        hconn *get_requester_hconn(void);

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
        void set_timeout_s(int32_t a_val);
        void set_host(std::string a_val);
        void set_hostname(std::string a_val);
        void set_id(std::string a_val);
        void set_where(std::string a_val);
        void set_port(uint16_t a_val);
        void set_data(void *a_data);
        void set_detach_resp(bool a_val);
        void set_uid(uint64_t a_uid);
        void set_requester_hconn(hconn *a_hconn);

        // Request Parts
        // Getters
        const std::string &get_query(void);
        const std::string &get_path(void);
        const std::string &get_verb(void);
        const kv_map_list_t &get_headers(void);
        const char *get_body_data(void);
        uint32_t get_body_len(void);

        // Setters
        void set_body_data(const char *a_ptr, uint32_t a_len);
        void set_verb(const std::string &a_verb);
        int set_header(const std::string &a_header);
        int set_header(const std::string &a_key, const std::string &a_val);

        // Clear
        void clear_headers(void);
        void reset_label(void);

        // Stats
        void bump_num_requested(void);
        void bump_num_completed(void);

        // Initialize
        int32_t init_with_url(const std::string &a_url);

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------
        static int32_t create_request(hlx &a_hlx, subr &a_subr, nbq &ao_q);

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
        int32_t m_timeout_s;
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
};

//: ----------------------------------------------------------------------------
//: subr
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
        void set_body_data(const char *a_ptr, uint32_t a_len);

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
int32_t nbq_write_header(nbq &ao_q, const char *a_key_buf, uint32_t a_key_len, const char *a_val_buf, uint32_t a_val_len);
int32_t nbq_write_body(nbq &ao_q, const char *a_buf, uint32_t a_len);

//: ----------------------------------------------------------------------------
//: nconn_utils
//: ----------------------------------------------------------------------------
int nconn_get_fd(nconn &a_nconn);
SSL *nconn_get_SSL(nconn &a_nconn);

} //namespace ns_hlx {

#endif


