//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
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
#include "ndebug.h"
#include "t_srvr.h"
#include "hlx/ups_srvr_session.h"
#include "hlx/nbq.h"
#include "hlx/srvr.h"
#include "hlx/trace.h"
#include "hlx/status.h"
#include "hlx/time_util.h"
#include "hlx/subr.h"
#include "hlx/proxy_h.h"

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
        m_idx(0),
        m_last_active_ms(0),
        m_timeout_ms(10000)
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
int32_t ups_srvr_session::evr_fd_readable_cb(void *a_data)
{
        return run_state_machine(a_data, EVR_MODE_READ);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t ups_srvr_session::evr_fd_writeable_cb(void *a_data)
{
        return run_state_machine(a_data, EVR_MODE_WRITE);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t ups_srvr_session::evr_fd_error_cb(void *a_data)
{
        return run_state_machine(a_data, EVR_MODE_ERROR);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t ups_srvr_session::evr_fd_timeout_cb(void *a_ctx, void *a_data)
{
        return run_state_machine(a_data, EVR_MODE_TIMEOUT);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t ups_srvr_session::teardown(http_status_t a_status)
{
        if(m_subr &&
           m_subr->get_ups() &&
          (m_subr->get_ups()->get_type() == proxy_u::S_UPS_TYPE_PROXY))
        {
                m_subr->get_ups()->set_shutdown();
                m_subr->get_ups()->set_ups_done();
        }
        if(a_status != HTTP_STATUS_OK)
        {
                if(m_resp)
                {
                        m_resp->set_status(a_status);
                }
                int32_t l_s;
                l_s = subr_error(a_status);
                if(l_s != HLX_STATUS_OK)
                {
                        TRC_ERROR("performing subr_error\n");
                }
        }
        if(m_t_srvr)
        {
                int32_t l_s;
                l_s = m_t_srvr->cleanup_srvr_session(this, m_nconn);
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
int32_t ups_srvr_session::run_state_machine(void *a_data, evr_mode_t a_conn_mode)
{
        //NDBG_PRINT("RUN a_conn_mode: %d a_data: %p\n", a_conn_mode, a_data);
        //CHECK_FOR_NULL_ERROR(a_data);
        // TODO -return OK for a_data == NULL
        if(!a_data)
        {
                return HLX_STATUS_OK;
        }
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_ctx());
        t_srvr *l_t_srvr = static_cast<t_srvr *>(l_nconn->get_ctx());
        ups_srvr_session *l_uss = static_cast<ups_srvr_session *>(l_nconn->get_data());

        // -------------------------------------------------
        // ERROR
        // -------------------------------------------------
        if(a_conn_mode == EVR_MODE_ERROR)
        {
                // ignore errors for free connections
                if(l_nconn->is_free())
                {
                        TRC_ERROR("call back for free connection\n");
                        return HLX_STATUS_OK;
                }
                if(l_t_srvr)
                {
                        ++l_t_srvr->m_stat.m_upsv_errors;
                }
                if(l_uss)
                {
                        l_uss->cancel_timer(l_uss->m_timer_obj);
                        // TODO Check status
                        l_uss->m_timer_obj = NULL;
                        return l_uss->teardown(HTTP_STATUS_BAD_GATEWAY);
                }
                else
                {
                        if(l_t_srvr)
                        {
                                return l_t_srvr->cleanup_srvr_session(NULL, l_nconn);
                        }
                        else
                        {
                                // TODO log error???
                                return HLX_STATUS_ERROR;
                        }
                }
        }
        // -------------------------------------------------
        // TIMEOUT
        // -------------------------------------------------
        else if(a_conn_mode == EVR_MODE_TIMEOUT)
        {
                // ignore timeout for free connections
                if(l_nconn->is_free())
                {
                        TRC_ERROR("call back for free connection\n");
                        return HLX_STATUS_OK;
                }
                // calc time since last active
                if(l_uss && l_t_srvr)
                {
                        // ---------------------------------
                        // timeout
                        // ---------------------------------
                        uint64_t l_ct_ms = get_time_ms();
                        if(((uint32_t)(l_ct_ms - l_uss->get_last_active_ms())) >= l_uss->get_timeout_ms())
                        {
                                ++(l_t_srvr->m_stat.m_upsv_errors);
                                ++(l_t_srvr->m_stat.m_upsv_idle_killed);
                                return l_uss->teardown(HTTP_STATUS_GATEWAY_TIMEOUT);
                        }
                        // ---------------------------------
                        // active -create new timer with
                        // delta time
                        // ---------------------------------
                        else
                        {
                                uint64_t l_d_time = (uint32_t)(l_uss->get_timeout_ms() - (l_ct_ms - l_uss->get_last_active_ms()));
                                l_t_srvr->add_timer(l_d_time,
                                                    ups_srvr_session::evr_fd_timeout_cb,
                                                    l_nconn,
                                                    (void **)(&(l_uss->m_timer_obj)));
                                // TODO check status
                                return HLX_STATUS_OK;
                        }
                }
                else
                {
                        TRC_ERROR("a_conn_mode[%d] ups_srvr_session[%p] || t_srvr[%p] == NULL\n",
                                        a_conn_mode,
                                        l_uss,
                                        l_t_srvr);
                        return HLX_STATUS_ERROR;
                }
        }
        else if(a_conn_mode == EVR_MODE_READ)
        {
                // ignore readable for free connections
                if(l_nconn->is_free())
                {
                        TRC_ERROR("call back for free connection\n");
                        return HLX_STATUS_OK;
                }
        }
        // -------------------------------------------------
        // TODO unknown conn mode???
        // -------------------------------------------------
        else if((a_conn_mode != EVR_MODE_READ) &&
                (a_conn_mode != EVR_MODE_WRITE))
        {
                TRC_ERROR("unknown a_conn_mode: %d\n", a_conn_mode);
                return HLX_STATUS_OK;
        }

        // set last active
        if(l_uss)
        {
                l_uss->set_last_active_ms(get_time_ms());
        }

        // -------------------------------------------------
        // in/out q's
        // -------------------------------------------------
        nbq *l_in_q = NULL;
        nbq *l_out_q = NULL;
        if(l_uss)
        {
                l_in_q = l_uss->m_in_q;
                l_out_q = l_uss->m_out_q;
        }
        else
        {
                l_in_q = l_t_srvr->m_orphan_in_q;
                l_out_q = l_t_srvr->m_orphan_out_q;
        }
        // -------------------------------------------------
        // conn loop
        // -------------------------------------------------
        bool l_idle = false;
        int32_t l_s = HLX_STATUS_OK;
        do {
                uint32_t l_read = 0;
                uint32_t l_written = 0;
                l_s = l_nconn->nc_run_state_machine(a_conn_mode, l_in_q, l_read, l_out_q, l_written);
                //NDBG_PRINT("l_nconn->nc_run_state_machine(%d): status: %d\n", a_conn_mode, l_s);
                if(l_t_srvr)
                {
                        l_t_srvr->m_stat.m_upsv_bytes_read += l_read;
                        l_t_srvr->m_stat.m_upsv_bytes_written += l_written;
                }
                if(!l_uss ||
                   !l_uss->m_subr ||
                   (l_s == nconn::NC_STATUS_EOF) ||
                   (l_s == nconn::NC_STATUS_ERROR) ||
                   l_nconn->is_done())
                {
                        goto check_conn_status;
                }
                // -----------------------------------------
                // READABLE
                // -----------------------------------------
                if(a_conn_mode == EVR_MODE_READ)
                {
                        if(l_uss->m_subr->get_ups() && (l_read > 0))
                        {
                                ssize_t l_size;
                                l_size = l_uss->m_subr->get_ups()->ups_read((size_t)l_read);
                                if(l_size < 0)
                                {
                                        TRC_ERROR("performing ups_read -a_conn_status: %d l_size: %zd\n", l_read, l_size);
                                }
                        }
                        // -----------------------------------------
                        // Handle completion
                        // -----------------------------------------
                        if(l_uss->m_resp &&
                           l_uss->m_resp->m_complete)
                        {
                                // Cancel timer
                                l_uss->cancel_timer(l_uss->m_timer_obj);
                                // TODO Check status
                                l_uss->m_timer_obj = NULL;

                                if(l_uss->m_rqst_resp_logging && l_uss->m_resp)
                                {
                                        if(l_uss->m_rqst_resp_logging_color) TRC_OUTPUT("%s", ANSI_COLOR_FG_CYAN);
                                        l_uss->m_resp->show();
                                        if(l_uss->m_rqst_resp_logging_color) TRC_OUTPUT("%s", ANSI_COLOR_OFF);
                                }
                                // Get request time
                                if(l_nconn->get_collect_stats_flag())
                                {
                                        l_nconn->set_stat_tt_completion_us(get_delta_time_us(l_nconn->get_connect_start_time_us()));
                                }
                                if(l_uss->m_resp && l_t_srvr)
                                {
                                        l_t_srvr->add_stat_to_agg(l_nconn->get_stats(), l_uss->m_resp->get_status());
                                }
                                bool l_hmsg_keep_alive = false;
                                if(l_uss->m_resp)
                                {
                                        l_hmsg_keep_alive = l_uss->m_resp->m_supports_keep_alives;
                                }
                                bool l_nconn_can_reuse = l_nconn->can_reuse();
                                bool l_keepalive = l_uss->m_subr->get_keepalive();
                                bool l_detach_resp = l_uss->m_subr->get_detach_resp();
                                bool l_complete = l_uss->subr_complete();
                                if(l_complete ||
                                   !l_nconn_can_reuse ||
                                   !l_keepalive ||
                                   !l_hmsg_keep_alive)
                                {
                                        l_s = nconn::NC_STATUS_EOF;
                                        goto check_conn_status;
                                }
                                // Give back rqst + in q
                                if(l_t_srvr)
                                {
                                        l_t_srvr->release_nbq(l_uss->m_out_q);
                                        l_uss->m_out_q = NULL;
                                        if(!l_detach_resp)
                                        {
                                                if(l_uss->m_resp)
                                                {
                                                        l_t_srvr->release_resp(l_uss->m_resp);
                                                        l_uss->m_resp = NULL;
                                                }
                                                if(!l_uss->m_in_q_detached)
                                                {
                                                        l_t_srvr->release_nbq(l_uss->m_in_q);
                                                        l_uss->m_in_q = NULL;
                                                }
                                        }
                                }
                                l_idle = true;
                        }
                }
                // -----------------------------------------
                // STATUS_OK
                // -----------------------------------------
                else if(l_s == nconn::NC_STATUS_OK)
                {
                        l_s = nconn::NC_STATUS_BREAK;
                        goto check_conn_status;
                }
check_conn_status:
                if(l_nconn->is_free())
                {
                        return HLX_STATUS_OK;
                }
                if(!l_uss)
                {
                        TRC_ERROR("no ups_srvr_session associated with nconn mode: %d\n", a_conn_mode);
                        if(l_t_srvr)
                        {
                                return l_t_srvr->cleanup_srvr_session(NULL, l_nconn);
                        }
                        else
                        {
                                // TODO log error???
                                return HLX_STATUS_ERROR;
                        }
                }
                switch(l_s)
                {
                case nconn::NC_STATUS_BREAK:
                {
                        goto done;
                }
                case nconn::NC_STATUS_EOF:
                {
                        // Connect only && done --early exit...
                        if(l_uss->m_subr &&
                           l_uss->m_subr->get_connect_only())
                        {
                                if(l_uss->m_resp)
                                {
                                        l_uss->m_resp->set_status(HTTP_STATUS_OK);
                                }
                                if(l_t_srvr)
                                {
                                        l_t_srvr->add_stat_to_agg(l_nconn->get_stats(), HTTP_STATUS_OK);
                                }
                        }
                        if(l_uss->m_subr)
                        {
                                l_uss->subr_complete();
                        }
                        return l_uss->teardown(HTTP_STATUS_OK);
                }
                case nconn::NC_STATUS_ERROR:
                {
                        if(l_t_srvr)
                        {
                                ++(l_t_srvr->m_stat.m_upsv_errors);
                        }
                        return l_uss->teardown(HTTP_STATUS_BAD_GATEWAY);
                }
                default:
                {
                        break;
                }
                }
                if(l_nconn->is_done())
                {
                        return l_uss->teardown(HTTP_STATUS_OK);
                }
        } while((l_s != nconn::NC_STATUS_AGAIN) &&
                (l_t_srvr->is_running()));

        if((l_s == nconn::NC_STATUS_AGAIN) &&
           l_idle)
        {
                if(l_uss && l_t_srvr)
                {
                        l_t_srvr->m_ups_srvr_session_pool.release(l_uss);
                        l_uss = NULL;
                }
                l_nconn->set_data(NULL);
                if(l_t_srvr)
                {
                        int32_t l_status;
                        l_status = l_t_srvr->m_nconn_proxy_pool.add_idle(l_nconn);
                        if(l_status != HLX_STATUS_OK)
                        {
                                TRC_ERROR("performing m_nconn_proxy_pool.add_idle(%p)\n", l_nconn);
                                return HLX_STATUS_ERROR;
                        }
                }
                return HLX_STATUS_OK;
        }
done:
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void ups_srvr_session::subr_log_status(uint16_t a_status)
{
        if(!m_t_srvr)
        {
                return;
        }
        ++(m_t_srvr->m_stat.m_upsv_resp);
        uint16_t l_status;
        if(m_resp)
        {
                l_status = m_resp->get_status();
        }
        else if(a_status)
        {
                l_status = a_status;
        }
        else
        {
                l_status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
        }
        if((l_status >= 100) && (l_status < 200)) {/* TODO log 1xx's? */}
        else if((l_status >= 200) && (l_status < 300)){++m_t_srvr->m_stat.m_upsv_resp_status_2xx;}
        else if((l_status >= 300) && (l_status < 400)){++m_t_srvr->m_stat.m_upsv_resp_status_3xx;}
        else if((l_status >= 400) && (l_status < 500)){++m_t_srvr->m_stat.m_upsv_resp_status_4xx;}
        else if((l_status >= 500) && (l_status < 600)){++m_t_srvr->m_stat.m_upsv_resp_status_5xx;}
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool ups_srvr_session::subr_complete(void)
{
        subr_log_status(0);
        if(!m_resp || !m_subr || !m_nconn)
        {
                return true;
        }
        bool l_complete = false;
        m_subr->set_end_time_ms(get_time_ms());
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
        subr_log_status(a_status);
        if(!m_subr)
        {
                return HLX_STATUS_ERROR;
        }
        m_subr->set_end_time_ms(get_time_ms());
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
uint32_t ups_srvr_session::get_timeout_ms(void)
{
        return m_timeout_ms;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void ups_srvr_session::set_timeout_ms(uint32_t a_t_ms)
{
        m_timeout_ms = a_t_ms;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t ups_srvr_session::get_last_active_ms(void)
{
        return m_last_active_ms;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void ups_srvr_session::set_last_active_ms(uint64_t a_time_ms)
{
        m_last_active_ms = a_time_ms;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t ups_srvr_session::cancel_timer(void *a_timer)
{
        if(!m_t_srvr)
        {
            return HLX_STATUS_ERROR;
        }
        int32_t l_s;
        l_s = m_t_srvr->cancel_timer(a_timer);
        if(l_s != HLX_STATUS_OK)
        {
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}

} //namespace ns_hlx {
