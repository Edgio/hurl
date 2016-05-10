//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    proxy_u.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    4/26/2015
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
#include "clnt_session.h"
#include "ups_srvr_session.h"
#include "nbq.h"
#include "t_srvr.h"
#include "hlx/proxy_u.h"
#include "hlx/subr.h"
#include "hlx/status.h"
#include "hlx/trace.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
proxy_u::proxy_u(clnt_session &a_clnt_session,
                 subr &a_subr):
        base_u(a_clnt_session),
        m_subr(a_subr)
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
        delete &m_subr;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ssize_t proxy_u::ups_read(size_t a_len)
{
        //NDBG_PRINT("a_len: %ld\n", a_len);
        //NDBG_PRINT_BT();
        if(ups_done())
        {
                return HLX_STATUS_OK;
        }
        m_state = UPS_STATE_SENDING;
        if(a_len <= 0)
        {
                return HLX_STATUS_OK;
        }
        ups_srvr_session *l_ups_srvr_session = m_subr.get_ups_srvr_session();
        if(!l_ups_srvr_session || !l_ups_srvr_session->m_in_q)
        {
                TRC_ERROR("requester ups_srvr_session or m_in_q == NULL\n");
                return HLX_STATUS_ERROR;
        }
        if(!m_clnt_session.m_out_q)
        {
                m_clnt_session.m_out_q = l_ups_srvr_session->m_in_q;
                l_ups_srvr_session->m_in_q_detached = true;
        }
        //NDBG_PRINT("l_ups_srvr_session->m_in_q->b_read_avail() = %d\n", l_ups_srvr_session->m_in_q->b_read_avail());
        //NDBG_PRINT("m_clnt_session.m_out_q->b_read_avail()     = %d\n", m_clnt_session.m_out_q->b_read_avail());
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
        m_state = UPS_STATE_DONE;
        //NDBG_PRINT("%sUPS_CANCEL%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
        // cancel subrequest
        m_subr.set_error_cb(NULL);
        int32_t l_s;
        l_s = m_subr.cancel();
        if(l_s != HLX_STATUS_OK)
        {
                TRC_ERROR("performing subr cancel.\n");
        }
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
