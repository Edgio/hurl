//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
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
#include "ndebug.h"
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
        phurl_u *l_phr = new phurl_u();
        for(host_list_t::iterator i_host = m_host_list.begin(); i_host != m_host_list.end(); ++i_host)
        {
                subr &l_subr = create_subr(a_clnt_session, m_subr_template);

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
                l_status = queue_subr(a_clnt_session, l_subr);
                if(l_status != HLX_STATUS_OK)
                {
                        //("Error: performing add_subreq.\n");
                        return H_RESP_SERVER_ERROR;
                }
        }

        l_phr->m_phurl_h = this;
        l_phr->m_requester_clnt_session = &a_clnt_session;
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
                l_status = add_timer(a_clnt_session, l_phr->m_timeout_ms, phurl_u::s_timeout_cb, l_phr, &(l_phr->m_timer));
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
int32_t phurl_h::s_done_check(subr &a_subr, phurl_u *a_phr)
{
        if(!a_phr)
        {
                return HLX_STATUS_ERROR;
        }
        if(a_phr->m_done)
        {
                return HLX_STATUS_OK;
        }
        if(( (a_phr->m_completion_ratio < 100.0) &&
             (a_phr->get_done_ratio() >= a_phr->m_completion_ratio)) ||
           !(a_phr->m_pending_subr_uid_map.size()))
        {
                // Cancel pending
                int32_t l_status;
                if(a_subr.get_ups_srvr_session())
                {
                        l_status = cancel_timer(*a_subr.get_ups_srvr_session(), a_phr->m_timer);
                        if(l_status != HLX_STATUS_OK)
                        {
                                // TODO ???
                                // warn -still need to cancel...
                        }
                        a_phr->m_timer = NULL;
                }
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
        // Get body of resp
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

        // Create length string
        char l_len_str[64];
        sprintf(l_len_str, "%" PRIu64 "", l_len);

        if(!a_phr->m_requester_clnt_session)
        {
                return HLX_STATUS_ERROR;
        }

        // Create resp
        api_resp &l_api_resp = create_api_resp(*(a_phr->m_requester_clnt_session));
        l_api_resp.set_status(HTTP_STATUS_OK);
        l_api_resp.set_header("Content-Length", l_len_str);
        l_api_resp.set_body_data(l_buf, l_len);

        // Queue
        queue_api_resp(*(a_phr->m_requester_clnt_session), l_api_resp);
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

} //namespace ns_hlx {
