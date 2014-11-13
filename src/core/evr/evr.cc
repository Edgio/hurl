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
#include "util.h"

#include <unistd.h>

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
evr_loop::evr_loop(evr_file_cb_t a_read_cb,
                   evr_file_cb_t a_write_cb,
                   evr_file_cb_t a_error_cb,
                   evr_loop_type_t a_type,
                   uint32_t a_max_conn,
                   bool a_use_lock):
        m_timer_pq(),
        m_timer_pq_mutex(),
        m_use_lock(a_use_lock),
        m_max_connections(a_max_conn),
        m_loop_type(a_type),
        m_epoll_event_vector(NULL),
        m_stopped(false),
        m_evr(NULL),
        m_read_cb(a_read_cb),
        m_write_cb(a_write_cb),
        m_error_cb(a_error_cb)
{

        // -------------------------------------------
        // TODO:
        // EPOLL specific for now
        // -------------------------------------------
        m_epoll_event_vector = (struct epoll_event *)malloc(sizeof(struct epoll_event)*a_max_conn);
        //std::vector<struct epoll_event> l_epoll_event_vector(m_max_parallel_connections);

        // -------------------------------------------
        // Get the event handler...
        // -------------------------------------------
        if (m_loop_type == EVR_LOOP_SELECT)
        {
                //NDBG_PRINT("Using evr_select\n");
                m_evr = new evr_select(m_max_connections);
        }
        // Default to epoll
        else
        {
                //NDBG_PRINT("Using evr_epoll\n");
                m_evr = new evr_epoll(m_max_connections);
        }

        if(m_use_lock)
        {
                pthread_mutex_init(&m_timer_pq_mutex, NULL);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
evr_loop::~evr_loop(void)
{
        if(m_epoll_event_vector)
        {
                free(m_epoll_event_vector);
                m_epoll_event_vector = NULL;
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
        evr_timer_event_t *l_timer_event = NULL;
        uint32_t l_time_diff_ms = EVR_DEFAULT_TIME_WAIT_MS;

        // Pop events off pq until time > now
        while(!m_timer_pq.empty())
        {
                uint64_t l_now_ms = get_time_ms();

                if(m_use_lock)
                {
                        pthread_mutex_lock(&m_timer_pq_mutex);
                }

                l_timer_event = m_timer_pq.top();
                if(l_now_ms < l_timer_event->m_time_ms)
                {
                        l_time_diff_ms = l_timer_event->m_time_ms - l_now_ms;

                        if(m_use_lock)
                        {
                                pthread_mutex_unlock(&m_timer_pq_mutex);
                        }
                        break;
                }
                else
                {
                        // Delete
                        m_timer_pq.pop();

                        if(m_use_lock)
                        {
                                pthread_mutex_unlock(&m_timer_pq_mutex);
                        }

                        // If not cancelled
                        if(l_timer_event->m_state != EVR_TIMER_CANCELLED)
                        {
                                //NDBG_PRINT("%sRUNNING_%s TIMER: %p at %lu ms\n",ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF,l_timer_event,l_now_ms);
                                l_timer_event->m_timer_cb(l_timer_event->m_data);
                        }



                        //NDBG_PRINT("%sDELETING%s TIMER: %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_timer_event);
                        delete l_timer_event;
                        l_timer_event = NULL;
                }
        }


        // -------------------------------------------
        // Wait for events
        // -------------------------------------------
        int l_num_events = 0;

        //NDBG_PRINT("%sWAIT4_CONNECTIONS%s: l_num_events = %d\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_num_events);
        l_num_events = m_evr->wait_events(m_epoll_event_vector, m_max_connections, l_time_diff_ms);
        //NDBG_PRINT("%sSTART_CONNECTIONS%s: l_num_events = %d\n", ANSI_COLOR_FG_MAGENTA, ANSI_COLOR_OFF, l_num_events);

        // -------------------------------------------
        // Service them
        // -------------------------------------------
        for (int i_event = 0;
                        (i_event < l_num_events) &&
                        (!m_stopped)
                        //&&
                        //((m_run_time_s == -1) || (m_run_time_s > (get_time_s() - m_start_time_s)))
                        ; ++i_event)
        {

                // -------------------------------------------
                // TODO:
                // EPOLL specific for now
                // -------------------------------------------
                void* l_data = m_epoll_event_vector[i_event].data.ptr;
                uint32_t l_events = m_epoll_event_vector[i_event].events;

                //NDBG_PRINT("%sEVENTS%s: l_events = 0x%08X --fd=%d\n", ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF, l_events);

                // Service callbacks per type
                if(l_events & EPOLLIN)
                {
                        if(m_read_cb)
                                m_read_cb(l_data);
                }
                if(l_events & EPOLLOUT)
                {
                        if(m_write_cb)
                                m_write_cb(l_data);
                }
                // TODO More errors...
                uint32_t l_other_events = l_events & (~(EPOLLIN | EPOLLOUT));
                if(l_other_events)
                //if(l_events & EPOLLERR)
                {
                        if(m_error_cb)
                                m_error_cb(l_data);
                }

        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t evr_loop::add_file(int a_fd, uint32_t a_file_attr_mask, void *a_data)
{

        int l_status;
        // -------------------------------------------
        // TODO FIX!!!!!!!!!
        // -------------------------------------------
#if 0
        EVR_FILE_ATTR_MASK_READ = 1 << 0,
        EVR_FILE_ATTR_MASK_WRITE = 1 << 1,
        EVR_FILE_ATTR_MASK_ERROR = 1 << 2
#endif
        if(a_file_attr_mask & EVR_FILE_ATTR_MASK_WRITE)
        {
                l_status = m_evr->add_out(a_fd, a_data);
        }
        else
        {
                l_status = m_evr->add_in(a_fd, a_data);
        }


        return l_status;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t evr_loop::remove_file(int a_fd, uint32_t a_file_attr_mask)
{

        // -------------------------------------------
        // TODO FIX!!!!!!!!!
        // -------------------------------------------
        m_evr->forget(a_fd, NULL);

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t evr_loop::add_timer(uint64_t a_time_ms, evr_timer_cb_t a_timer_cb, void *a_data, void **ao_timer)
{
        evr_timer_event_t *l_timer_event = new evr_timer_event_t();

        l_timer_event->m_data = a_data;
        l_timer_event->m_state = EVR_TIMER_ACTIVE;
        l_timer_event->m_time_ms = get_time_ms() + a_time_ms;
        l_timer_event->m_timer_cb = a_timer_cb;

        //NDBG_PRINT("%sADDING__%s TIMER: %p at %lu ms --> %lu\n",ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF,l_timer_event,a_time_ms, l_timer_event->m_time_ms);

        // Add to pq
        if(m_use_lock)
        {
                pthread_mutex_lock(&m_timer_pq_mutex);
        }
        m_timer_pq.push(l_timer_event);

        if(m_use_lock)
        {
                pthread_mutex_unlock(&m_timer_pq_mutex);
        }

        *ao_timer = static_cast<void *>(l_timer_event);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t evr_loop::cancel_timer(void *a_timer)
{
        // TODO synchronization???
        if(a_timer)
        {
                evr_timer_event_t *l_timer_event = static_cast<evr_timer_event_t *>(a_timer);
                //printf("%sXXX%s: %p TIMER AT %24lu ms --> %24lu\n",ANSI_COLOR_FG_RED, ANSI_COLOR_OFF,a_timer,0,l_timer_event->m_time_ms);
                //NDBG_PRINT_BT(0,"__");
                l_timer_event->m_state = EVR_TIMER_CANCELLED;
                return STATUS_OK;
        }
        else
        {
                return STATUS_ERROR;
        }
}
