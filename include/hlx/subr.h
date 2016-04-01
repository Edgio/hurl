//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    subr.h
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
#ifndef _SUBR_H
#define _SUBR_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/http_status.h"
#include "hlx/scheme.h"
#include "hlx/kv_map_list.h"
#include "hlx/host_info.h"

// For fixed size types
#include <stdint.h>
#include <string>
#include <list>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class url_router;
class hconn;
class nconn;
class t_hlx;
class subr;
class resp;
class nbq;

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::list <subr *> subr_list_t;

//: ----------------------------------------------------------------------------
//: subr
//: ----------------------------------------------------------------------------
class subr
{
public:
        // -------------------------------------------------
        // Public types
        // -------------------------------------------------
        // state
        typedef enum {
                SUBR_STATE_NONE = 0,
                SUBR_STATE_QUEUED,
                SUBR_STATE_DNS_LOOKUP,
                SUBR_STATE_ACTIVE
        } subr_state_t;

        // kind
        typedef enum {
                SUBR_KIND_NONE = 0,
                SUBR_KIND_DUPE
        } subr_kind_t;

        // ---------------------------------------
        // Callbacks
        // ---------------------------------------
        typedef int32_t (*error_cb_t)(subr &, nconn &);
        typedef int32_t (*completion_cb_t)(subr &, nconn &, resp &);
        typedef int32_t (*create_req_cb_t)(subr &, nbq &);
        typedef int32_t (*pre_connect_cb_t)(int);

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        subr(void);
        subr(const subr &a_subr);
        ~subr();

        // Getters
        subr_state_t get_state() const;
        subr_kind_t get_kind() const;
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
        hconn *get_hconn(void);
        hconn *get_requester_hconn(void) const;
        const host_info &get_host_info(void) const;
        t_hlx *get_t_hlx(void) const;
        uint64_t get_start_time_ms(void) const;
        uint64_t get_end_time_ms(void) const;
        void * get_lookup_job(void);
        bool get_tls_verify(void) const;
        bool get_tls_sni(void) const;
        bool get_tls_self_ok(void) const;
        bool get_tls_no_host_check(void) const;
        const std::string &get_label(void);
        http_status_t get_fallback_status_code(void);
        pre_connect_cb_t get_pre_connect_cb(void) const;

        // Setters
        void set_state(subr_state_t a_state);
        void set_kind(subr_kind_t a_kind);
        void set_scheme(scheme_t a_scheme);
        void set_save(bool a_val);
        void set_connect_only(bool a_val);
        void set_is_multipath(bool a_val);
        void set_num_to_request(int32_t a_val);
        void set_keepalive(bool a_val);
        void set_error_cb(error_cb_t a_cb);
        void set_completion_cb(completion_cb_t a_cb);
        void set_create_req_cb(create_req_cb_t a_cb);
        void set_timeout_ms(int32_t a_val);
        void set_host(const std::string &a_val);
        void set_hostname(const std::string &a_val);
        void set_id(const std::string &a_val);
        void set_where(const std::string &a_val);
        void set_port(uint16_t a_val);
        void set_data(void *a_data);
        void set_detach_resp(bool a_val);
        void set_uid(uint64_t a_uid);
        void set_hconn(hconn *a_hconn);
        void set_requester_hconn(hconn *a_hconn);
        void set_host_info(const host_info &a_host_info);
        void set_t_hlx(t_hlx *a_t_hlx);
        void set_start_time_ms(uint64_t a_val);
        void set_end_time_ms(uint64_t a_val);
        void set_lookup_job(void *a_data);
        void set_i_q(subr_list_t::iterator a_i_q);
        void set_tls_verify(bool a_val);
        void set_tls_sni(bool a_val);
        void set_tls_self_ok(bool a_val);
        void set_tls_no_host_check(bool a_val);
        void set_fallback_status_code(http_status_t a_status);
        void set_pre_connect_cb(pre_connect_cb_t a_cb);

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
        int del_header(const std::string &a_key);
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
        subr_state_t m_state;
        subr_kind_t m_kind;
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
        hconn *m_hconn;
        hconn *m_requester_hconn;
        host_info m_host_info;
        t_hlx *m_t_hlx;
        uint64_t m_start_time_ms;
        uint64_t m_end_time_ms;
        void *m_lookup_job;
        subr_list_t::iterator m_i_q;
        bool m_tls_verify;
        bool m_tls_sni;
        bool m_tls_self_ok;
        bool m_tls_no_host_check;
        http_status_t m_fallback_status_code;
        pre_connect_cb_t m_pre_connect_cb;

};

//: ----------------------------------------------------------------------------
//: hnconn_utils
//: ----------------------------------------------------------------------------
// Subrequests
subr &create_subr(hconn &a_hconn);
subr &create_subr(hconn &a_hconn, const subr &a_subr);
int32_t queue_subr(hconn &a_hconn, subr &a_subr);
int32_t add_subr_t_hlx(void *a_t_hlx, subr &a_subr);
uint16_t get_status_code(subr &a_subr);

}

#endif


