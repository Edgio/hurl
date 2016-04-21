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
#include "hlx/http_status.h"
#include "hlx/proxy_h.h"
#include "hlx/subr.h"
#include "hlx/rqst.h"
#include "hlx/resp.h"
#include "hlx/hlx.h"
#include "ndebug.h"
#include "hconn.h"
#include "nbq.h"

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
        m_timeout_ms(600000)
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
h_resp_t proxy_h::do_default(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        ns_hlx::subr &l_subr = ns_hlx::create_subr(a_hconn);
        // Remove route from url
        std::string l_route = a_rqst.get_url();
        std::string::size_type i_s = l_route.find(m_route);
        if (i_s != std::string::npos)
        {
                l_route.erase(i_s, m_route.length());
        }
        std::string l_url = m_ups_host + l_route;
        l_subr.init_with_url(l_url);
        l_subr.set_completion_cb(s_completion_cb);
        l_subr.set_error_cb(s_error_cb);
        std::string l_host = l_subr.get_host();
        l_subr.set_data(this);
        l_subr.set_headers(a_rqst.get_headers());
        l_subr.set_keepalive(true);
        l_subr.set_timeout_ms(m_timeout_ms);
        l_subr.set_verb(a_rqst.get_method_str());
        const char *l_body_data = a_rqst.get_body_data();
        uint64_t l_body_data_len = a_rqst.get_body_len();
        l_subr.set_body_data(l_body_data, l_body_data_len);
        l_subr.set_requester_hconn(&a_hconn);
        int32_t l_s;
        l_s = queue_subr(a_hconn, l_subr);
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
int32_t proxy_h::s_completion_cb(subr &a_subr, nconn &a_nconn, resp &a_resp)
{
        // Create resp
        hconn *l_hconn = a_subr.get_requester_hconn();
        if(!l_hconn)
        {
                return STATUS_ERROR;
        }
        nbq *l_out_q = l_hconn->m_out_q;
        if(!l_out_q)
        {
                return STATUS_ERROR;
        }
        // TODO needed ???
        l_out_q->reset_write();
        nbq *l_in_q = a_resp.get_q();
        if(!l_in_q)
        {
                return STATUS_ERROR;
        }
        l_in_q->reset_read();
        int64_t l_size;
        l_size = l_out_q->write_q(*l_in_q);
        if(l_size < 0)
        {
                return STATUS_ERROR;
        }

        // access info
        l_hconn->m_access_info.m_resp_status = (http_status_t)a_resp.get_status();
        // TODO enum cast is bad -will fix

        int32_t l_s;
        l_s = queue_resp(*l_hconn);
        if(l_s != HLX_STATUS_OK)
        {
                return H_RESP_SERVER_ERROR;
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t proxy_h::s_error_cb(subr &a_subr, nconn &a_nconn)
{
        proxy_h *l_proxy_h = (proxy_h *)a_subr.get_data();
        if(!l_proxy_h)
        {
                return STATUS_ERROR;
        }
        if(!a_subr.get_requester_hconn())
        {
                return STATUS_ERROR;
        }
        l_proxy_h->send_json_resp_err(*a_subr.get_requester_hconn(),
                                      false,
                                      // TODO use supports keep-alives from client request
                                      a_subr.get_fallback_status_code());
        return STATUS_OK;
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

} //namespace ns_hlx {
