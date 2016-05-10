//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    phurl_h.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    12/12/2015
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
#ifndef _PHURL_U_H
#define _PHURL_U_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/base_u.h"
#include "hlx/default_rqst_h.h"
#include "hlx/subr.h"
#include "hlx/scheme.h"

// For fixed size types
#include <stdint.h>
#include <map>
#include <string>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------
class phurl_h;
class clnt_session;
class nconn;
class resp;

//: ----------------------------------------------------------------------------
//: Host Struct
//: ----------------------------------------------------------------------------
struct host_s
{
        std::string m_host;
        uint16_t m_port;
        scheme_t m_scheme;
        host_s(std::string a_host, uint16_t a_port = 80):
               m_host(a_host),
               m_port(a_port),
               m_scheme(SCHEME_TCP)
        {};
};

//: ----------------------------------------------------------------------------
//: Single Resp Struct
//: ----------------------------------------------------------------------------
class hlx_resp
{
public:
        subr *m_subr;
        resp *m_resp;
        std::string m_error_str;
        uint16_t m_status_code;
        hlx_resp();
        ~hlx_resp();
private:
        // Disallow copy/assign
        hlx_resp& operator=(const hlx_resp &);
        hlx_resp(const hlx_resp &);
};

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::list <hlx_resp *> hlx_resp_list_t;
typedef std::map <uint64_t, subr *> subr_uid_map_t;
typedef std::list <struct host_s> host_list_t;

//: ----------------------------------------------------------------------------
//: fanout resp
//: ----------------------------------------------------------------------------
class phurl_u: public base_u
{
public:
        // -------------------------------------------------
        // const
        // -------------------------------------------------
        static const uint32_t S_UPS_TYPE_PHURL = 0xFFFF000C;

        // -------------------------------------------------
        // Types
        // -------------------------------------------------
        typedef int32_t (*create_resp_cb_t)(phurl_u *);

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        phurl_u(clnt_session &a_clnt_session,
                uint32_t a_timeout_ms=10000,
                float a_completion_ratio=100.0);
        ~phurl_u(void);

        // -------------------------------------------------
        // upstream methods
        // -------------------------------------------------
        ssize_t ups_read(size_t a_len);
        int32_t ups_cancel(void);
        uint32_t get_type(void) { return S_UPS_TYPE_PHURL;}

        float get_done_ratio(void);

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------
        static int32_t s_timeout_cb(void *a_ctx, void *a_data);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        subr_uid_map_t m_pending_subr_uid_map;
        hlx_resp_list_t m_resp_list;
        phurl_h *m_phurl_h;
        void *m_data;
        void *m_timer;
        uint64_t m_size;
        uint32_t m_timeout_ms;
        float m_completion_ratio;
        bool m_delete;
        create_resp_cb_t m_create_resp_cb;

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        phurl_u& operator=(const phurl_u &);
        phurl_u(const phurl_u &);

};

} //namespace ns_hlx {

#endif
