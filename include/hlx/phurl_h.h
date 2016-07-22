//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
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
#ifndef _PHURL_H_H
#define _PHURL_H_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/default_rqst_h.h"
#include "hlx/subr.h"
#include "hlx/scheme.h"
#include "hlx/phurl_u.h"

// For fixed size types
#include <stdint.h>
#include <map>
#include <string>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------
class clnt_session;
class nconn;
class resp;

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::list <struct host_s> host_list_t;

//: ----------------------------------------------------------------------------
//: phurl_h
//: ----------------------------------------------------------------------------
class phurl_h: public default_rqst_h
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        phurl_h(void);
        ~phurl_h();

        h_resp_t do_default(clnt_session &a_clnt_session, rqst &a_rqst, const url_pmap_t &a_url_pmap);

        // Do default method override
        bool get_do_default(void);

        void add_host(const std::string a_host, uint16_t a_port = 80);
        void set_host_list(const host_list_t &a_host_list);
        const subr &get_subr_template(void) const;
        subr &get_subr_template_mutable(void);

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------
        static int32_t s_completion_cb(subr &a_subr, nconn &a_nconn, resp &a_resp);
        static int32_t s_error_cb(subr &a_subr,
                                  nconn *a_nconn,
                                  http_status_t a_status,
                                  const char *a_error_str);
        static int32_t s_done_check(subr &a_subr, phurl_u *a_phr);
        static int32_t s_create_resp(phurl_u *a_phr);

protected:
        // -------------------------------------------------
        // Protected members
        // -------------------------------------------------
        subr m_subr_template;
        host_list_t m_host_list;
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        phurl_h& operator=(const phurl_h &);
        phurl_h(const phurl_h &);
};

} //namespace ns_hlx {

#endif
