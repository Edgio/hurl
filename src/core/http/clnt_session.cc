//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    clnt_session.cc
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
#include "clnt_session.h"

#include "t_srvr.h"
#include "nbq.h"
#include "ndebug.h"
#include "hlx/srvr.h"
#include "hlx/url_router.h"
#include "hlx/api_resp.h"
#include "hlx/lsnr.h"
#include "hlx/time_util.h"
#include "hlx/trace.h"
#include "hlx/status.h"
#include "hlx/file_u.h"

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define CHECK_FOR_NULL_ERROR_DEBUG(_data) \
        do {\
                if(!_data) {\
                        NDBG_PRINT("Error.\n");\
                        return HLX_STATUS_ERROR;\
                }\
        } while(0);

#define CHECK_FOR_NULL_ERROR(_data) \
        do {\
                if(!_data) {\
                        return HLX_STATUS_ERROR;\
                }\
        } while(0);

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Static
//: ----------------------------------------------------------------------------
default_rqst_h clnt_session::s_default_rqst_h;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
clnt_session::clnt_session(void):
        m_nconn(NULL),
        m_t_srvr(NULL),
        m_timer_obj(NULL),
        m_rqst(NULL),
        m_rqst_resp_logging(false),
        m_rqst_resp_logging_color(false),
        m_lsnr(NULL),
        m_in_q(NULL),
        m_out_q(NULL),
        m_idx(0),
        m_ups(NULL),
        m_access_info(),
        m_resp_done_cb(NULL)
{}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void clnt_session::clear(void)
{
        m_nconn = NULL;
        m_t_srvr = NULL;
        m_timer_obj = NULL;
        m_rqst = NULL;
        m_rqst_resp_logging = false;
        m_rqst_resp_logging_color = false;
        m_lsnr = NULL;
        m_in_q = NULL;
        m_out_q = NULL;
        m_ups = NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t clnt_session::evr_fd_readable_cb(void *a_data)
{
        return run_state_machine(a_data, nconn::NC_MODE_READ);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t clnt_session::evr_fd_writeable_cb(void *a_data)
{
        return run_state_machine(a_data, nconn::NC_MODE_WRITE);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t clnt_session::evr_fd_error_cb(void *a_data)
{
        return run_state_machine(a_data, nconn::NC_MODE_ERROR);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t clnt_session::evr_fd_timeout_cb(void *a_ctx, void *a_data)
{
        return run_state_machine(a_data, nconn::NC_MODE_TIMEOUT);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void clnt_session::cancel_last_timer(void)
{
        if(m_timer_obj && m_t_srvr)
        {
                m_t_srvr->cancel_timer(m_timer_obj);
                m_timer_obj = NULL;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t clnt_session::teardown(t_srvr *a_t_srvr, clnt_session *a_cs, nconn *a_nconn)
{
        if(a_cs)
        {
                if(a_cs->m_ups)
                {
                        if(!a_cs->m_ups->ups_done())
                        {
                                int32_t l_s;
                                l_s = a_cs->m_ups->ups_cancel();
                                if(l_s != HLX_STATUS_OK)
                                {
                                        TRC_ERROR("performing ups_cancel\n");
                                }
                        }
                        delete a_cs->m_ups;
                        a_cs->m_ups = NULL;
                }
        }
        if(a_t_srvr)
        {
                int32_t l_s;
                l_s = a_t_srvr->cleanup_clnt_session(a_cs, a_nconn);
                if(l_s != HLX_STATUS_OK)
                {
                        return HLX_STATUS_ERROR;
                }
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t clnt_session::run_state_machine(void *a_data, nconn::mode_t a_conn_mode)
{
        //NDBG_PRINT("RUN a_conn_mode: %d a_data: %p\n", a_conn_mode, a_data);
        CHECK_FOR_NULL_ERROR(a_data);
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_ctx());
        t_srvr *l_t_srvr = static_cast<t_srvr *>(l_nconn->get_ctx());
        clnt_session *l_cs = static_cast<clnt_session *>(l_nconn->get_data());
        // -------------------------------------------------
        // ERROR
        // -------------------------------------------------
        if(a_conn_mode == nconn::NC_MODE_ERROR)
        {
                if(l_cs)
                {
                        l_cs->cancel_last_timer();
                }
                if(l_t_srvr)
                {
                        ++(l_t_srvr->m_stat.m_clnt_errors);
                }
                return teardown(l_t_srvr, l_cs, l_nconn);
        }
        // -------------------------------------------------
        // TIMEOUT
        // -------------------------------------------------
        if(a_conn_mode == nconn::NC_MODE_TIMEOUT)
        {
                if(l_t_srvr)
                {
                        ++(l_t_srvr->m_stat.m_clnt_idle_killed);
                        ++(l_t_srvr->m_stat.m_clnt_errors);
                }
                return teardown(l_t_srvr, l_cs, l_nconn);
        }
        // -------------------------------------------------
        // TODO unknown conn mode???
        // -------------------------------------------------
        if((a_conn_mode != nconn::NC_MODE_READ) &&
           (a_conn_mode != nconn::NC_MODE_WRITE))
        {
                TRC_ERROR("unknown a_conn_mode: %d\n", a_conn_mode);
                return HLX_STATUS_ERROR;
        }
        // -------------------------------------------------
        // TODO -only for writeable or timeout ???
        // -------------------------------------------------
        //if(l_nconn->is_free())
        //{
        //        return HLX_STATUS_OK;
        //}
        if(l_cs)
        {
                l_cs->cancel_last_timer();
        }
        uint32_t l_timeout_ms = 0;
        if(l_t_srvr)
        {
                l_timeout_ms = l_t_srvr->get_timeout_ms();
        }
        // -------------------------------------------------
        // in/out q's
        // -------------------------------------------------
        nbq *l_in_q = NULL;
        nbq *l_out_q = NULL;
        if(l_cs)
        {
                l_in_q = l_cs->m_in_q;
                // -----------------------------------------
                // TODO Make t_srvr fx
                // -----------------------------------------
                if(!l_cs->m_out_q && l_t_srvr)
                {
                        l_out_q = l_t_srvr->m_orphan_out_q;
                        l_out_q->reset_write();
                }
                else
                {
                        l_out_q = l_cs->m_out_q;
                }
        }
        else
        {
                if(l_t_srvr)
                {
                        l_in_q = l_t_srvr->m_orphan_in_q;
                        l_out_q = l_t_srvr->m_orphan_out_q;
                }
        }

        // -----------------------------------------------------------
        // conn loop
        // -----------------------------------------------------------
        int32_t l_s = HLX_STATUS_OK;
        do {
                bool l_shutdown = false;
                // ---------------------------------------------------
                // Special handling for files
                // ---------------------------------------------------
                if((a_conn_mode == nconn::NC_MODE_WRITE) &&
                   (!l_nconn->is_accepting()) &&
                   l_cs &&
                   l_cs->m_ups &&
                   !l_cs->m_ups->ups_done() &&
                   (l_cs->m_ups->get_type() == file_u::S_UPS_TYPE_FILE) &&
                   (l_cs->m_out_q && !(l_cs->m_out_q->read_avail())))
                {
                        l_cs->m_ups->ups_read(32768);
                }

                l_s = l_nconn->nc_run_state_machine(a_conn_mode, l_in_q, l_out_q);
                //NDBG_PRINT("%snc_run_state_machine%s l_s:  %d\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, l_s);
                if(l_s > 0)
                {
                        if(a_conn_mode == nconn::NC_MODE_READ)
                        {
                                if(l_t_srvr)
                                {
                                        l_t_srvr->m_stat.m_clnt_bytes_read += l_s;
                                }
                                if(l_cs)
                                {
                                        l_cs->m_access_info.m_bytes_in += l_s;
                                }
                        }
                        else if(a_conn_mode == nconn::NC_MODE_WRITE)
                        {
                                if(l_t_srvr)
                                {
                                        l_t_srvr->m_stat.m_clnt_bytes_written += l_s;
                                }
                                if(l_cs)
                                {
                                        l_cs->m_access_info.m_bytes_out += l_s;
                                }
                        }
                }
                if(!l_cs ||
                   (l_s == nconn::NC_STATUS_EOF) ||
                   (l_s == nconn::NC_STATUS_ERROR) ||
                   l_nconn->is_done())
                {
                        goto check_conn_status;
                }
                // ---------------------------------------------------
                // READABLE
                // ---------------------------------------------------
                if(a_conn_mode == nconn::NC_MODE_READ)
                {
                        // -----------------------------------
                        // send expect response -if signalled
                        // -----------------------------------
                        if(l_cs->m_rqst && l_cs->m_rqst->m_expect)
                        {
                                nbq l_nbq(64);
                                const char l_exp_reply[] = "HTTP/1.1 100 Continue\r\n\r\n";
                                l_nbq.write(l_exp_reply, sizeof(l_exp_reply));
                                l_nconn->nc_write(&l_nbq);
                                l_cs->m_rqst->m_expect = false;
                        }
                        // -------------------------------------------
                        // rqst complete
                        // -------------------------------------------
                        if((l_cs->m_rqst &&
                            l_cs->m_rqst->m_complete))
                        {
                                // Display...
                                if(l_cs->m_rqst_resp_logging && l_cs->m_rqst)
                                {
                                        if(l_cs->m_rqst_resp_logging_color) TRC_OUTPUT("%s", ANSI_COLOR_FG_CYAN);
                                        l_cs->m_rqst->show();
                                        if(l_cs->m_rqst_resp_logging_color) TRC_OUTPUT("%s", ANSI_COLOR_OFF);
                                }
                                if(l_t_srvr)
                                {
                                        ++l_t_srvr->m_stat.m_clnt_reqs;
                                }
                                if(l_cs->handle_req() != HLX_STATUS_OK)
                                {
                                        // Modified connection status
                                        l_s = nconn::NC_STATUS_ERROR;
                                        goto check_conn_status;
                                }
                                if(l_cs->m_rqst)
                                {
                                        bool l_ka = l_cs->m_rqst->m_supports_keep_alives;
                                        l_cs->m_rqst->init(true);
                                        l_cs->m_rqst->m_supports_keep_alives = l_ka;
                                }
                                if(l_cs->m_in_q)
                                {
                                        l_cs->m_in_q->reset_write();
                                }
                                if(l_t_srvr->dequeue_clnt_session_writeable())
                                {
                                        l_s = run_state_machine(a_data, nconn::NC_MODE_WRITE);
                                        if(!l_s != HLX_STATUS_OK)
                                        {
                                                // TODO check status
                                        }
                                }
                        }
                }
                // ---------------------------------------------------
                // WRITEABLE
                // ---------------------------------------------------
                else if(a_conn_mode == nconn::NC_MODE_WRITE)
                {
                        if(!l_cs->m_out_q && l_t_srvr)
                        {
                                // Take over orphan -and assign new to orphan out q
                                l_cs->m_out_q = l_t_srvr->m_orphan_out_q;
                                l_t_srvr->m_orphan_out_q = l_cs->m_t_srvr->get_nbq();
                                // TODO check for error...
                        }
                        // -------------------------------------------
                        // check is done
                        // -------------------------------------------
                        bool l_done = false;
                        if(l_cs->m_out_q && !l_cs->m_out_q->read_avail())
                        {
                                if(l_cs->m_ups)
                                {
                                        if(l_cs->m_ups->ups_done())
                                        {
                                                l_shutdown = l_cs->m_ups->get_shutdown();
                                                delete l_cs->m_ups;
                                                l_cs->m_ups = NULL;
                                                l_done = true;
                                        }
                                }
                                else if(!l_nconn->is_accepting())
                                {
                                        l_done = true;
                                }
                        }
                        if(l_done)
                        {
                                l_cs->m_access_info.m_total_time_ms = get_time_ms() - l_cs->m_access_info.m_start_time_ms;
                                if(l_cs->m_resp_done_cb)
                                {
                                        int32_t l_status;
                                        l_status = l_cs->m_resp_done_cb(*l_cs);
                                        if(l_status != 0)
                                        {
                                                // TODO Do nothing???
                                        }
                                        // TODO only resp done cb for clnt's with ups?
                                        l_cs->log_status(0);
                                        l_cs->m_access_info.clear();
                                }
                                l_cs->m_out_q->reset_write();
                                if((l_cs->m_rqst != NULL) &&
                                   (l_cs->m_rqst->m_supports_keep_alives))
                                {
                                        l_cs->m_nconn->nc_set_connected();
                                        // TODO -check status
                                        l_s = nconn::NC_STATUS_BREAK;
                                }
                                else
                                {
                                        l_s = nconn::NC_STATUS_EOF;
                                        goto check_conn_status;
                                }
                        }
                }
check_conn_status:
                if(!l_cs)
                {
                        return teardown(l_t_srvr, NULL, l_nconn);
                }
                if(l_shutdown)
                {
                        l_s = nconn::NC_STATUS_EOF;
                }
                if(l_nconn->is_free())
                {
                        return HLX_STATUS_OK;
                }
                else if(l_nconn->is_done())
                {
                        return teardown(l_t_srvr, l_cs, l_nconn);
                }
                switch(l_s)
                {
                case nconn::NC_STATUS_ERROR:
                {
                        if(l_t_srvr)
                        {
                                ++(l_t_srvr->m_stat.m_clnt_errors);
                        }
                        return teardown(l_t_srvr, l_cs, l_nconn);
                }
                case nconn::NC_STATUS_EOF:
                {
                        return teardown(l_t_srvr, l_cs, l_nconn);
                }
                case nconn::NC_STATUS_OK:
                {
                        goto done;
                }
                case nconn::NC_STATUS_BREAK:
                {
                        goto done;
                }
                }
        } while((l_s != nconn::NC_STATUS_AGAIN) &&
                 (l_t_srvr && l_t_srvr->is_running()));

done:
        // Add idle timeout
        if(l_t_srvr &&
           l_cs &&
           !l_cs->m_timer_obj)
        {
                l_t_srvr->add_timer(l_timeout_ms,
                                   evr_fd_timeout_cb,
                                   l_nconn,
                                   (void **)(&(l_cs->m_timer_obj)));
                // TODO Check status
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void clnt_session::log_status(uint16_t a_status)
{
        if(!m_t_srvr)
        {
                return;
        }
        ++m_t_srvr->m_stat.m_clnt_resp;
        uint16_t l_status = m_access_info.m_resp_status;
        if((l_status >= 100) && (l_status < 200)) {/* TODO log 1xx's? */}
        else if((l_status >= 200) && (l_status < 300)){++m_t_srvr->m_stat.m_clnt_resp_status_2xx;}
        else if((l_status >= 300) && (l_status < 400)){++m_t_srvr->m_stat.m_clnt_resp_status_3xx;}
        else if((l_status >= 400) && (l_status < 500)){++m_t_srvr->m_stat.m_clnt_resp_status_4xx;}
        else if((l_status >= 500) && (l_status < 600)){++m_t_srvr->m_stat.m_clnt_resp_status_5xx;}
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t clnt_session::handle_req(void)
{
        if(!m_lsnr || !m_lsnr->get_url_router())
        {
                return HLX_STATUS_ERROR;
        }
        // -------------------------------------------------
        // access info
        // TODO -this is a lil clunky...
        // -------------------------------------------------
        m_access_info.m_rqst_request = m_rqst->get_url();
        const kv_map_list_t &l_headers = m_rqst->get_headers();
        kv_map_list_t::const_iterator i_h;
        if((i_h = l_headers.find("User-Agent")) != l_headers.end())
        {
                if(!i_h->second.empty())
                {
                        m_access_info.m_rqst_http_user_agent = i_h->second.front();
                }
        }
        if((i_h = l_headers.find("Host")) != l_headers.end())
        {
                if(!i_h->second.empty())
                {
                        m_access_info.m_rqst_host = i_h->second.front();
                }
        }
        if((i_h = l_headers.find("Referer")) != l_headers.end())
        {
                if(!i_h->second.empty())
                {
                        m_access_info.m_rqst_http_referer = i_h->second.front();
                }
        }
        m_access_info.m_rqst_method = m_rqst->get_method_str();
        m_access_info.m_rqst_http_major = m_rqst->m_http_major;
        m_access_info.m_rqst_http_minor = m_rqst->m_http_minor;
        TRC_DEBUG("RQST: %s %s\n", m_rqst->get_method_str(), m_rqst->get_url().c_str());
        url_pmap_t l_pmap;
        h_resp_t l_hdlr_status = H_RESP_NONE;
        rqst_h *l_rqst_h = (rqst_h *)m_lsnr->get_url_router()->find_route(m_rqst->get_url_path(),l_pmap);
        TRC_VERBOSE("l_rqst_h: %p\n", l_rqst_h);
        if(l_rqst_h)
        {
                if(l_rqst_h->get_do_default())
                {
                        l_hdlr_status = l_rqst_h->do_default(*this, *m_rqst, l_pmap);
                }
                else
                {
                        // Method switch
                        switch(m_rqst->m_method)
                        {
                        case HTTP_GET:
                        {
                                l_hdlr_status = l_rqst_h->do_get(*this, *m_rqst, l_pmap);
                                break;
                        }
                        case HTTP_POST:
                        {
                                l_hdlr_status = l_rqst_h->do_post(*this, *m_rqst, l_pmap);
                                break;
                        }
                        case HTTP_PUT:
                        {
                                l_hdlr_status = l_rqst_h->do_put(*this, *m_rqst, l_pmap);
                                break;
                        }
                        case HTTP_DELETE:
                        {
                                l_hdlr_status = l_rqst_h->do_delete(*this, *m_rqst, l_pmap);
                                break;
                        }
                        default:
                        {
                                l_hdlr_status = l_rqst_h->do_default(*this, *m_rqst, l_pmap);
                                break;
                        }
                        }
                }
        }
        else
        {
                rqst_h *l_default_rqst_h = m_lsnr->get_default_route();
                if(l_default_rqst_h)
                {
                        // Default response
                        l_hdlr_status = l_default_rqst_h->do_default(*this, *m_rqst, l_pmap);
                }
                else
                {
                        // Default response
                        l_hdlr_status = s_default_rqst_h.do_get(*this, *m_rqst, l_pmap);
                }
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
        case H_RESP_CLIENT_ERROR:
        {
                l_hdlr_status = s_default_rqst_h.send_bad_request(*this, m_rqst->m_supports_keep_alives);
                break;
        }
        case H_RESP_SERVER_ERROR:
        {
                l_hdlr_status = s_default_rqst_h.send_internal_server_error(*this, m_rqst->m_supports_keep_alives);
                break;
        }
        default:
        {
                break;
        }
        }

        // TODO Handler errors?
        if(l_hdlr_status == H_RESP_SERVER_ERROR)
        {
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: Subrequests
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subr &create_subr(clnt_session &a_clnt_session)
{
        subr *l_subr = new subr();
        l_subr->set_uid(a_clnt_session.m_t_srvr->get_srvr()->get_next_subr_uuid());
        // TODO check exists!!!
        return *l_subr;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subr &create_subr(clnt_session &a_clnt_session, const subr &a_subr)
{
        subr *l_subr = new subr(a_subr);
        l_subr->set_uid(a_clnt_session.m_t_srvr->get_srvr()->get_next_subr_uuid());
        // TODO check exists!!!
        return *l_subr;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t queue_subr(clnt_session &a_clnt_session, subr &a_subr)
{
        a_subr.set_clnt_session(&a_clnt_session);
        a_clnt_session.m_t_srvr->subr_add(a_subr);
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: add subr to t_srvr
//:           WARNING only meant to be run if t_srvr is stopped.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t add_subr_t_srvr(void *a_t_srvr, subr &a_subr)
{
        if(!a_t_srvr)
        {
                return HLX_STATUS_ERROR;
        }
        t_srvr *l_hlx = static_cast<t_srvr *>(a_t_srvr);
        if(l_hlx->is_running())
        {
                return HLX_STATUS_ERROR;
        }
        int32_t l_status = l_hlx->subr_add(a_subr);
        if(l_status != HLX_STATUS_OK)
        {
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: API Responses
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
api_resp &create_api_resp(clnt_session &a_clnt_session)
{
        api_resp *l_api_resp = new api_resp();
        return *l_api_resp;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t queue_api_resp(clnt_session &a_clnt_session, api_resp &a_api_resp)
{
        if(!a_clnt_session.m_t_srvr)
        {
            return HLX_STATUS_ERROR;
        }
        a_clnt_session.m_t_srvr->queue_api_resp(a_api_resp, a_clnt_session);
        delete &a_api_resp;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t queue_resp(clnt_session &a_clnt_session)
{
        if(!a_clnt_session.m_t_srvr)
        {
            return HLX_STATUS_ERROR;
        }
        a_clnt_session.m_t_srvr->queue_output(a_clnt_session);
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t add_timer(clnt_session &a_clnt_session, uint32_t a_ms,
                  timer_cb_t a_cb, void *a_data,
                  void **ao_timer)
{
        if(!a_clnt_session.m_t_srvr)
        {
            return HLX_STATUS_ERROR;
        }
        int32_t l_status;
        l_status = a_clnt_session.m_t_srvr->add_timer(a_ms, a_cb, a_data, ao_timer);
        if(l_status != HLX_STATUS_OK)
        {
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t cancel_timer(clnt_session &a_clnt_session, void *a_timer)
{
        if(!a_clnt_session.m_t_srvr)
        {
            return HLX_STATUS_ERROR;
        }
        int32_t l_status;
        l_status = a_clnt_session.m_t_srvr->cancel_timer(a_timer);
        if(l_status != HLX_STATUS_OK)
        {
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t add_timer(void *a_t_srvr, uint32_t a_ms,
                  timer_cb_t a_cb, void *a_data,
                  void **ao_timer)
{
        if(!a_t_srvr)
        {
                return HLX_STATUS_ERROR;
        }
        t_srvr *l_hlx = static_cast<t_srvr *>(a_t_srvr);
        int32_t l_status;
        l_status = l_hlx->add_timer(a_ms, a_cb, a_data, ao_timer);
        if(l_status != HLX_STATUS_OK)
        {
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t cancel_timer(void *a_t_srvr, void *a_timer)
{
        if(!a_t_srvr)
        {
            return HLX_STATUS_ERROR;
        }
        t_srvr *l_hlx = static_cast<t_srvr *>(a_t_srvr);
        int32_t l_status;
        l_status = l_hlx->cancel_timer(a_timer);
        if(l_status != HLX_STATUS_OK)
        {
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
srvr *get_srvr(clnt_session &a_clnt_session)
{
        if(!a_clnt_session.m_t_srvr)
        {
                TRC_ERROR("a_clnt_session.m_t_srvr == NULL\n");
                return NULL;
        }
        return a_clnt_session.m_t_srvr->get_srvr();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *get_nconn(clnt_session &a_clnt_session)
{
        return a_clnt_session.m_nconn;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const access_info &get_access_info(clnt_session &a_clnt_session)
{
        return a_clnt_session.m_access_info;
}


} //namespace ns_hlx {
