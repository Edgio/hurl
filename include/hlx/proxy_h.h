//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    stat_h.h
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
#ifndef _PROXY_H_H
#define _PROXY_H_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/default_rqst_h.h"
#include "hlx/base_u.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: fwd decl's
//: ----------------------------------------------------------------------------
class clnt_session;
class nconn;
class rqst;
class resp;
class subr;
class nbq;

//: ----------------------------------------------------------------------------
//: Handler
//: ----------------------------------------------------------------------------
class proxy_h: public default_rqst_h
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        proxy_h(const std::string &a_ups_host, const std::string &a_route);
        ~proxy_h();
        h_resp_t do_default(clnt_session &a_clnt_session,
                            rqst &a_rqst,
                            const url_pmap_t &a_url_pmap);

        // Do default method override
        bool get_do_default(void);

        // Setters
        void set_timeout_ms(uint32_t a_val);
        void set_max_parallel(int32_t a_val);

protected:
        // -------------------------------------------------
        // Protected methods
        // -------------------------------------------------
        h_resp_t get_proxy(clnt_session &a_clnt_session,
                           rqst &a_rqst,
                           const std::string &a_url);
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        proxy_h& operator=(const proxy_h &);
        proxy_h(const proxy_h &);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        std::string m_ups_host;
        std::string m_route;
        uint32_t m_timeout_ms;
        int32_t m_max_parallel;

};

//: ----------------------------------------------------------------------------
//: Upstream Object
//: ----------------------------------------------------------------------------
class proxy_u: public base_u
{

public:
        // -------------------------------------------------
        // const
        // -------------------------------------------------
        static const uint32_t S_UPS_TYPE_PROXY = 0xFFFF000B;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        proxy_u(clnt_session &a_clnt_session, subr *a_subr, char *a_body_data, uint64_t a_body_len);
        ~proxy_u();

        // -------------------------------------------------
        // upstream methods
        // -------------------------------------------------
        ssize_t ups_read(size_t a_len);
        int32_t ups_cancel(void);
        uint32_t get_type(void) { return S_UPS_TYPE_PROXY;}

        // -------------------------------------------------
        // Public Class methods
        // -------------------------------------------------
        static int32_t s_completion_cb(subr &a_subr, nconn &a_nconn, resp &a_resp);
        static int32_t s_error_cb(subr &a_subr,
                                  nconn *a_nconn,
                                  http_status_t a_status,
                                  const char *a_error_str);
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        proxy_u& operator=(const proxy_u &);
        proxy_u(const proxy_u &);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        subr *m_subr;
        char *m_body_data;
        uint64_t m_body_data_len;
};

} //namespace ns_hlx {

#endif
