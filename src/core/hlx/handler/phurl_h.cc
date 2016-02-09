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
#include "hlx/hlx.h"
#include "hlx/phurl_h.h"
#include "hconn.h"
#include "t_hlx.h"
#include "ndebug.h"

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
phurl_h_resp::phurl_h_resp(uint32_t a_timeout_ms, float a_completion_ratio) :
        m_pending_subr_uid_map(),
        m_resp_list(),
        m_phurl_h(NULL),
        m_requester_hconn(NULL),
        m_data(NULL),
        m_timer(NULL),
        m_size(0),
        m_timeout_ms(a_timeout_ms),
        m_completion_ratio(a_completion_ratio),
        m_delete(true),
        m_done(false),
        m_create_resp_cb(NULL)
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
phurl_h_resp::~phurl_h_resp(void)
{
        for(hlx_resp_list_t::iterator i_resp = m_resp_list.begin(); i_resp != m_resp_list.end(); ++i_resp)
        {
                if(*i_resp)
                {
                        if(m_requester_hconn && m_requester_hconn->m_t_hlx)
                        {
                                t_hlx *l_t_hlx = m_requester_hconn->m_t_hlx;
                                resp *l_resp = (*i_resp)->m_resp;
                                if(l_resp)
                                {
                                        if(l_resp->get_q())
                                        {
                                                l_t_hlx->release_nbq(l_resp->get_q());
                                                l_resp->set_q(NULL);
                                        }
                                        l_t_hlx->release_resp((*i_resp)->m_resp);
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
int32_t phurl_h_resp::done(void)
{
        if(m_done)
        {
                return STATUS_OK;
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
                        if(l_status != STATUS_OK)
                        {
                                // TODO ???
                        }
                }
        }
        // ---------------------------------------
        // Create Response...
        // ---------------------------------------
        int32_t l_status = STATUS_OK;
        if(m_create_resp_cb)
        {
                l_status = m_create_resp_cb(this);
                if(l_status != STATUS_OK)
                {
                        //return STATUS_ERROR;
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
float phurl_h_resp::get_done_ratio(void)
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
int32_t phurl_h_resp::s_timeout_cb(void *a_data)
{
        phurl_h_resp *l_phr = static_cast<phurl_h_resp *>(a_data);
        if(!l_phr)
        {
                return STATUS_ERROR;
        }
        return l_phr->done();
}

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
        m_subr_template.set_header("Connection", "keep-alive");
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
h_resp_t phurl_h::do_get(hconn &a_hconn,
                         rqst &a_rqst,
                         const url_pmap_t &a_url_pmap)
{
        return do_get_w_subr_template(a_hconn, a_rqst, a_url_pmap, m_subr_template);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t phurl_h::do_get_w_subr_template(hconn &a_hconn, rqst &a_rqst,
                                         const url_pmap_t &a_url_pmap, const subr &a_subr,
                                         phurl_h_resp *a_phr,
                                         uint32_t a_timeout_ms,
                                         float a_completion_ratio)
{
        // Create request state if not already made
        phurl_h_resp *l_phr = a_phr;
        if(!l_phr)
        {
                l_phr = new phurl_h_resp();
                l_phr->m_phurl_h = this;
        }
        for(host_list_t::iterator i_host = m_host_list.begin(); i_host != m_host_list.end(); ++i_host)
        {
                subr &l_subr = create_subr(a_hconn, a_subr);
                l_subr.set_host(i_host->m_host);
                l_subr.reset_label();
                l_subr.set_data(l_phr);

                // Add to pending map
                l_phr->m_pending_subr_uid_map[l_subr.get_uid()] = &l_subr;

                int32_t l_status = 0;
                l_status = queue_subr(a_hconn, l_subr);
                if(l_status != HLX_STATUS_OK)
                {
                        //("Error: performing add_subreq.\n");
                        return H_RESP_SERVER_ERROR;
                }
        }
        l_phr->m_requester_hconn = &a_hconn;
        l_phr->m_size = l_phr->m_pending_subr_uid_map.size();
        l_phr->m_timeout_ms = a_timeout_ms;
        l_phr->m_completion_ratio = a_completion_ratio;
        l_phr->m_create_resp_cb = s_create_resp;
        if(l_phr->m_timeout_ms)
        {
                // TODO set timeout
                if(!a_hconn.m_t_hlx)
                {
                        return H_RESP_SERVER_ERROR;
                }
                // TODO -cancel pending???
                int32_t l_status;
                l_status = add_timer(a_hconn, l_phr->m_timeout_ms, phurl_h_resp::s_timeout_cb, l_phr, &(l_phr->m_timer));
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
        phurl_h_resp *l_phr = static_cast<phurl_h_resp *>(a_subr.get_data());
        if(!l_phr)
        {
                return STATUS_ERROR;
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
int32_t phurl_h::s_error_cb(subr &a_subr, nconn &a_nconn)
{
        phurl_h_resp *l_phr = static_cast<phurl_h_resp *>(a_subr.get_data());
        if(!l_phr)
        {
                return STATUS_ERROR;
        }
        ns_hlx::hlx_resp *l_resp = new ns_hlx::hlx_resp();
        l_resp->m_resp = NULL;
        l_resp->m_subr = &a_subr;
        l_resp->m_error_str = ns_hlx::nconn_get_last_error_str(a_nconn);
        l_resp->m_status_code = get_status_code(a_subr);
        l_phr->m_resp_list.push_back(l_resp);
        l_phr->m_pending_subr_uid_map.erase(a_subr.get_uid());
        return s_done_check(a_subr, l_phr);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t phurl_h::s_done_check(subr &a_subr, phurl_h_resp *a_phr)
{
        if(!a_phr)
        {
                return STATUS_ERROR;
        }
        if(a_phr->m_done)
        {
                return STATUS_OK;
        }
        if(( (a_phr->m_completion_ratio < 100.0) &&
             (a_phr->get_done_ratio() >= a_phr->m_completion_ratio)) ||
           !(a_phr->m_pending_subr_uid_map.size()))
        {
                // Cancel pending
                int32_t l_status;
                if(a_subr.get_hconn())
                {
                        l_status = cancel_timer(*a_subr.get_hconn(), a_phr->m_timer);
                        if(l_status != STATUS_OK)
                        {
                                // TODO ???
                                // warn -still need to cancel...
                        }
                        a_phr->m_timer = NULL;
                }
                return a_phr->done();
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t phurl_h::s_create_resp(phurl_h_resp *a_phr)
{
        //NDBG_PRINT("DONE\n");
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
        sprintf(l_len_str, "%lu", l_len);

        if(!a_phr->m_requester_hconn)
        {
                return STATUS_ERROR;
        }

        // Create resp
        api_resp &l_api_resp = create_api_resp(*(a_phr->m_requester_hconn));
        l_api_resp.set_status(HTTP_STATUS_OK);
        l_api_resp.set_header("Content-Length", l_len_str);
        l_api_resp.set_body_data(l_buf, l_len);

        // Queue
        queue_api_resp(*(a_phr->m_requester_hconn), l_api_resp);
        return STATUS_OK;
}

} //namespace ns_hlx {
