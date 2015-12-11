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
#include "file.h"
#include "time_util.h"
#include "hconn.h"

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
h_resp_t rqst_h::get_file(hlx &a_hlx,
                          hconn &a_hconn,
                          rqst &a_rqst,
                          const std::string &a_path)
{
        if(!a_hconn.m_out_q)
        {
                // TODO 5xx's for errors?
                return send_not_found(a_hlx, a_hconn, a_rqst);
        }

        // Make relative...
        std::string l_path = "." + a_path;

        filesender *l_fs = new filesender();
        int l_status = l_fs->fsinit(l_path.c_str());
        if(l_status != STATUS_OK)
        {
                delete l_fs;
                // TODO 5xx's for errors?
                return send_not_found(a_hlx, a_hconn, a_rqst);
        }

        a_hconn.m_fs = l_fs;
        nbq &l_q = *(a_hconn.m_out_q);

        // ---------------------------------------
        // Write headers
        // ---------------------------------------
        nbq_write_status(l_q, HTTP_STATUS_OK);
        nbq_write_header(l_q, "Server", "hss/0.0.1");
        nbq_write_header(l_q, "Date", get_date_str());
        nbq_write_header(l_q, "Content-type", "text/html");
        char l_length_str[64];
        sprintf(l_length_str, "%lu", l_fs->fssize());
        nbq_write_header(l_q, "Content-Length", l_length_str);

        // TODO get last modified for file...
        nbq_write_header(l_q, "Last-Modified", get_date_str());
        if(a_rqst.m_supports_keep_alives)
        {
                nbq_write_header(l_q, "Connection", "keep-alive");
        }
        else
        {
                nbq_write_header(l_q, "Connection", "close");
        }
        l_q.write("\r\n", strlen("\r\n"));

        // Read up to 32k
        uint32_t l_read = 32678;
        if(l_fs->fssize() < l_read)
        {
                l_read = l_fs->fssize();
        }
        l_fs->fsread(l_q, l_read);

        a_hlx.queue_resp(a_hconn);
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
