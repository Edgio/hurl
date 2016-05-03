//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    stat_h.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    01/19/2016
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
#include "ndebug.h"
#include "nbq.h"
#include "clnt_session.h"
#include "hlx/http_status.h"
#include "hlx/proxy_h.h"
#include "hlx/proxy_u.h"
#include "hlx/subr.h"
#include "hlx/rqst.h"
#include "hlx/resp.h"
#include "hlx/srvr.h"
#include "hlx/status.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
proxy_h::proxy_h(const std::string &a_ups_host, const std::string &a_route):
        default_rqst_h(),
        m_ups_host(a_ups_host),
        m_route(a_route),
        // 10 min
        m_timeout_ms(600000),
        m_max_parallel(-1)
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
proxy_h::~proxy_h(void)
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t proxy_h::do_default(clnt_session &a_clnt_session, rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        // Remove route from url
        std::string l_route = a_rqst.get_url();
        std::string::size_type i_s = l_route.find(m_route);
        if (i_s != std::string::npos)
        {
                l_route.erase(i_s, m_route.length());
        }
        std::string l_url = m_ups_host + l_route;

        //NDBG_PRINT("l_path: %s\n",l_path.c_str());
        return get_proxy(a_clnt_session, a_rqst, l_url);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t proxy_h::get_proxy(clnt_session &a_clnt_session,
                            rqst &a_rqst,
                            const std::string &a_url)
{
        // subr setup
        ns_hlx::subr &l_subr = ns_hlx::create_subr(a_clnt_session);
        l_subr.init_with_url(a_url);
        l_subr.set_completion_cb(proxy_u::s_completion_cb);
        l_subr.set_error_cb(proxy_u::s_error_cb);
        l_subr.set_data(this);
        l_subr.set_headers(a_rqst.get_headers());
        l_subr.set_keepalive(true);
        l_subr.set_timeout_ms(m_timeout_ms);
        l_subr.set_max_parallel(m_max_parallel);
        l_subr.set_verb(a_rqst.get_method_str());
        const char *l_body_data = a_rqst.get_body_data();
        uint64_t l_body_data_len = a_rqst.get_body_len();
        l_subr.set_body_data(l_body_data, l_body_data_len);
        l_subr.set_clnt_session(&a_clnt_session);

        proxy_u *l_px = new proxy_u();
        l_px->set_subr(&l_subr);
        l_subr.set_ups(l_px);
        a_clnt_session.m_ups = l_px;
        int32_t l_s;
        l_s = queue_subr(a_clnt_session, l_subr);
        if(l_s != HLX_STATUS_OK)
        {
                return ns_hlx::H_RESP_SERVER_ERROR;
        }
        return ns_hlx::H_RESP_DONE;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool proxy_h::get_do_default(void)
{
        return true;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void proxy_h::set_timeout_ms(uint32_t a_val)
{
        m_timeout_ms = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void proxy_h::set_max_parallel(int32_t a_val)
{
        m_max_parallel = a_val;
}

} //namespace ns_hlx {
