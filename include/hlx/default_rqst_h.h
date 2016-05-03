//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    default_rqst_h.h
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
#ifndef _DEFAULT_RQST_H
#define _DEFAULT_RQST_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/rqst_h.h"

namespace ns_hlx {

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

        h_resp_t do_get(clnt_session &a_clnt_session, rqst &a_rqst, const url_pmap_t &a_url_pmap);
        h_resp_t do_post(clnt_session &a_clnt_session, rqst &a_rqst, const url_pmap_t &a_url_pmap);
        h_resp_t do_put(clnt_session &a_clnt_session, rqst &a_rqst, const url_pmap_t &a_url_pmap);
        h_resp_t do_delete(clnt_session &a_clnt_session, rqst &a_rqst, const url_pmap_t &a_url_pmap);
        h_resp_t do_default(clnt_session &a_clnt_session, rqst &a_rqst, const url_pmap_t &a_url_pmap);

        // Do default method override
        bool get_do_default(void);

private:
        // Disallow copy/assign
        default_rqst_h& operator=(const default_rqst_h &);
        default_rqst_h(const default_rqst_h &);
};

}

#endif


