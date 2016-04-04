//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
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

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: fwd decl's
//: ----------------------------------------------------------------------------
class hconn;
class nconn;
class rqst;
class resp;
class subr;

//: ----------------------------------------------------------------------------
//: file_h
//: ----------------------------------------------------------------------------
class proxy_h: public default_rqst_h
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        proxy_h(const std::string &a_ups_host, const std::string &a_route);
        ~proxy_h();
        h_resp_t do_default(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap);

        // Do default method override
        bool get_do_default(void);

        // -------------------------------------------------
        // Public Class methods
        // -------------------------------------------------
        static int32_t s_completion_cb(subr &a_subr, nconn &a_nconn, resp &a_resp);

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

};

} //namespace ns_hlx {

#endif
