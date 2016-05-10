//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    ups_srvr_session.cc
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
#include "ups_srvr_session.h"
#include "ndebug.h"
#include "t_srvr.h"
#include "nbq.h"
#include "hlx/srvr.h"
#include "hlx/trace.h"
#include "hlx/status.h"
#include "hlx/time_util.h"
#include "hlx/subr.h"

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
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t ups_srvr_session::evr_fd_readable_cb(void *a_data)
{
        if(!a_data)
        {
                return HLX_STATUS_OK;
        }
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_ctx());
        t_srvr *l_t_srvr = static_cast<t_srvr *>(l_nconn->get_ctx());
        ups_srvr_session *l_ups_srvr_session = static_cast<ups_srvr_session *>(l_nconn->get_data());
        if(l_ups_srvr_session &&
           l_ups_srvr_session->m_timer_obj)
        {
                l_t_srvr->cancel_timer(l_ups_srvr_session->m_timer_obj);
                l_ups_srvr_session->m_timer_obj = NULL;
        }
        // Get timeout ms
        uint32_t l_timeout_ms = l_t_srvr->get_timeout_ms();
        if(l_ups_srvr_session &&
           l_ups_srvr_session->m_subr)
        {
                l_timeout_ms = l_ups_srvr_session->m_subr->get_timeout_ms();
        }
        int32_t l_s = HLX_STATUS_OK;
        do {
                // Flipping modes to support TLS connections
                nbq *l_in_q = NULL;
                nbq *l_out_q = NULL;
                if(l_ups_srvr_session)
                {
                        l_in_q = l_ups_srvr_session->m_in_q;
                        l_out_q = l_ups_srvr_session->m_out_q;
                }
                else
                {
                        l_in_q = l_t_srvr->m_orphan_in_q;
                        l_out_q = l_t_srvr->m_orphan_out_q;
                }
                l_s = l_nconn->nc_run_state_machine(nconn::NC_MODE_READ, l_in_q, l_out_q);
                if(l_s > 0)
                {
                        l_t_srvr->m_stat.m_total_bytes_read += l_s;
                }

                if(l_ups_srvr_session)
                {
                        int32_t l_hstatus;
                        l_hstatus = l_ups_srvr_session->run_state_machine(nconn::NC_MODE_READ, l_s);
                        if(l_hstatus != HLX_STATUS_OK)
                        {
                                // Modified connection status
                                l_s = l_hstatus;
                        }
                }
                if(l_nconn->is_free())
                {
                        return HLX_STATUS_OK;
                }
                switch(l_s)
                {
                case nconn::NC_STATUS_BREAK:
                {
                        goto done;
                }
                case nconn::NC_STATUS_IDLE:
                {
                        int32_t l_s;
                        if(l_ups_srvr_session)
                        {
                                l_t_srvr->m_ups_srvr_session_pool.release(l_ups_srvr_session);
                                l_ups_srvr_session = NULL;
                        }
                        l_nconn->set_data(NULL);
                        l_s = l_t_srvr->m_nconn_proxy_pool.add_idle(l_nconn);
                        if(l_s != HLX_STATUS_OK)
                        {
                                return HLX_STATUS_ERROR;
                        }
                        return HLX_STATUS_OK;
                }
                case nconn::NC_STATUS_EOF:
                {
                        l_t_srvr->cleanup_conn(l_ups_srvr_session, l_nconn);
                        // TODO Check return
                        return HLX_STATUS_OK;
                }
                case nconn::NC_STATUS_ERROR:
                {
                        ++(l_t_srvr->m_stat.m_total_errors);
                        l_t_srvr->cleanup_conn(l_ups_srvr_session, l_nconn);
                        // TODO Check return
                        return HLX_STATUS_ERROR;
                }
                }
                if(!l_ups_srvr_session)
                {
                        l_t_srvr->cleanup_conn(l_ups_srvr_session, l_nconn);
                        // TODO Check return
                        return HLX_STATUS_OK;
                }
        } while((l_s != nconn::NC_STATUS_AGAIN) &&
                (l_t_srvr->is_running()));
