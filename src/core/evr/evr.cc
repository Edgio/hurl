//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    evr.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    02/07/2014
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
#include "evr.h"
#include "evr_select.h"
#include "evr_epoll.h"
#include "ndebug.h"
#include "hlx/time_util.h"
#include "hlx/status.h"
#include "hlx/trace.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
evr_loop::evr_loop(evr_loop_type_t a_type,
                   uint32_t a_max_events):
        m_timer_pq(),
        m_max_events(a_max_events),
        m_loop_type(a_type),
        m_events(NULL),
        m_stopped(false),
        m_evr(NULL)
{

        // -------------------------------------------
        // TODO:
        // EPOLL specific for now
        // -------------------------------------------
        m_events = (evr_event_t *)malloc(sizeof(evr_event_t)*m_max_events);
        //std::vector<struct epoll_event> l_epoll_event_vector(m_max_parallel_connections);

        // -------------------------------------------
        // Get the event handler...
        // -------------------------------------------
#if defined(__linux__)
        if (m_loop_type == EVR_LOOP_EPOLL)
        {
                //NDBG_PRINT("Using evr_select\n");
                m_evr = new evr_epoll();
        }
#endif
        if(!m_evr)
        {
                //NDBG_PRINT("Using evr_select\n");
                m_evr = new evr_select();
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
evr_loop::~evr_loop(void)
{
        // Clean out timer q
        while(!m_timer_pq.empty())
        {
                evr_timer_t *l_timer = m_timer_pq.top();
                if(l_timer)
                {
                        delete l_timer;
                        l_timer = NULL;
                }
                m_timer_pq.pop();
        }
        if(m_events)
        {
                free(m_events);
                m_events = NULL;
        }
        if(m_evr)
        {
                delete m_evr;
                m_evr = NULL;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t evr_loop::run(void)
{
        // -------------------------------------------
        // TODO:
        // EPOLL specific for now
        // -------------------------------------------
        evr_timer_t *l_timer = NULL;
        uint32_t l_time_diff_ms = EVR_DEFAULT_TIME_WAIT_MS;

        // Pop events off pq until time > now
        while(!m_timer_pq.empty())
        {
                uint64_t l_now_ms = get_time_ms();
                l_timer = m_timer_pq.top();
                if((l_now_ms < l_timer->m_time_ms) && (l_timer->m_state != EVR_TIMER_CANCELLED))
                {
                        l_time_diff_ms = l_timer->m_time_ms - l_now_ms;
                        break;
                }
                else
                {
                        // Delete
                        m_timer_pq.pop();
                        // If not cancelled
                        if(l_timer->m_state != EVR_TIMER_CANCELLED)
                        {
                                //NDBG_PRINT("%sRUNNING_%s TIMER: %p at %lu ms\n",ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF,l_timer_event,l_now_ms);
                                int32_t l_s;
                                l_s = l_timer->m_timer_cb(l_timer->m_ctx, l_timer->m_data);
                                delete l_timer;
                                l_timer = NULL;
                                return l_s;
                        }
                        //NDBG_PRINT("%sDELETING%s TIMER: %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_timer);
                        delete l_timer;
                        l_timer = NULL;
                }
        }

        // -------------------------------------------
        // Wait for events
        // -------------------------------------------
        int l_num_events = 0;
        //NDBG_PRINT("%sWAIT4_CONNECTIONS%s: l_time_diff_ms = %d\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_time_diff_ms);
        l_num_events = m_evr->wait(m_events, m_max_events, l_time_diff_ms);
        //NDBG_PRINT("%sSTART_CONNECTIONS%s: l_num_events = %d\n", ANSI_COLOR_FG_MAGENTA, ANSI_COLOR_OFF, l_num_events);

        // -------------------------------------------
        // Service them
        // -------------------------------------------
        for (int i_event = 0; (i_event < l_num_events) && (!m_stopped); ++i_event)
        {
                evr_fd_t* l_evr_fd = static_cast<evr_fd_t*>(m_events[i_event].data.ptr);
                // Validity checks
                if(!l_evr_fd ||
                   (l_evr_fd->m_magic != EVR_EVENT_FD_MAGIC))
                {
                        TRC_ERROR("bad event -ignoring.\n");
                        continue;
                }
                uint32_t l_events = m_events[i_event].events;

                //NDBG_PRINT("%sEVENTS%s: l_events %d/%d = 0x%08X\n",
                //           ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF,
                //           i_event, l_num_events, l_events);
                // Service callbacks per type
                if((l_events & EVR_EV_IN) ||
                   (l_events & EVR_EV_RDHUP) ||
                   (l_events & EVR_EV_HUP) ||
                   (l_events & EVR_EV_ERR))
                {
                        if(l_evr_fd->m_read_cb)
                        {
                                int32_t l_status;
                                l_status = l_evr_fd->m_read_cb(l_evr_fd->m_data);
                                if(l_status != HLX_STATUS_OK)
                                {
                                        //NDBG_PRINT("Error\n");
                                        // Skip handling more events for this fd
                                        continue;
                                }
                        }
                }
                if(l_events & EVR_EV_OUT)
                {
                        if(l_evr_fd->m_write_cb)
                        {
                                int32_t l_status;
                                l_status = l_evr_fd->m_write_cb(l_evr_fd->m_data);
                                if(l_status != HLX_STATUS_OK)
                                {
                                        //NDBG_PRINT("Error\n");
                                        // Skip handling more events for this fd
                                        continue;
                                }
                        }
                }
                // -----------------------------------------
                // TODO other errors???
                // Currently "most" errors handled by read
                // callbacks
                // -----------------------------------------
                //uint32_t l_other_events = l_events & (~(EPOLLIN | EPOLLOUT));
                //if(l_events & EPOLLRDHUP)
                //if(l_events & EPOLLERR)
                //if(0)
                //{
                //        if(l_evr_event->m_error_cb)
                //        {
                //                int32_t l_status = HLX_STATUS_OK;
                //                l_status = l_evr_event->m_error_cb(l_evr_event->m_data);
                //                if(l_status != HLX_STATUS_OK)
                //                {
                //                        //NDBG_PRINT("Error: l_status: %d\n", l_status);
                //                        // Skip handling more events for this fd
                //                        continue;
                //                }
                //        }
                //}
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t evr_loop::add_fd(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event)
{
        int l_status;
        //NDBG_PRINT("%sADD_FD%s: fd[%d], mask: 0x%08X\n", ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF, a_fd, a_attr_mask);
        l_status = m_evr->add(a_fd, a_attr_mask, a_evr_fd_event);
        return l_status;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t evr_loop::mod_fd(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event)
{
        int l_status;
        //NDBG_PRINT("%sMOD_FD%s: fd[%d], mask: 0x%08X\n", ANSI_COLOR_FG_WHITE, ANSI_COLOR_OFF, a_fd, a_attr_mask);
        l_status = m_evr->mod(a_fd, a_attr_mask, a_evr_fd_event);
        return l_status;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t evr_loop::del_fd(int a_fd)
{
        if(!m_evr)
        {
                return HLX_STATUS_OK;
        }
        int l_status;
        l_status = m_evr->del(a_fd);
        return l_status;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t evr_loop::add_timer(uint32_t a_time_ms,
                            evr_timer_cb_t a_timer_cb,
                            void *a_ctx,
                            void *a_data,
                            evr_timer_t **ao_timer)
{
        evr_timer_t *l_timer = new evr_timer_t();
        l_timer->m_ctx = a_ctx;
        l_timer->m_data = a_data;
        l_timer->m_state = EVR_TIMER_ACTIVE;
        l_timer->m_time_ms = get_time_ms() + a_time_ms;
        l_timer->m_timer_cb = a_timer_cb;
        //NDBG_PRINT("%sADDING__%s[%p] TIMER: %p at %u ms --> %lu\n",
        //        ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF,
        //        a_data, l_timer,a_time_ms, l_timer->m_time_ms);
        //NDBG_PRINT_BT();
        m_timer_pq.push(l_timer);
        *ao_timer = l_timer;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t evr_loop::cancel_timer(evr_timer_t *a_timer)
{
        //NDBG_PRINT("%sCANCEL__%s TIMER: %p\n",ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,a_timer);
        //NDBG_PRINT_BT();
        // TODO synchronization???
        if(a_timer)
        {
                //printf("%sXXX%s: %p TIMER AT %24lu ms --> %24lu\n",ANSI_COLOR_FG_RED, ANSI_COLOR_OFF,a_timer,0,l_timer_event->m_time_ms);
                a_timer->m_state = EVR_TIMER_CANCELLED;
                return HLX_STATUS_OK;
        }
        else
        {
                return HLX_STATUS_ERROR;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t evr_loop::signal(void)
{
        //NDBG_PRINT("%sSIGNAL%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
        if(!m_evr)
        {
                return HLX_STATUS_ERROR;
        }
        return m_evr->signal();
}

} //namespace ns_hlx {

