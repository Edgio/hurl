//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    cgi_h.cc
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
#include "nbq.h"
#include "string_util.h"
#include "ndebug.h"
#include "t_srvr.h"
#include "clnt_session.h"
#include "hlx/time_util.h"
#include "hlx/cgi_h.h"
#include "hlx/cgi_u.h"
#include "hlx/file_u.h"
#include "hlx/rqst.h"
#include "hlx/api_resp.h"
#include "hlx/srvr.h"
#include "hlx/status.h"
#include "hlx/trace.h"
#include <string.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
cgi_h::cgi_h(void):
        default_rqst_h(),
        m_root(),
        m_route(),
        m_timeout_ms(-1)
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
cgi_h::~cgi_h(void)
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t cgi_h::do_get(clnt_session &a_clnt_session, rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        // GET
        // Set path
        std::string l_path;
        int32_t l_s;
        std::string l_index;
        l_s = get_path(m_root, l_index, m_route, a_rqst.get_url_path(), l_path);
        if(l_s != HLX_STATUS_OK)
        {
                return H_RESP_CLIENT_ERROR;
        }
        TRC_VERBOSE("l_path: %s\n",l_path.c_str());
        return init_cgi(a_clnt_session, a_rqst, l_path);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void cgi_h::set_root(const std::string &a_root)
{
        m_root = a_root;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void cgi_h::set_route(const std::string &a_route)
{
        m_route = a_route;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void cgi_h::set_timeout_ms(int32_t a_val)
{
        m_timeout_ms = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t cgi_h::init_cgi(clnt_session &a_clnt_session,
                         rqst &a_rqst,
                         const std::string &a_path)
{
        cgi_u *l_cgi_u = new cgi_u(a_clnt_session, m_timeout_ms);
        l_cgi_u->m_supports_keep_alives = a_rqst.m_supports_keep_alives;
        int32_t l_s;
        l_s = l_cgi_u->cginit(a_path.c_str(), a_rqst.get_url_query_map());
        if(l_s != HLX_STATUS_OK)
        {
                delete l_cgi_u;
                // TODO -use status code to determine is actual 404
                return send_not_found(a_clnt_session, a_rqst.m_supports_keep_alives);
        }
        a_clnt_session.m_ups = l_cgi_u;
        return H_RESP_DONE;
}

} //namespace ns_hlx {
