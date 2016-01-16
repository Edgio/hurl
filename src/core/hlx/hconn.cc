//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hconn.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    07/20/2015
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
#include "t_hlx.h"
#include "hconn.h"
#include "time_util.h"
#include "hlx/url_router.h"
#include "nbq.h"
#include "ndebug.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Static
//: ----------------------------------------------------------------------------
default_rqst_h hconn::s_default_rqst_h;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hconn::hconn(void):
        m_type(HCONN_TYPE_NONE),
        m_nconn(NULL),
        m_t_hlx(NULL),
        m_timer_obj(NULL),
        m_hmsg(NULL),
        m_http_parser_settings(),
        m_http_parser(),
        m_cur_off(0),
        m_cur_buf(NULL),
        m_save(false),
        m_status_code(0),
        m_verbose(false),
        m_color(false),
        m_url_router(NULL),
        m_in_q(NULL),
        m_out_q(NULL),
        m_subr(NULL),
        m_idx(0),

        // TODO TEST
        m_fs(NULL)
{}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hconn::run_state_machine(hconn_ev_cb_t a_ev_cb, int32_t a_conn_status)
{
        //NDBG_PRINT("%sRUN_STATE_MACHINE%s: EV_CB[%d] CONN_STATUS[%d]\n",
        //                ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, a_ev_cb, a_conn_status);
        switch(a_ev_cb)
        {
        // -----------------------------------------------------------
        // Readable
        // -----------------------------------------------------------
        case HCONN_EV_CB_READABLE:
        {
                //NDBG_PRINT("LABEL[%s] l_status: %d\n", m_nconn->get_label().c_str(), a_conn_status);
                switch(a_conn_status)
                {
                case nconn::NC_STATUS_EOF:
                {
                        // Connect only && done --early exit...
                        if(m_nconn->get_connect_only())
                        {
                                //NDBG_PRINT("LABEL[%s] l_status: %d\n", l_nconn->get_label().c_str(), l_status);
                                if(m_hmsg)
                                {
                                        resp *l_resp = static_cast<resp *>(m_hmsg);
                                        l_resp->set_status(200);
                                }
                                m_status_code = 200;
                                m_t_hlx->add_stat_to_agg(m_nconn->get_stats(), m_status_code);
                        }
                        if(m_subr)
                        {
                                m_subr->bump_num_completed();
                                subr_complete();
                        }
                        //NDBG_PRINT("Cleanup EOF\n");
                        return STATUS_OK;
                }
                case nconn::NC_STATUS_ERROR:
                {
                        // subr...
                        if(m_subr)
                        {
                                if(m_hmsg)
                                {
                                        resp *l_resp = static_cast<resp *>(m_hmsg);
                                        l_resp->set_status(901);
                                }
                                subr_error();
                                // TODO Check error;
                        }
                        //NDBG_PRINT("Error: nc_run_state_machine.\n");
                        return STATUS_OK;
                }
                default:
                {
                        //NDBG_PRINT("m_nconn->is_done(): %d\n", m_nconn->is_done());
                        //NDBG_PRINT("m_hmsg:             %p\n", m_hmsg);
                        //NDBG_PRINT("m_hmsg->m_complete: %d\n", m_hmsg->m_complete);
                        // Handle completion
                        if((m_nconn->is_done()) ||
                           (m_hmsg &&
                            m_hmsg->m_complete))
                        {
                                // Display...
                                // TODO FIX!!!
                                if(m_verbose && m_hmsg)
                                {
                                        if(m_color) NDBG_OUTPUT("%s", ANSI_COLOR_FG_CYAN);
                                        m_hmsg->show();
                                        if(m_color) NDBG_OUTPUT("%s", ANSI_COLOR_OFF);
                                }

                                // ---------------------------------------------------
                                // subr...
                                // ---------------------------------------------------
                                if(m_subr)
                                {
                                        // Get request time
                                        if(m_nconn->get_collect_stats_flag())
                                        {
                                                m_nconn->set_stat_tt_completion_us(get_delta_time_us(m_nconn->get_connect_start_time_us()));
                                        }
                                        m_t_hlx->add_stat_to_agg(m_nconn->get_stats(), m_status_code);
                                        m_subr->bump_num_completed();
                                        bool l_complete = subr_complete();
                                        if(l_complete ||
                                           !m_nconn->can_reuse() ||
                                           !m_subr->get_keepalive() ||
                                           ((m_hmsg != NULL) && (!m_hmsg->m_supports_keep_alives)))
                                        {
                                                //NDBG_PRINT("Cleanup: subr done l_complete: %d l_nconn->can_reuse(): %d m_subr->get_keepalive(): %d.\n",
                                                //                l_complete, m_nconn->can_reuse(),
                                                //                m_subr->get_keepalive());
                                                return nconn::NC_STATUS_EOF;
                                        }
                                        return nconn::NC_STATUS_IDLE;
                                }
                                // ---------------------------------------------------
                                // server...
                                // ---------------------------------------------------
                                else
                                {
                                        //NDBG_PRINT("g_req_num: %d\n", ++g_req_num);
                                        ++m_t_hlx->m_stat.m_num_cln_reqs;

                                        // request handling...
                                        if(handle_req() != STATUS_OK)
                                        {
                                                //NDBG_PRINT("Error performing handle_req\n");
                                                return nconn::NC_STATUS_ERROR;
                                        }
                                        if(m_hmsg)
                                        {
                                                m_hmsg->clear();
                                        }
                                        if(m_in_q)
                                        {
                                                m_in_q->reset_write();
                                        }
                                }
                        }
                        //NDBG_PRINT("Error: nc_run_state_machine.\n");
                        return STATUS_OK;
                }
                }
        }
        // -----------------------------------------------------------
        // Writeable
        // -----------------------------------------------------------
        case HCONN_EV_CB_WRITEABLE:
        {
                // TODO Make callback...
                if(m_fs)
                {
                        if(!m_out_q->read_avail())
                        {
                                m_fs->fsread(*(m_out_q), 32768);
                                if(m_fs->fsdone())
                                {
                                        delete m_fs;
                                        m_fs = NULL;
                                }
                        }
                }

                //NDBG_PRINT("LABEL[%s] l_status: %d\n", l_nconn->get_label().c_str(), l_status);
                switch(a_conn_status)
                {
                case nconn::NC_STATUS_ERROR:
                {
                        if(m_subr)
                        {
                                subr::error_cb_t l_error_cb = m_subr->get_error_cb();
                                if(l_error_cb)
                                {
                                        l_error_cb(*m_subr, *m_nconn);
                                }
                        }
                        return STATUS_OK;
                }
                case nconn::NC_STATUS_EOF:
                {
                        if(m_subr)
                        {
                                m_subr->bump_num_completed();
                                bool l_complete = subr_complete();
                                if(l_complete)
                                {
                                        //NDBG_PRINT("Cleanup subr complete\n");
                                        return STATUS_OK;
                                }
                        }
                        //NDBG_PRINT("Cleanup EOF\n");
                        return STATUS_OK;
                }
                default:
                {
                        if(!m_nconn->is_accepting() &&
                           m_out_q &&
                           !m_out_q->read_avail() &&
                           (m_type == HCONN_TYPE_CLIENT))
                        {
                                if((m_hmsg != NULL) &&
                                   (!m_hmsg->m_supports_keep_alives))
                                {
                                        return nconn::NC_STATUS_BREAK;
                                }
                                // No data left to send
                                return nconn::NC_STATUS_EOF;
                        }
                        else
                        {
                                if(a_conn_status == nconn::NC_STATUS_OK)
                                {
                                        return nconn::NC_STATUS_BREAK;
                                }
                        }
                        return STATUS_OK;
                }
                }
        }
        // -----------------------------------------------------------
        // Timeout
        // -----------------------------------------------------------
        case HCONN_EV_CB_TIMEOUT:
        {
                if(m_type == HCONN_TYPE_UPSTREAM)
                {
                        ++m_t_hlx->m_stat.m_num_ups_idle_killed;
                        if(m_subr)
                        {
                                subr_error();
                                // TODO Check error;
                        }
                }
                else if(m_type == HCONN_TYPE_CLIENT)
                {
                        ++m_t_hlx->m_stat.m_num_cln_idle_killed;
                }
                return STATUS_OK;
        }
        // -----------------------------------------------------------
        // Error
        // -----------------------------------------------------------
        case HCONN_EV_CB_ERROR:
        {
                if(m_type == HCONN_TYPE_UPSTREAM)
                {
                        if(m_subr)
                        {
                                subr_error();
                                // TODO Check error;
                        }
                }
                return STATUS_OK;
        }
        // -----------------------------------------------------------
        // Default
        // -----------------------------------------------------------
        default:
        {
                return STATUS_OK;
        }
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hconn::handle_req(void)
{
        rqst *l_rqst = static_cast<rqst *>(m_hmsg);
        if(!l_rqst)
        {
                return STATUS_ERROR;
        }
        // TODO Null check...
        if(!m_url_router)
        {
                return STATUS_ERROR;
        }
        url_pmap_t l_pmap;
        //NDBG_PRINT("a_url_router:   %p\n", a_url_router);
        //NDBG_PRINT("a_req.m_method: %d\n", l_rqst->m_method);
        rqst_h *l_rqst_h = (rqst_h *)m_url_router->find_route(l_rqst->get_url_path(),l_pmap);
        //NDBG_PRINT("l_rqst_h:       %p\n", l_rqst_h);
        h_resp_t l_hdlr_status = H_RESP_NONE;
        if(l_rqst_h)
        {
                // Method switch
                switch(l_rqst->m_method)
                {
                case HTTP_GET:
                {
                        l_hdlr_status = l_rqst_h->do_get(*this, *l_rqst, l_pmap);
                        break;
                }
                case HTTP_POST:
                {
                        l_hdlr_status = l_rqst_h->do_post(*this, *l_rqst, l_pmap);
                        break;
                }
                case HTTP_PUT:
                {
                        l_hdlr_status = l_rqst_h->do_put(*this, *l_rqst, l_pmap);
                        break;
                }
                case HTTP_DELETE:
                {
                        l_hdlr_status = l_rqst_h->do_delete(*this, *l_rqst, l_pmap);
                        break;
                }
                default:
                {
                        l_hdlr_status = l_rqst_h->do_default(*this, *l_rqst, l_pmap);
                        break;
                }
                }
        }
        else
        {
                // Default response
                l_hdlr_status = s_default_rqst_h.do_get(*this, *l_rqst, l_pmap);
        }

        switch(l_hdlr_status)
        {
        case H_RESP_DONE:
        {
                break;
        }
        case H_RESP_DEFERRED:
        {
                break;
        }
        case H_RESP_ERROR:
        {
                break;
        }
        default:
        {
                break;
        }
        }
        // TODO Handler errors?
        if(l_hdlr_status == H_RESP_ERROR)
        {
                return STATUS_ERROR;
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: Subrequests
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool hconn::subr_complete(void)
{
        subr *l_subr = m_subr;
        nconn *l_nconn = m_nconn;
        resp *l_resp = static_cast<resp *>(m_hmsg);

        bool l_complete = false;
        if(m_subr->get_type() != SUBR_TYPE_DUPE)
        {
                m_subr->set_end_time_ms(get_time_ms());
        }
        subr::completion_cb_t l_completion_cb = l_subr->get_completion_cb();
        // Call completion handler
        if(l_completion_cb)
        {
                l_completion_cb(*l_subr, *l_nconn, *l_resp);
        }
        // Connect only
        if(l_subr->get_connect_only())
        {
                l_complete = true;
        }
        if(m_subr->get_detach_resp())
        {
                m_subr = NULL;
                m_hmsg = NULL;
                m_in_q = NULL;
        }
        else
        {
                if(m_subr->get_type() != SUBR_TYPE_DUPE)
                {
                        delete m_subr;
                        m_subr = NULL;
                }
        }
        return l_complete;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hconn::subr_error(void)
{
        nconn *l_nconn = m_nconn;
        resp *l_resp = static_cast<resp *>(m_hmsg);
        m_subr->bump_num_completed();
        if(m_subr->get_type() != SUBR_TYPE_DUPE)
        {
                m_subr->set_end_time_ms(get_time_ms());
        }
        if(l_nconn->get_collect_stats_flag())
        {
                l_nconn->set_stat_tt_completion_us(get_delta_time_us(l_nconn->get_connect_start_time_us()));
        }
        // TODO ??? why two status codes ???
        m_status_code = l_resp->get_status();

        // TODO FIX!!!
        //m_t_hlx->add_stat_to_agg(l_nconn->get_stats(), l_resp->get_status());

        l_nconn->reset_stats();
        subr::error_cb_t l_error_cb = m_subr->get_error_cb();
        if(l_error_cb)
        {
                l_error_cb(*(m_subr), *l_nconn);
        }

        if(m_subr->get_detach_resp())
        {
                if(m_hmsg)
                {
                        delete m_hmsg;
                        m_hmsg = NULL;
                }
                m_subr = NULL;
        }
        else
        {
                if(m_subr->get_type() != SUBR_TYPE_DUPE)
                {
                        delete m_subr;
                        m_subr = NULL;
                }
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subr &create_subr(hconn &a_hconn)
{
        subr *l_subr = new subr();
        l_subr->set_uid(a_hconn.m_t_hlx->get_hlx()->get_next_subr_uuid());
        // TODO check exists!!!
        return *l_subr;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subr &create_subr(hconn &a_hconn, const subr &a_subr)
{
        subr *l_subr = new subr(a_subr);
        l_subr->set_uid(a_hconn.m_t_hlx->get_hlx()->get_next_subr_uuid());
        // TODO check exists!!!
        return *l_subr;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t queue_subr(hconn &a_hconn, subr &a_subr)
{
        a_subr.set_requester_hconn(&a_hconn);
        a_hconn.m_t_hlx->subr_add(a_subr);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: API Responses
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
api_resp &create_api_resp(hconn &a_hconn)
{
        api_resp *l_api_resp = new api_resp();
        return *l_api_resp;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t queue_api_resp(hconn &a_hconn, api_resp &a_api_resp)
{
        if(!a_hconn.m_t_hlx)
        {
            return HLX_STATUS_ERROR;
        }
        a_hconn.m_t_hlx->queue_api_resp(a_api_resp, a_hconn);
        delete &a_api_resp;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t queue_resp(hconn &a_hconn)
{
        if(!a_hconn.m_t_hlx)
        {
            return HLX_STATUS_ERROR;
        }
        a_hconn.m_t_hlx->queue_output(a_hconn);
        return HLX_STATUS_OK;
}

} //namespace ns_hlx {
