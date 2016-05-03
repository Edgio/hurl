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
#include "hlx/proxy_u.h"
#include "hlx/subr.h"
#include "hlx/status.h"
#include "hlx/trace.h"
#include "ndebug.h"
#include "clnt_session.h"
#include "ups_srvr_session.h"
#include "nbq.h"
#include "t_srvr.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
proxy_u::proxy_u(void):
                m_subr(NULL)
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
proxy_u::~proxy_u(void)
{
        if(m_subr)
        {
                delete m_subr;
                m_subr = NULL;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void proxy_u::set_subr(subr *a_subr)
{
        m_subr = a_subr;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ssize_t proxy_u::ups_read(char *ao_dst, size_t a_len)
{
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ssize_t proxy_u::ups_read(nbq &ao_q, size_t a_len)
{
        //NDBG_PRINT("a_len: %ld\n", a_len);
        //NDBG_PRINT_BT();
        if(a_len <= 0)
        {
                return HLX_STATUS_OK;
        }
        if(!m_subr)
        {
                TRC_ERROR("m_subr == NULL\n");
                return HLX_STATUS_ERROR;
        }
        // Create resp
        clnt_session *l_clnt_session = m_subr->get_clnt_session();
        if(!l_clnt_session)
        {
                TRC_ERROR("requester ups_srvr_session == NULL\n");
                return HLX_STATUS_ERROR;
        }
        nbq *l_out_q = l_clnt_session->m_out_q;
        if(!l_out_q)
        {
                TRC_ERROR("requester l_clnt_session out_q == NULL\n");
                return HLX_STATUS_ERROR;
        }
        ups_srvr_session *l_ups_srvr_session = m_subr->get_ups_srvr_session();
        if(!l_ups_srvr_session || !l_ups_srvr_session->m_in_q)
        {
                TRC_ERROR("requester ups_srvr_session or m_in_q == NULL\n");
                return HLX_STATUS_ERROR;
        }
        //NDBG_PRINT("m_in_q->b_read_avail() = %d\n", m_in_q->b_read_avail());
        int64_t l_wrote;
        l_wrote = l_clnt_session->m_out_q->write_q(*(l_ups_srvr_session->m_in_q));
        if(l_wrote > 0)
        {
                l_clnt_session->m_t_srvr->queue_output(*l_clnt_session);
        }
        else
        {
                //NDBG_PRINT("Error writing to ups_srvr_session out_q\n");
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool proxy_u::ups_done(void)
{
        if(m_subr)
        {
                return false;
        }
        return true;
}

//: ----------------------------------------------------------------------------
//: \details: Cancel and cleanup
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t proxy_u::ups_cancel(void)
{
        //NDBG_PRINT("%sUPS_CANCEL%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
        if(m_subr)
        {
                // cancel subrequest
                m_subr->set_error_cb(NULL);
                int32_t l_s;
                l_s = m_subr->cancel();
                if(l_s != HLX_STATUS_OK)
                {
                        TRC_ERROR("performing subr cancel.\n");
                }
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
        clnt_session *l_clnt_session = a_subr.get_clnt_session();
        if(!l_clnt_session)
        {
                return HLX_STATUS_ERROR;
        }
        if(l_clnt_session->m_ups)
        {
                delete l_clnt_session->m_ups;
                l_clnt_session->m_ups = NULL;
        }
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
        if(!a_subr.get_clnt_session())
        {
                return HLX_STATUS_ERROR;
        }
        clnt_session *l_clnt_session = a_subr.get_clnt_session();
        if(!l_clnt_session)
        {
                return HLX_STATUS_ERROR;
        }
        if(l_clnt_session->m_ups)
        {
                delete l_clnt_session->m_ups;
                l_clnt_session->m_ups = NULL;
        }
        rqst_h::send_json_resp_err(*l_clnt_session,
                                   false,
                                   // TODO use supports keep-alives from client request
                                   a_status);
        return HLX_STATUS_OK;
}


} //namespace ns_hlx {