done:
        // Add idle timeout
        if(l_ups_srvr_session &&
           !l_ups_srvr_session->m_timer_obj)
        {
                l_t_srvr->add_timer(l_timeout_ms,
                                   evr_fd_timeout_cb,
                                   l_nconn,
                                   (void **)(&(l_ups_srvr_session->m_timer_obj)));
                // TODO Check status
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t ups_srvr_session::evr_fd_writeable_cb(void *a_data)
{
        if(!a_data)
        {
                //NDBG_PRINT("a_data == NULL\n");
                return HLX_STATUS_OK;
        }
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_ctx());
        t_srvr *l_t_srvr = static_cast<t_srvr *>(l_nconn->get_ctx());
        ups_srvr_session *l_ups_srvr_session = static_cast<ups_srvr_session *>(l_nconn->get_data());
        //NDBG_PRINT("%sWRITEABLE%s LABEL: %s CLNT: %p\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF,
        //                l_nconn->get_label().c_str(), l_ups_srvr_session);
        // Cancel last timer
        if(l_ups_srvr_session &&
           l_ups_srvr_session->m_timer_obj)
        {
                l_t_srvr->cancel_timer(l_ups_srvr_session->m_timer_obj);
                l_ups_srvr_session->m_timer_obj = NULL;
        }
        // Get timeout ms
        uint32_t l_timeout_ms = l_t_srvr->get_timeout_ms();
        if(l_ups_srvr_session &&
           l_ups_srvr_session->m_subr)
        {
                l_timeout_ms = l_ups_srvr_session->m_subr->get_timeout_ms();
        }
        nbq *l_in_q = NULL;
        nbq *l_out_q = NULL;
        if(l_ups_srvr_session)
        {
                l_in_q = l_ups_srvr_session->m_in_q;
                l_out_q = l_ups_srvr_session->m_out_q;
        }
        else
        {
                l_in_q = l_t_srvr->m_orphan_in_q;
                l_out_q = l_t_srvr->m_orphan_out_q;
        }
        int32_t l_s = HLX_STATUS_OK;
        do {
                l_s = l_nconn->nc_run_state_machine(nconn::NC_MODE_WRITE, l_in_q, l_out_q);
                if(l_s > 0)
                {
                        l_t_srvr->m_stat.m_total_bytes_written += l_s;
                }
                if(l_ups_srvr_session)
                {
                        int32_t l_hstatus;
                        l_hstatus = l_ups_srvr_session->run_state_machine(nconn::NC_MODE_WRITE, l_s);
                        if(l_hstatus != HLX_STATUS_OK)
                        {
                                // Modified connection status
                                l_s = l_hstatus;
                        }
                }
                if(l_nconn->is_free())
                {
                        return HLX_STATUS_OK;
                }
                switch(l_s)
                {
                case nconn::NC_STATUS_BREAK:
                {
                        goto done;
                }
                case nconn::NC_STATUS_EOF:
                {
                        l_t_srvr->cleanup_conn(l_ups_srvr_session, l_nconn);
                        // TODO Check return
                        return HLX_STATUS_OK;
                }
                case nconn::NC_STATUS_ERROR:
                {
                        ++(l_t_srvr->m_stat.m_total_errors);
                        l_t_srvr->cleanup_conn(l_ups_srvr_session, l_nconn);
                        // TODO Check return
                        return HLX_STATUS_ERROR;
                }
                }

                if(!l_ups_srvr_session)
                {
                        l_t_srvr->cleanup_conn(l_ups_srvr_session, l_nconn);
                        // TODO Check return
                        return HLX_STATUS_OK;
                }

        } while((l_s != nconn::NC_STATUS_AGAIN) &&
                 (l_t_srvr->is_running()));
done:
        // Add idle timeout
        if(l_ups_srvr_session &&
           !l_ups_srvr_session->m_timer_obj)
        {
                l_t_srvr->add_timer(l_timeout_ms,
                                   evr_fd_timeout_cb,
                                   l_nconn,
                                   (void **)(&(l_ups_srvr_session->m_timer_obj)));
                // TODO Check status
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t ups_srvr_session::evr_fd_error_cb(void *a_data)
{
        CHECK_FOR_NULL_ERROR(a_data);
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_ctx());
        t_srvr *l_t_srvr = static_cast<t_srvr *>(l_nconn->get_ctx());
        ups_srvr_session *l_ups_srvr_session = static_cast<ups_srvr_session *>(l_nconn->get_data());
        ++l_t_srvr->m_stat.m_total_errors;
        if(l_ups_srvr_session)
        {
                int32_t l_hstatus;
                l_hstatus = l_ups_srvr_session->run_state_machine(nconn::NC_MODE_ERROR, 0);
                if(l_hstatus != HLX_STATUS_OK)
                {
                        // TODO???
                }
        }
        int32_t l_s;
        l_s = l_t_srvr->cleanup_conn(l_ups_srvr_session, l_nconn);
        if(l_s != HLX_STATUS_OK)
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
int32_t ups_srvr_session::evr_fd_timeout_cb(void *a_ctx, void *a_data)
{
        CHECK_FOR_NULL_ERROR(a_data);
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_ctx());
        t_srvr *l_t_srvr = static_cast<t_srvr *>(l_nconn->get_ctx());
        ups_srvr_session *l_ups_srvr_session = static_cast<ups_srvr_session *>(l_nconn->get_data());
        if(l_nconn->is_free())
        {
                return HLX_STATUS_OK;
        }
        ++(l_t_srvr->m_stat.m_total_errors);
        if(l_ups_srvr_session)
        {
                int32_t l_hstatus;
                l_hstatus = l_ups_srvr_session->run_state_machine(nconn::NC_MODE_TIMEOUT, 0);
                if(l_hstatus != HLX_STATUS_OK)
                {
                        // TODO???
                }
        }
        l_t_srvr->cleanup_conn(l_ups_srvr_session, l_nconn);
        // TODO Check return
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ups_srvr_session::ups_srvr_session(void):
        m_nconn(NULL),
        m_t_srvr(NULL),
        m_timer_obj(NULL),
        m_resp(NULL),
        m_rqst_resp_logging(false),
        m_rqst_resp_logging_color(false),
        m_in_q(NULL),
        m_out_q(NULL),
        m_in_q_detached(false),
        m_subr(NULL),
        m_idx(0)
{}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void ups_srvr_session::clear(void)
{
        m_nconn = NULL;
        m_t_srvr = NULL;
        m_timer_obj = NULL;
        m_resp = NULL;
        m_rqst_resp_logging = false;
        m_rqst_resp_logging_color = false;
        m_in_q = NULL;
        m_out_q = NULL;
        m_subr = NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t ups_srvr_session::run_state_machine(nconn::mode_t a_conn_mode, int32_t a_conn_status)
{
        if(!m_subr)
        {
                TRC_ERROR("no subrequest associated with upstream ups_srvr_session mode: %d status: %d\n",
                                a_conn_mode, a_conn_status);
                return nconn::NC_STATUS_ERROR;
        }

        switch(a_conn_mode)
        {
        // -----------------------------------------------------------
        // Readable
        // -----------------------------------------------------------
        case nconn::NC_MODE_READ:
        {
                switch(a_conn_status)
                {
                case nconn::NC_STATUS_EOF:
                {
                        // Connect only && done --early exit...
                        if(m_subr->get_connect_only())
                        {
                                if(m_resp)
                                {
                                        m_resp->set_status(HTTP_STATUS_OK);
                                }
                                m_t_srvr->add_stat_to_agg(m_nconn->get_stats(), HTTP_STATUS_OK);
                        }
                        m_subr->bump_num_completed();
                        subr_complete();
                        return HLX_STATUS_OK;
                }
                case nconn::NC_STATUS_ERROR:
                {
                        if(m_resp)
                        {
                                m_resp->set_status(HTTP_STATUS_BAD_GATEWAY);
                        }
                        int32_t l_s;
                        l_s = subr_error(HTTP_STATUS_BAD_GATEWAY);
                        if(l_s != HLX_STATUS_OK)
                        {
                                return HLX_STATUS_ERROR;
                        }
                        return HLX_STATUS_OK;
                }
                default:
                {
                        if(m_subr->get_ups() && (a_conn_status > 0))
                        {
                                ssize_t l_s;
                                l_s = m_subr->get_ups()->ups_read((size_t)a_conn_status);
                                if(l_s < 0)
                                {
                                        TRC_ERROR("performing ups_read -a_conn_status: %d l_s: %ld\n", a_conn_status, l_s);
                                }
                        }
                        // Handle completion
                        if((m_nconn && m_nconn->is_done()) ||
                           (m_resp &&
                            m_resp->m_complete))
                        {
                                if(m_rqst_resp_logging && m_resp)
                                {
                                        if(m_rqst_resp_logging_color) TRC_OUTPUT("%s", ANSI_COLOR_FG_CYAN);
                                        m_resp->show();
                                        if(m_rqst_resp_logging_color) TRC_OUTPUT("%s", ANSI_COLOR_OFF);
                                }
                                // Get request time
                                if(m_nconn && m_nconn->get_collect_stats_flag())
                                {
                                        m_nconn->set_stat_tt_completion_us(get_delta_time_us(m_nconn->get_connect_start_time_us()));
                                }

                                if(m_nconn && m_resp)
                                {
                                        m_t_srvr->add_stat_to_agg(m_nconn->get_stats(), m_resp->get_status());
                                }
                                m_subr->bump_num_completed();
                                bool l_hmsg_keep_alive = false;
                                if(m_resp)
                                {
                                        l_hmsg_keep_alive = m_resp->m_supports_keep_alives;
                                }
                                bool l_nconn_can_reuse = false;
                                if(m_nconn)
                                {
                                        l_nconn_can_reuse = m_nconn->can_reuse();
                                }
                                bool l_keepalive = m_subr->get_keepalive();
                                bool l_detach_resp = m_subr->get_detach_resp();
                                bool l_complete = subr_complete();
                                if(l_complete ||
                                   !l_nconn_can_reuse ||
                                   !l_keepalive ||
                                   !l_hmsg_keep_alive)
                                {
                                        return nconn::NC_STATUS_EOF;
                                }
                                // Give back rqst + in q
                                if(m_out_q)
                                {
                                        m_t_srvr->release_nbq(m_out_q);
                                        m_out_q = NULL;
                                }
                                if(!l_detach_resp)
                                {
                                        m_t_srvr->release_resp(m_resp);
                                        m_resp = NULL;
                                        m_t_srvr->release_nbq(m_in_q);
                                        m_in_q = NULL;
                                }
                                return nconn::NC_STATUS_IDLE;
                        }
                        return HLX_STATUS_OK;
                }
                }
        }
        // -----------------------------------------------------------
        // Writeable
        // -----------------------------------------------------------
        case nconn::NC_MODE_WRITE:
        {
                switch(a_conn_status)
                {
                case nconn::NC_STATUS_ERROR:
                {
                        subr::error_cb_t l_error_cb = m_subr->get_error_cb();
                        if(l_error_cb)
                        {
                                const char *l_err_str = "unknown";
                                if(m_nconn)
                                {
                                        l_err_str = m_nconn->get_last_error().c_str();
                                }
                                l_error_cb(*m_subr, m_nconn, HTTP_STATUS_INTERNAL_SERVER_ERROR, l_err_str);
                                // TODO check status
                        }
                        return HLX_STATUS_OK;
                }
                case nconn::NC_STATUS_EOF:
                {
                        m_subr->bump_num_completed();
                        bool l_complete = subr_complete();
                        if(l_complete)
                        {
                                return HLX_STATUS_OK;
                        }
                        //NDBG_PRINT("Cleanup EOF\n");
                        return HLX_STATUS_OK;
                }
                default:
                {
                        if(a_conn_status == nconn::NC_STATUS_OK)
                        {
                                return nconn::NC_STATUS_BREAK;
                        }
                        return HLX_STATUS_OK;
                }
                }
        }
        // -----------------------------------------------------------
        // Timeout
        // -----------------------------------------------------------
        case nconn::NC_MODE_TIMEOUT:
        {
                m_t_srvr->bump_num_ups_idle_killed();
                if(m_subr)
                {
                        int32_t l_s;
                        l_s = subr_error(HTTP_STATUS_GATEWAY_TIMEOUT);
                        if(l_s != HLX_STATUS_OK)
                        {
                                return HLX_STATUS_ERROR;
                        }
                }
                return HLX_STATUS_OK;
        }
        // -----------------------------------------------------------
        // Error
        // -----------------------------------------------------------
        case nconn::NC_MODE_ERROR:
        {
                int32_t l_s;
                l_s = subr_error(HTTP_STATUS_BAD_GATEWAY);
                if(l_s != HLX_STATUS_OK)
                {
                        return HLX_STATUS_ERROR;
                }
                return HLX_STATUS_OK;
        }
        // -----------------------------------------------------------
        // Default
        // -----------------------------------------------------------
        default:
        {
                return HLX_STATUS_OK;
        }
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
bool ups_srvr_session::subr_complete(void)
{
        if(!m_resp || !m_subr || !m_nconn)
        {
                return true;
        }
        bool l_complete = false;
        if(m_subr->get_kind() != subr::SUBR_KIND_DUPE)
        {
                m_subr->set_end_time_ms(get_time_ms());
        }
        // Get vars -completion -can delete subr object
        bool l_connect_only = m_subr->get_connect_only();
        bool l_detach_resp = m_subr->get_detach_resp();
        subr::completion_cb_t l_completion_cb = m_subr->get_completion_cb();
        // Call completion handler
        if(l_completion_cb)
        {
                l_completion_cb(*m_subr, *m_nconn, *m_resp);
        }
        // Connect only
        if(l_connect_only)
        {
                l_complete = true;
        }
        if(l_detach_resp)
        {
                m_subr = NULL;
                m_resp = NULL;
                m_in_q = NULL;
        }
        return l_complete;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t ups_srvr_session::subr_error(http_status_t a_status)
{
        if(!m_subr)
        {
                return HLX_STATUS_ERROR;
        }
        m_subr->bump_num_completed();
        if(m_subr->get_kind() != subr::SUBR_KIND_DUPE)
        {
                m_subr->set_end_time_ms(get_time_ms());
        }
        if(m_nconn && m_nconn->get_collect_stats_flag())
        {
                m_nconn->set_stat_tt_completion_us(get_delta_time_us(m_nconn->get_connect_start_time_us()));
                m_nconn->reset_stats();
        }
        bool l_detach_resp = m_subr->get_detach_resp();
        subr::error_cb_t l_error_cb = m_subr->get_error_cb();
        if(l_error_cb)
        {
                const char *l_err_str = NULL;
                if(m_nconn)
                {
                        l_err_str = m_nconn->get_last_error().c_str();
                }
                l_error_cb(*(m_subr), m_nconn, a_status, l_err_str);
                // TODO Check status...
        }
        if(l_detach_resp)
        {
                m_subr = NULL;
        }
        return HLX_STATUS_OK;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t cancel_timer(ups_srvr_session &a_ups_srvr_session, void *a_timer)
{
        if(!a_ups_srvr_session.m_t_srvr)
        {
            return HLX_STATUS_ERROR;
        }
        int32_t l_s;
        l_s = a_ups_srvr_session.m_t_srvr->cancel_timer(a_timer);
        if(l_s != HLX_STATUS_OK)
        {
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}

} //namespace ns_hlx {
