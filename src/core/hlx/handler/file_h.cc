//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    file_h.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    01/02/2016
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
#include "hlx/file_h.h"
#include "file.h"
#include "hconn.h"
#include "nbq.h"
#include "time_util.h"
#include "string_util.h"
#include "mime_types.h"
#include "ndebug.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
file_h::file_h(void):
        default_rqst_h()
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
file_h::~file_h(void)
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t file_h::do_get(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        // GET
        return get_file(a_hlx, a_hconn, a_rqst, a_rqst.get_url_path());
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t file_h::get_file(hlx &a_hlx,
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

        // Get extension
        std::string l_ext = get_file_ext(l_path);
        mime_types::ext_type_map_t::const_iterator i_m = mime_types::S_EXT_TYPE_MAP.find(l_ext);
        if(i_m != mime_types::S_EXT_TYPE_MAP.end())
        {
                nbq_write_header(l_q, "Content-type", i_m->second.c_str());
        }
        else
        {
                nbq_write_header(l_q, "Content-type", "text/html");
        }

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

} //namespace ns_hlx {
