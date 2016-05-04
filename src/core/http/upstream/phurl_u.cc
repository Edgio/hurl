//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    phurl_u.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    12/12/2015
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
#include "hlx/phurl_u.h"

#include "clnt_session.h"
#include "t_srvr.h"
#include "hlx/phurl_h.h"
#include "hlx/api_resp.h"
#include "hlx/srvr.h"
#include "hlx/status.h"
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <string.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlx_resp::hlx_resp():
        m_subr(NULL),
        m_resp(NULL),
        m_error_str(),
        m_status_code(0)
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlx_resp::~hlx_resp()
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
phurl_u::phurl_u(uint32_t a_timeout_ms, float a_completion_ratio):
        m_pending_subr_uid_map(),
        m_resp_list(),
        m_phurl_h(NULL),
        m_clnt_session(NULL),
        m_data(NULL),
        m_timer(NULL),
        m_size(0),
        m_timeout_ms(a_timeout_ms),
        m_completion_ratio(a_completion_ratio),
        m_delete(true),
        m_done(false),
        m_create_resp_cb(phurl_h::s_create_resp)
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
phurl_u::~phurl_u(void)
{
        for(hlx_resp_list_t::iterator i_resp = m_resp_list.begin(); i_resp != m_resp_list.end(); ++i_resp)
        {
                if(*i_resp)
                {
                        if(m_clnt_session && m_clnt_session->m_t_srvr)
                        {
                                t_srvr *l_t_srvr = m_clnt_session->m_t_srvr;
                                resp *l_resp = (*i_resp)->m_resp;
                                if(l_resp)
                                {
                                        if(l_resp->get_q())
                                        {
                                                l_t_srvr->release_nbq(l_resp->get_q());
                                                l_resp->set_q(NULL);
                                        }
                                        l_t_srvr->release_resp((*i_resp)->m_resp);
                                        (*i_resp)->m_resp = NULL;
                                }
                        }
                        delete *i_resp;
                        *i_resp = NULL;
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ssize_t phurl_u::ups_read(char *ao_dst, size_t a_len)
{
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ssize_t phurl_u::ups_read(nbq &ao_q, size_t a_len)
{
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool phurl_u::ups_done(void)
{
        return m_done;
}

//: ----------------------------------------------------------------------------
//: \details: Cancel and cleanup
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t phurl_u::ups_cancel(void)
{
        if(m_done)
        {
                return HLX_STATUS_OK;
        }
        m_done = true;
        // ---------------------------------------
        // Cancel pending...
        // ---------------------------------------
        subr_uid_map_t l_map = m_pending_subr_uid_map;
        //NDBG_PRINT("Cancel pending size: %lu\n", l_map.size());
        for(subr_uid_map_t::iterator i_s = l_map.begin(); i_s != l_map.end(); ++i_s)
        {
                if(i_s->second)
                {
                        int32_t l_status;
                        l_status = i_s->second->cancel();
                        if(l_status != HLX_STATUS_OK)
                        {
                                // TODO ???
                        }
                }
        }
        // ---------------------------------------
        // Create Response...
        // ---------------------------------------
        int32_t l_status = HLX_STATUS_OK;
        if(m_create_resp_cb)
        {
                l_status = m_create_resp_cb(this);
                if(l_status != HLX_STATUS_OK)
                {
                        //return HLX_STATUS_ERROR;
                }
        }
        if(m_delete)
        {
                delete this;
        }
        return l_status;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
float phurl_u::get_done_ratio(void)
{
        float l_size = (float)m_size;
        float l_done = (float)(m_pending_subr_uid_map.size());
        return 100.0*((l_size - l_done)/l_size);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t phurl_u::s_timeout_cb(void *a_ctx, void *a_data)
{
        phurl_u *l_phr = static_cast<phurl_u *>(a_data);
        if(!l_phr)
        {
                return HLX_STATUS_ERROR;
        }
        return l_phr->ups_cancel();
}

} //namespace ns_hlx {
