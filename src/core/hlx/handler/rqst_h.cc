//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
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
#include "hlx/hlx.h"
#include <string.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t rqst_h::send_json_resp(hconn &a_hconn, const rqst &a_rqst,
                                http_status_t a_status, const char *a_json_resp)
{
        hlx *l_hlx = get_hlx(a_hconn);
        if(!l_hlx)
        {
                return H_RESP_SERVER_ERROR;
        }
        api_resp &l_api_resp = create_api_resp(a_hconn);
        l_api_resp.add_std_headers(a_status, "application/json",
                                   strlen(a_json_resp), a_rqst,
                                   *(l_hlx));
        l_api_resp.set_body_data(a_json_resp, strlen(a_json_resp));
        queue_api_resp(a_hconn, l_api_resp);
        return H_RESP_DONE;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t rqst_h::send_json_resp_err(hconn &a_hconn, const rqst &a_rqst, http_status_t a_status)
{
        std::string l_resp;
        create_json_resp_str(a_status, l_resp);
        return send_json_resp(a_hconn, a_rqst, a_status, l_resp.c_str());
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t rqst_h::send_not_found(hconn &a_hconn, const rqst &a_rqst)
{
        return send_json_resp_err(a_hconn, a_rqst, HTTP_STATUS_NOT_FOUND);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t rqst_h::send_not_implemented(hconn &a_hconn, const rqst &a_rqst)
{
        return send_json_resp_err(a_hconn, a_rqst, HTTP_STATUS_NOT_IMPLEMENTED);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t rqst_h::send_internal_server_error(hconn &a_hconn, const rqst &a_rqst)
{
        return send_json_resp_err(a_hconn, a_rqst, HTTP_STATUS_INTERNAL_SERVER_ERROR);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t rqst_h::send_bad_request(hconn &a_hconn, const rqst &a_rqst)
{
        return send_json_resp_err(a_hconn, a_rqst, HTTP_STATUS_BAD_REQUEST);
}

} //namespace ns_hlx {
