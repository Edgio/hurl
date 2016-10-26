//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    rqst_h.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    08/29/2015
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

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/rqst_h.h"
#include "hlx/api_resp.h"
#include "hlx/srvr.h"
#include "hlx/clnt_session.h"

#include <string.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t rqst_h::send_json_resp(clnt_session &a_clnt_session,
                                bool a_keep_alive,
                                http_status_t a_status,
                                const char *a_json_resp)
{
        srvr *l_srvr = a_clnt_session.get_srvr();
        if(!l_srvr)
        {
                return H_RESP_SERVER_ERROR;
        }
        api_resp &l_api_resp = create_api_resp(a_clnt_session);
        l_api_resp.add_std_headers(a_status,
                                   "application/json",
                                   strlen(a_json_resp),
                                   a_keep_alive,
                                   *(l_srvr));
        l_api_resp.set_body_data(a_json_resp, strlen(a_json_resp));
        queue_api_resp(a_clnt_session, l_api_resp);
        return H_RESP_DONE;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t rqst_h::send_json_resp_err(clnt_session &a_clnt_session, bool a_keep_alive, http_status_t a_status)
{
        std::string l_resp;
        create_json_resp_str(a_status, l_resp);
        return send_json_resp(a_clnt_session, a_keep_alive, a_status, l_resp.c_str());
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t rqst_h::send_not_found(clnt_session &a_clnt_session, bool a_keep_alive)
{
        return send_json_resp_err(a_clnt_session, a_keep_alive, HTTP_STATUS_NOT_FOUND);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t rqst_h::send_not_implemented(clnt_session &a_clnt_session, bool a_keep_alive)
{
        return send_json_resp_err(a_clnt_session, a_keep_alive, HTTP_STATUS_NOT_IMPLEMENTED);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t rqst_h::send_internal_server_error(clnt_session &a_clnt_session, bool a_keep_alive)
{
        return send_json_resp_err(a_clnt_session, a_keep_alive, HTTP_STATUS_INTERNAL_SERVER_ERROR);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t rqst_h::send_bad_request(clnt_session &a_clnt_session, bool a_keep_alive)
{
        return send_json_resp_err(a_clnt_session, a_keep_alive, HTTP_STATUS_BAD_REQUEST);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t rqst_h::send_service_not_available(clnt_session &a_clnt_session, bool a_keep_alive)
{
        return send_json_resp_err(a_clnt_session, a_keep_alive, HTTP_STATUS_SERVICE_NOT_AVAILABLE);
}

} //namespace ns_hlx {
