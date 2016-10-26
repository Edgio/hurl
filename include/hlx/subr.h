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
class ups_srvr_session;
class clnt_session;
class nconn;
class t_srvr;
class subr;
class resp;
class nbq;
class base_u;

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

        // ---------------------------------------
        // Callbacks
        // ---------------------------------------
        typedef int32_t (*error_cb_t)(subr &, nconn *, http_status_t, const char *);
        typedef int32_t (*completion_cb_t)(subr &, nconn &, resp &);

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        subr(void);
        subr(const subr &a_subr);
        ~subr();

        // Getters
        subr_state_t get_state() const;
        scheme_t get_scheme(void) const;
        bool get_save(void) const;
        bool get_connect_only(void) const;
        const std::string &get_host(void) const;
        const std::string &get_hostname(void) const;
        const std::string &get_id(void) const;
        const std::string &get_where(void) const;
        uint16_t get_port(void) const;
        bool get_keepalive(void) const;
        error_cb_t get_error_cb(void) const;
        completion_cb_t get_completion_cb(void) const;
        int32_t get_timeout_ms(void) const;
        void *get_data(void) const;
        bool get_detach_resp(void) const;
        uint64_t get_uid(void) const;
        ups_srvr_session *get_ups_srvr_session(void);
        clnt_session *get_clnt_session(void) const;
        const host_info &get_host_info(void) const;
        t_srvr *get_t_srvr(void) const;
        uint64_t get_start_time_ms(void) const;
        uint64_t get_end_time_ms(void) const;
        void * get_lookup_job(void);
        void set_i_q(subr_list_t::iterator a_i_q);
        bool get_tls_verify(void) const;
        bool get_tls_sni(void) const;
        bool get_tls_self_ok(void) const;
        bool get_tls_no_host_check(void) const;
        const std::string &get_label(void);
        base_u *get_ups(void);

        // Setters
        void set_state(subr_state_t a_state);
        void set_scheme(scheme_t a_scheme);
        void set_save(bool a_val);
        void set_connect_only(bool a_val);
        void set_num_to_request(int32_t a_val);
        void set_max_parallel(int32_t a_val);
        void set_keepalive(bool a_val);
        void set_error_cb(error_cb_t a_cb);
        void set_completion_cb(completion_cb_t a_cb);
        void set_timeout_ms(int32_t a_val);
        void set_host(const std::string &a_val);
        void set_hostname(const std::string &a_val);
        void set_id(const std::string &a_val);
        void set_where(const std::string &a_val);
        void set_port(uint16_t a_val);
        void set_data(void *a_data);
        void set_detach_resp(bool a_val);
        void set_uid(uint64_t a_uid);
        void set_ups_srvr_session(ups_srvr_session *a_ups_srvr_session);
        void set_clnt_session(clnt_session *a_clnt_session);
        void set_host_info(const host_info &a_host_info);
        void set_t_srvr(t_srvr *a_t_srvr);
        void set_start_time_ms(uint64_t a_val);
        void set_end_time_ms(uint64_t a_val);
        void set_lookup_job(void *a_data);
        void set_tls_verify(bool a_val);
        void set_tls_sni(bool a_val);
        void set_tls_self_ok(bool a_val);
        void set_tls_no_host_check(bool a_val);
        void set_ups(base_u *a_ups);

        // Request Parts
        // Getters
        const std::string &get_verb(void);
        bool get_expect_resp_body_flag(void);
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
        int set_header(const std::string &a_key, const std::string &a_val);
        int del_header(const std::string &a_key);
        void set_body_data(const char *a_ptr, uint32_t a_len);

        // Clear
        void clear_headers(void);
        void reset_label(void);

        // Initialize
        int32_t init_with_url(const std::string &a_url);

        // Create request
        int32_t create_request(nbq &ao_q);

        // Cancel
        int32_t cancel(void);

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
        scheme_t m_scheme;
        std::string m_host;
        uint16_t m_port;
        std::string m_server_label;
        bool m_save;
        bool m_connect_only;
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
        error_cb_t m_error_cb;
        completion_cb_t m_completion_cb;
        void *m_data;
        bool m_detach_resp;
        uint64_t m_uid;
        ups_srvr_session *m_ups_srvr_session;
        clnt_session *m_clnt_session;
        host_info m_host_info;
        t_srvr *m_t_srvr;
        uint64_t m_start_time_ms;
        uint64_t m_end_time_ms;
        void *m_lookup_job;
        subr_list_t::iterator m_i_q;
        bool m_tls_verify;
        bool m_tls_sni;
        bool m_tls_self_ok;
        bool m_tls_no_host_check;
        base_u *m_ups;
};

//: ----------------------------------------------------------------------------
//: hnconn_utils
//: ----------------------------------------------------------------------------
int32_t add_subr_t_srvr(void *a_t_srvr, subr &a_subr);

}

#endif


