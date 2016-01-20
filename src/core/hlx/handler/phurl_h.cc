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
#include "ndebug.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
phurl_h_resp::phurl_h_resp(void) :
        m_pending_uid_set(),
        m_resp_list(),
        m_phurl_h(NULL),
        m_data(NULL)
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
                        if((*i_resp)->m_resp)
                        {
                                delete (*i_resp)->m_resp;
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
const subr &phurl_h::get_subr_template(void)
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
//: \details: Initialize
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool phurl_h::init_resp(subr &a_subr)
{

        // if the subr doesn't have a object to carry the response state, set it
        phurl_h_resp *l_resp = static_cast <phurl_h_resp*>(a_subr.get_data());
        if(NULL == l_resp){
                // no state made already
                // create a new phurl handler response to carry the state
                l_resp = new phurl_h_resp();
                a_subr.set_data(l_resp);
        }
        l_resp->m_phurl_h = this;
        return true;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t phurl_h::do_get_w_subr_template(hconn &a_hconn, rqst &a_rqst,
                                         const url_pmap_t &a_url_pmap, subr &a_subr)
{
        // Create request state if not already made
        init_resp(a_subr);

        phurl_h_resp *l_floodecho_response = static_cast <phurl_h_resp*>(a_subr.get_data());

        for(host_list_t::iterator i_host = m_host_list.begin(); i_host != m_host_list.end(); ++i_host)
        {
                subr &l_subr = create_subr(a_hconn, a_subr);
                l_subr.set_host(i_host->m_host);
                l_subr.reset_label();

                l_floodecho_response->m_pending_uid_set.insert(l_subr.get_uid());

                int32_t l_status = 0;
                l_status = queue_subr(a_hconn, l_subr);
                if(l_status != HLX_STATUS_OK)
                {
                        //("Error: performing add_subreq.\n");
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
        phurl_h_resp *l_fanout_resp = static_cast<phurl_h_resp *>(a_subr.get_data());
        if(!l_fanout_resp)
        {
                return -1;
        }
        hlx_resp *l_resp = new hlx_resp();
        l_resp->m_resp = &a_resp;
        l_resp->m_subr = &a_subr;
        l_fanout_resp->m_pending_uid_set.erase(a_subr.get_uid());
        l_fanout_resp->m_resp_list.push_back(l_resp);
        // Check for done
        if(l_fanout_resp->m_pending_uid_set.empty())
        {
                if(!l_fanout_resp->m_phurl_h)
                {
                        return -1;
                }
                int32_t l_status;
                // Create resp will destroy fanout resp obj
                l_status = l_fanout_resp->m_phurl_h->create_resp(a_subr, l_fanout_resp);
                if(l_status != 0)
                {
                        return -1;
                }
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t phurl_h::s_error_cb(subr &a_subr, nconn &a_nconn)
{
        phurl_h_resp *l_fanout_resp = static_cast<phurl_h_resp *>(a_subr.get_data());
        if(!l_fanout_resp)
        {
                return -1;
        }
        l_fanout_resp->m_pending_uid_set.erase(a_subr.get_uid());
        // Check for done
        if(l_fanout_resp->m_pending_uid_set.empty())
        {
                if(!l_fanout_resp->m_phurl_h)
                {
                        return -1;
                }
                int32_t l_status;
                // Create resp will destroy fanout resp obj
                l_status = l_fanout_resp->m_phurl_h->create_resp(a_subr, l_fanout_resp);
                if(l_status != 0)
                {
                        return -1;
                }
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t phurl_h::create_resp(subr &a_subr, phurl_h_resp *a_fanout_resp)
{
        // Get body of resp
        char l_buf[2048];
        l_buf[0] = '\0';
        for(hlx_resp_list_t::iterator i_resp = a_fanout_resp->m_resp_list.begin();
            i_resp != a_fanout_resp->m_resp_list.end();
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

        // Create resp
        api_resp &l_api_resp = create_api_resp(*(a_subr.get_requester_hconn()));
        l_api_resp.set_status(HTTP_STATUS_OK);
        l_api_resp.set_header("Content-Length", l_len_str);
        l_api_resp.set_body_data(l_buf, l_len);

        // Queue
        queue_api_resp(*(a_subr.get_requester_hconn()), l_api_resp);
        delete a_fanout_resp;
        return 0;
}

} //namespace ns_hlx {
