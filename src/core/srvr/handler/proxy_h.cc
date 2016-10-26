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
#include "t_srvr.h"
#include "ndebug.h"
#include "hlx/nbq.h"
#include "hlx/clnt_session.h"
#include "hlx/ups_srvr_session.h"
#include "hlx/http_status.h"
#include "hlx/proxy_h.h"
#include "hlx/subr.h"
#include "hlx/rqst.h"
#include "hlx/resp.h"
#include "hlx/srvr.h"
#include "hlx/status.h"
#include "hlx/trace.h"
#include <stdlib.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Handler
//: ----------------------------------------------------------------------------
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
        if(l_route.empty())
        {
                return H_RESP_SERVER_ERROR;
        }
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
        subr *l_subr = a_clnt_session.create_subr(NULL);
        if(!l_subr)
        {
                TRC_ERROR("performing create_subr\n");
                return H_RESP_SERVER_ERROR;
        }
        l_subr->init_with_url(a_url);
        l_subr->set_completion_cb(proxy_u::s_completion_cb);
        l_subr->set_error_cb(proxy_u::s_error_cb);
        l_subr->set_data(this);
        l_subr->set_headers(a_rqst.get_headers());
        l_subr->set_keepalive(true);
        l_subr->set_timeout_ms(m_timeout_ms);
        l_subr->set_verb(a_rqst.get_method_str());
        char *l_body_data = NULL;
        uint64_t l_body_data_len = 0;
        if(!a_rqst.get_body_data_copy(&l_body_data, l_body_data_len))
        {
                delete l_subr;
                l_subr = NULL;
                return H_RESP_SERVER_ERROR;
        }
        l_subr->set_body_data(l_body_data, l_body_data_len);
        l_subr->set_clnt_session(&a_clnt_session);
        proxy_u *l_px = new proxy_u(a_clnt_session, l_subr, l_body_data, l_body_data_len);
        l_subr->set_ups(l_px);
        a_clnt_session.m_ups = l_px;
        int32_t l_s;
        l_s = a_clnt_session.queue_subr(*l_subr);
        if(l_s != HLX_STATUS_OK)
        {
                delete l_subr;
                l_subr = NULL;
                return H_RESP_SERVER_ERROR;
        }
        return H_RESP_DONE;
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

//: ----------------------------------------------------------------------------
//: Upstream Object
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
proxy_u::proxy_u(clnt_session &a_clnt_session,
                 subr *a_subr,
                 char *a_body_data,
                 uint64_t a_body_len):
        base_u(a_clnt_session),
        m_subr(a_subr),
        m_body_data(a_body_data),
        m_body_data_len(a_body_len)
{
        //NDBG_PRINT("%sCONSTRUCT%s\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
proxy_u::~proxy_u(void)
{
        //NDBG_PRINT("%sDELETE%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
        if(m_subr)
        {
                if(m_subr->get_state() == subr::SUBR_STATE_QUEUED)
                {
                        m_subr->cancel();
                }
                delete m_subr;
                m_subr = NULL;
        }
        // TODO move body data ownership into subr???
        if(m_body_data)
        {
                free(m_body_data);
                m_body_data = NULL;
                m_body_data_len = 0;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ssize_t proxy_u::ups_read(size_t a_len)
{
        if(ups_done())
        {
                return HLX_STATUS_OK;
        }
        if(a_len <= 0)
        {
                return HLX_STATUS_OK;
        }
        if(!m_subr)
        {
                TRC_ERROR("m_subr == NULL\n");
                return HLX_STATUS_ERROR;
        }
        ups_srvr_session *l_ups_srvr_session = m_subr->get_ups_srvr_session();
        if(!l_ups_srvr_session || !l_ups_srvr_session->m_in_q)
        {
                TRC_ERROR("requester ups_srvr_session or m_in_q == NULL\n");
                return HLX_STATUS_ERROR;
        }
        if(m_state == UPS_STATE_IDLE)
        {
                // first time
                if(m_clnt_session.m_out_q)
                {
                        // Return to pool
                        if(!m_clnt_session.m_t_srvr)
                        {
                                TRC_ERROR("m_clnt_session.m_t_srvr == NULL\n");
                                return HLX_STATUS_ERROR;
                        }
                        m_clnt_session.m_t_srvr->release_nbq(m_clnt_session.m_out_q);
                        m_clnt_session.m_out_q = NULL;
                }
                m_clnt_session.m_out_q = l_ups_srvr_session->m_in_q;
                l_ups_srvr_session->m_in_q_detached = true;
        }
        m_state = UPS_STATE_SENDING;
        if(!m_clnt_session.m_out_q)
        {
                TRC_ERROR("m_clnt_session.m_out_q == NULL\n");
                return HLX_STATUS_ERROR;
        }
        //NDBG_PRINT("l_ups_srvr_session->m_in_q->b_read_avail(): = %d\n", l_ups_srvr_session->m_in_q->b_read_avail());
        //NDBG_PRINT("m_clnt_session.m_out_q->b_read_avail():     = %d\n", m_clnt_session.m_out_q->b_read_avail());
        //NDBG_PRINT("l_ups_srvr_session->m_resp->m_complete:     = %d\n", l_ups_srvr_session->m_resp->m_complete);
        if(l_ups_srvr_session->m_resp->m_complete)
        {
                m_state = UPS_STATE_DONE;
        }
        int32_t l_s;
        l_s = m_clnt_session.m_t_srvr->queue_output(m_clnt_session);
        if(l_s != HLX_STATUS_OK)
        {
                TRC_ERROR("performing queue_output\n");
                return HLX_STATUS_ERROR;
        }
        return a_len;
}

//: ----------------------------------------------------------------------------
//: \details: Cancel and cleanup
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t proxy_u::ups_cancel(void)
{
        if(ups_done())
        {
                return HLX_STATUS_OK;
        }
        if(!m_subr)
        {
                TRC_ERROR("m_subr == NULL\n");
                return HLX_STATUS_ERROR;
        }
        m_state = UPS_STATE_DONE;
        //NDBG_PRINT("%sUPS_CANCEL%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
        // cancel subrequest
        m_subr->set_error_cb(NULL);
        int32_t l_s;
        l_s = m_subr->cancel();
        if(l_s != HLX_STATUS_OK)
        {
                TRC_ERROR("performing subr cancel.\n");
        }
        delete m_subr;
        m_subr = NULL;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t proxy_u::s_completion_cb(subr &a_subr, nconn &a_nconn, resp &a_resp)
{
        //NDBG_PRINT("%ss_completion_cb%s\n", ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF);
        clnt_session *l_cs = a_subr.get_clnt_session();
        if(!l_cs)
        {
                return HLX_STATUS_ERROR;
        }
        // access info
        l_cs->m_access_info.m_resp_status = (http_status_t)a_resp.get_status();
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t proxy_u::s_error_cb(subr &a_subr,
                            nconn *a_nconn,
                            http_status_t a_status,
                            const char *a_error_str)
{
        //NDBG_PRINT("%ss_error_cb%s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF);
        if(!a_subr.get_clnt_session())
        {
                return HLX_STATUS_ERROR;
        }
        clnt_session *l_cs = a_subr.get_clnt_session();
        if(!l_cs)
        {
                return HLX_STATUS_ERROR;
        }
        if(l_cs->m_ups)
        {
                delete l_cs->m_ups;
                l_cs->m_ups = NULL;
        }
        rqst_h::send_json_resp_err(*l_cs,
                                   false,
                                   // TODO use supports keep-alives from client request
                                   a_status);
        return HLX_STATUS_OK;
}

} //namespace ns_hlx {
