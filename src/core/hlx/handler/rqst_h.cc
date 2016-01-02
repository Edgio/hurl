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
#include "nconn.h"
#include "ndebug.h"
#include "hconn.h"
#include "time_util.h"

#include <inttypes.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Custom responses
//: ----------------------------------------------------------------------------
const char *G_RESP_GETFILE_NOT_FOUND = "{ \"errors\": ["
                                       "{\"code\": 404, \"message\": \"Not found\"}"
                                       "], \"success\": false}\r\n";

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t rqst_h::send_not_found(hlx &a_hlx, hconn &a_hconn, const rqst &a_rqst)
{
        api_resp &l_api_resp = a_hlx.create_api_resp();
        l_api_resp.set_status(HTTP_STATUS_NOT_FOUND);
        l_api_resp.set_header("Server","hss/0.0.1");
        l_api_resp.set_header("Date", get_date_str());
        l_api_resp.set_header("Content-type", "application/json");
        char l_length_str[64];
        uint32_t l_resp_len = sizeof(char)*strlen(G_RESP_GETFILE_NOT_FOUND);
        sprintf(l_length_str, "%u", l_resp_len);
        l_api_resp.set_header("Content-Length", l_length_str);
        if(a_rqst.m_supports_keep_alives)
        {
                l_api_resp.set_header("Connection", "keep-alive");
        }
        else
        {
                l_api_resp.set_header("Connection", "close");
        }
        char *l_resp = (char *)malloc(l_resp_len);
        memcpy(l_resp, G_RESP_GETFILE_NOT_FOUND, l_resp_len);
        l_api_resp.set_body_data(l_resp, l_resp_len);
        a_hlx.queue_api_resp(a_hconn, l_api_resp);
        if(l_resp)
        {
                free(l_resp);
                l_resp = NULL;
        }
        return H_RESP_DONE;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
default_rqst_h::default_rqst_h(void)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
default_rqst_h::~default_rqst_h()
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t default_rqst_h::do_get(hlx &a_hlx, hconn &a_hconn,
                                rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        return send_not_found(a_hlx, a_hconn, a_rqst);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t default_rqst_h::do_post(hlx &a_hlx, hconn &a_hconn,
                                 rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        return send_not_found(a_hlx, a_hconn, a_rqst);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t default_rqst_h::do_put(hlx &a_hlx, hconn &a_hconn,
                                rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        return send_not_found(a_hlx, a_hconn, a_rqst);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t default_rqst_h::do_delete(hlx &a_hlx, hconn &a_hconn,
                                   rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        return send_not_found(a_hlx, a_hconn, a_rqst);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t default_rqst_h::do_default(hlx &a_hlx, hconn &a_hconn,
                                    rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        return send_not_found(a_hlx, a_hconn, a_rqst);
}

} //namespace ns_hlx {
