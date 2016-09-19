//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    phurl_h.cc
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
#include "t_srvr.h"
#include "ndebug.h"
#include "hlx/clnt_session.h"
#include "hlx/ups_srvr_session.h"
#include "hlx/phurl_h.h"
#include "hlx/api_resp.h"
#include "hlx/srvr.h"
#include "hlx/status.h"
#include "hlx/trace.h"
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <string.h>
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
phurl_h::phurl_h(void):
        default_rqst_h(),
        m_subr_template(),
        m_host_list()
{
        // Setup template
        m_subr_template.init_with_url("http://blorp/");
        m_subr_template.set_completion_cb(s_completion_cb);
        m_subr_template.set_error_cb(s_error_cb);
        m_subr_template.set_header("User-Agent", "hlx");
        m_subr_template.set_header("Accept", "*/*");
        m_subr_template.set_keepalive(true);
        m_subr_template.set_save(true);
        m_subr_template.set_detach_resp(true);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
phurl_h::~phurl_h(void)
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const subr &phurl_h::get_subr_template(void) const
{
        return m_subr_template;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subr &phurl_h::get_subr_template_mutable(void)
{
        return m_subr_template;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void phurl_h::add_host(const std::string a_host, uint16_t a_port)
{
        // Setup host list
        host_s l_host(a_host, a_port);
        m_host_list.push_back(l_host);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void phurl_h::set_host_list(const host_list_t &a_host_list)
{
        m_host_list = a_host_list;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t phurl_h::do_default(clnt_session &a_clnt_session,
                             rqst &a_rqst,
                             const url_pmap_t &a_url_pmap)
{
        // Create request state if not already made
        phurl_u *l_phr = new phurl_u(a_clnt_session);
        for(host_list_t::iterator i_host = m_host_list.begin(); i_host != m_host_list.end(); ++i_host)
        {
                subr *l_subr_ptr = a_clnt_session.create_subr(&m_subr_template);
                if(l_subr_ptr)
                {
                        TRC_ERROR("creating subr.\n");
                        return H_RESP_SERVER_ERROR;
                }
                subr &l_subr = *l_subr_ptr;

                const char *l_body_data = a_rqst.get_body_data();
                uint64_t l_body_data_len = a_rqst.get_body_len();
                // subr setup
                //l_subr.init_with_url(l_url);
                l_subr.set_keepalive(true);
                l_subr.set_timeout_ms(10000);
                l_subr.set_verb(a_rqst.get_method_str());
                l_subr.set_headers(a_rqst.get_headers());
                l_subr.set_host(i_host->m_host);
                l_subr.set_port(i_host->m_port);
                l_subr.reset_label();
                l_subr.set_data(l_phr);
                l_subr.set_body_data(l_body_data, l_body_data_len);
                l_subr.set_clnt_session(&a_clnt_session);

                // Add to pending map
                l_phr->m_pending_subr_uid_map[l_subr.get_uid()] = &l_subr;

                int32_t l_status = 0;
                l_status = a_clnt_session.queue_subr(l_subr);
                if(l_status != HLX_STATUS_OK)
                {
                        TRC_ERROR("performing add_subreq.\n");
                        return H_RESP_SERVER_ERROR;
                }
        }
        l_phr->m_phurl_h = this;
        l_phr->m_size = l_phr->m_pending_subr_uid_map.size();
        l_phr->m_timeout_ms = 10000;
        l_phr->m_completion_ratio = 100.0;
        if(l_phr->m_timeout_ms)
        {
                // TODO set timeout
                if(!a_clnt_session.m_t_srvr)
                {
                        return H_RESP_SERVER_ERROR;
                }
                // TODO -cancel pending???
                int32_t l_status;
                l_status = a_clnt_session.add_timer(l_phr->m_timeout_ms, phurl_u::s_timeout_cb, l_phr, &(l_phr->m_timer));
                if(l_status != HLX_STATUS_OK)
                {
                        return H_RESP_SERVER_ERROR;
                }
        }
        return H_RESP_DONE;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t phurl_h::s_completion_cb(subr &a_subr, nconn &a_nconn, resp &a_resp)
{
        phurl_u *l_phr = static_cast<phurl_u *>(a_subr.get_data());
        if(!l_phr)
        {
                return HLX_STATUS_ERROR;
        }
        hlx_resp *l_resp = new hlx_resp();
        l_resp->m_resp = &a_resp;
        l_resp->m_subr = &a_subr;
        l_resp->m_status_code = a_resp.get_status();
        l_phr->m_resp_list.push_back(l_resp);
        l_phr->m_pending_subr_uid_map.erase(a_subr.get_uid());
        return s_done_check(a_subr, l_phr);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t phurl_h::s_error_cb(subr &a_subr,
                            nconn *a_nconn,
                            http_status_t a_status,
                            const char *a_error_str)
{
        phurl_u *l_phr = static_cast<phurl_u *>(a_subr.get_data());
        if(!l_phr)
        {
                return HLX_STATUS_ERROR;
        }
        ns_hlx::hlx_resp *l_resp = new ns_hlx::hlx_resp();
        l_resp->m_resp = NULL;
        l_resp->m_subr = &a_subr;
        l_resp->m_error_str = a_error_str;
        l_resp->m_status_code = a_status;
        l_phr->m_resp_list.push_back(l_resp);
        l_phr->m_pending_subr_uid_map.erase(a_subr.get_uid());
        return s_done_check(a_subr, l_phr);
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
        l_phr->m_timer = NULL;
        return l_phr->ups_cancel();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t phurl_h::s_done_check(subr &a_subr, phurl_u *a_phr)
{
        if(!a_phr)
        {
                return HLX_STATUS_ERROR;
        }
        if(a_phr->ups_done())
        {
                return HLX_STATUS_OK;
        }
        if(( (a_phr->m_completion_ratio < 100.0) &&
             (a_phr->get_done_ratio() >= a_phr->m_completion_ratio)) ||
           !(a_phr->m_pending_subr_uid_map.size()))
        {
                return a_phr->ups_cancel();
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t phurl_h::s_create_resp(phurl_u *a_phr)
{
        char l_buf[2048];
        l_buf[0] = '\0';
        for(hlx_resp_list_t::iterator i_resp = a_phr->m_resp_list.begin();
            i_resp != a_phr->m_resp_list.end();
            ++i_resp)
        {
                char *l_status_buf = NULL;
                int l_as_len = asprintf(&l_status_buf, "STATUS: %u\n", (*i_resp)->m_resp->get_status());
                strncat(l_buf, l_status_buf, l_as_len);
                free(l_status_buf);
        }
        uint64_t l_len = strnlen(l_buf, 2048);
        char l_len_str[64];
        sprintf(l_len_str, "%" PRIu64 "", l_len);
        api_resp &l_api_resp = create_api_resp(a_phr->get_clnt_session());
        l_api_resp.set_status(HTTP_STATUS_OK);
        l_api_resp.set_header("Content-Length", l_len_str);
        l_api_resp.set_body_data(l_buf, l_len);
        queue_api_resp(a_phr->get_clnt_session(), l_api_resp);
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool phurl_h::get_do_default(void)
{
        return true;
}

//: ----------------------------------------------------------------------------
//: Upstream Object
//: ----------------------------------------------------------------------------
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
phurl_u::phurl_u(clnt_session &a_clnt_session,
                 uint32_t a_timeout_ms,
                 float a_completion_ratio):
        base_u(a_clnt_session),
        m_pending_subr_uid_map(),
        m_resp_list(),
        m_phurl_h(NULL),
        m_data(NULL),
        m_timer(NULL),
        m_size(0),
        m_timeout_ms(a_timeout_ms),
        m_completion_ratio(a_completion_ratio),
        m_delete(true),
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
        ups_cancel();
        for(hlx_resp_list_t::iterator i_resp = m_resp_list.begin(); i_resp != m_resp_list.end(); ++i_resp)
        {
                if(*i_resp)
                {
                        t_srvr *l_t_srvr = m_clnt_session.m_t_srvr;
                        resp *l_resp = (*i_resp)->m_resp;
                        if(l_t_srvr &&
                           l_resp)
                        {
                                if(l_resp->get_q())
                                {
                                        m_clnt_session.m_t_srvr->release_nbq(l_resp->get_q());
                                        l_resp->set_q(NULL);
                                }
                                l_t_srvr->release_resp((*i_resp)->m_resp);
                                (*i_resp)->m_resp = NULL;
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
ssize_t phurl_u::ups_read(size_t a_len)
{
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
        }
        m_state = UPS_STATE_SENDING;
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: Cancel and cleanup
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t phurl_u::ups_cancel(void)
{
        if(m_timer)
        {
                cancel_timer(m_t_srvr, m_timer);
                // TODO Check status
                m_timer = NULL;
        }
        if(ups_done())
        {
                return HLX_STATUS_OK;
        }
        int32_t l_status = HLX_STATUS_OK;
        m_state = UPS_STATE_DONE;
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

} //namespace ns_hlx {
