//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    cgi_h.h
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
#ifndef _CGI_H_H
#define _CGI_H_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/default_rqst_h.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: cgi_h
//: ----------------------------------------------------------------------------
class cgi_h: public default_rqst_h
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        cgi_h(void);
        ~cgi_h();
        h_resp_t do_get(clnt_session &a_clnt_session, rqst &a_rqst, const url_pmap_t &a_url_pmap);
        void set_root(const std::string &a_root);
        void set_route(const std::string &a_route);
        void set_timeout_ms(int32_t a_val);
protected:
        // -------------------------------------------------
        // Protected methods
        // -------------------------------------------------
        h_resp_t init_cgi(clnt_session &a_clnt_session, rqst &a_rqst, const std::string &a_path);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        cgi_h& operator=(const cgi_h &);
        cgi_h(const cgi_h &);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        std::string m_root;
        std::string m_route;
        int32_t m_timeout_ms;
};

} //namespace ns_hlx {

#endif
