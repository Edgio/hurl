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
#include "hlx/rqst.h"
#include "hlx/api_resp.h"
#include "hlx/hlx.h"

#include "file.h"
#include "hconn.h"
#include "nbq.h"
#include "time_util.h"
#include "string_util.h"
#include "mime_types.h"
#include "ndebug.h"

#include <string.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
file_h::file_h(void):
        default_rqst_h(),
        m_root(),
        m_index("index.html"),
        m_route()
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
h_resp_t file_h::do_get(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        // GET
        // Set path
        std::string l_path;
        int32_t l_s;
        l_s = get_path(m_root, m_index, m_route, a_rqst.get_url_path(), l_path);
        if(l_s != STATUS_OK)
        {
                return H_RESP_CLIENT_ERROR;
        }
        //NDBG_PRINT("l_path: %s\n",l_path.c_str());
        return get_file(a_hconn, a_rqst, l_path);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t file_h::set_root(const std::string &a_root)
{
        m_root = a_root;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t file_h::set_index(const std::string &a_index)
{
        m_index = a_index;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t file_h::set_route(const std::string &a_route)
{
        m_route = a_route;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t file_h::get_file(hconn &a_hconn,
                          rqst &a_rqst,
                          const std::string &a_path)
{
        if(!a_hconn.m_out_q)
        {
                // TODO 5xx's for errors?
                return send_not_found(a_hconn, a_rqst);
        }

        // Make relative...
        filesender *l_fs = new filesender();
        int l_status = l_fs->fsinit(a_path.c_str());
        if(l_status != STATUS_OK)
        {
                delete l_fs;
                // TODO 5xx's for errors?
                return send_not_found(a_hconn, a_rqst);
        }

        a_hconn.m_fs = l_fs;
        nbq &l_q = *(a_hconn.m_out_q);

        // ---------------------------------------
        // Write headers
        // ---------------------------------------
        if(!get_hlx(a_hconn))
        {
                return H_RESP_SERVER_ERROR;
        }
        nbq_write_status(l_q, HTTP_STATUS_OK);
        nbq_write_header(l_q, "Server", get_hlx(a_hconn)->get_server_name().c_str());
        nbq_write_header(l_q, "Date", get_date_str());

        // Get extension
        std::string l_ext = get_file_ext(a_path);
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
        queue_resp(a_hconn);
        return H_RESP_DONE;
}

} //namespace ns_hlx {
