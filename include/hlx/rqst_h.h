//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    rqst_h.h
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
#ifndef _RQST_H_H
#define _RQST_H_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/url_router.h"
#include "hlx/h_resp.h"
#include "hlx/http_status.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class clnt_session;
class rqst;

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

        // -------------------------------------------------
        // Public Virutal
        // -------------------------------------------------
        virtual h_resp_t do_get(clnt_session &a_clnt_session, rqst &a_rqst, const url_pmap_t &a_url_pmap) = 0;
        virtual h_resp_t do_post(clnt_session &a_clnt_session, rqst &a_rqst, const url_pmap_t &a_url_pmap) = 0;
        virtual h_resp_t do_put(clnt_session &a_clnt_session, rqst &a_rqst, const url_pmap_t &a_url_pmap) = 0;
        virtual h_resp_t do_delete(clnt_session &a_clnt_session, rqst &a_rqst, const url_pmap_t &a_url_pmap) = 0;
        virtual h_resp_t do_default(clnt_session &a_clnt_session, rqst &a_rqst, const url_pmap_t &a_url_pmap) = 0;

        // Do default method override
        virtual bool get_do_default(void) = 0;

        // -------------------------------------------------
        // Helpers
        // -------------------------------------------------
        static h_resp_t send_not_found(clnt_session &a_clnt_session, bool a_keep_alive);
        static h_resp_t send_not_implemented(clnt_session &a_clnt_session, bool a_keep_alive);
        static h_resp_t send_internal_server_error(clnt_session &a_clnt_session, bool a_keep_alive);
        static h_resp_t send_bad_request(clnt_session &a_clnt_session, bool a_keep_alive);
        static h_resp_t send_json_resp(clnt_session &a_clnt_session, bool a_keep_alive,
                                       http_status_t a_status, const char *a_json_resp);
        static h_resp_t send_json_resp_err(clnt_session &a_clnt_session, bool a_keep_alive,
                                           http_status_t a_status);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        rqst_h& operator=(const rqst_h &);
        rqst_h(const rqst_h &);
};

}

#endif



