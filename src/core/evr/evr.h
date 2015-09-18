//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    evr.h
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
#ifndef _EVR_H
#define _EVR_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <stdlib.h>
#include <sys/epoll.h>

#include <queue> // for std::priority_queue
#include <vector>
#include <list>

#include "ndebug.h"
//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define EVR_DEFAULT_TIME_WAIT_MS 300

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class conn;

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------
typedef enum evr_loop_type
{
        EVR_LOOP_EPOLL,
        EVR_LOOP_SELECT
} evr_loop_type_t;

typedef enum evr_timer_state
{
        EVR_TIMER_ACTIVE,
        EVR_TIMER_CANCELLED
} evr_timer_state_t;

typedef enum evr_file_attr
{
        EVR_FILE_ATTR_MASK_READ = 1 << 0,
        EVR_FILE_ATTR_MASK_WRITE = 1 << 1,
        EVR_FILE_ATTR_MASK_STATUS_ERROR = 1 << 2
} evr_file_attr_t;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class evr
{
public:
        evr():
                m_epoll_fd(-1)
        {};
        virtual ~evr() {};

        virtual int wait(epoll_event* a_ev, int a_max_events, int a_timeout_msec) = 0;
        virtual int add(int a_fd, uint32_t a_attr_mask, void* a_data, bool a_edge_triggered) = 0;
        virtual int mod(int a_fd, uint32_t a_attr_mask, void* a_data, bool a_edge_triggered) = 0;
        virtual int del(int a_fd) = 0;

private:
        int m_epoll_fd;
};

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------

// -----------------------------------------------
// Callback Types
// -----------------------------------------------
// File callback
typedef int32_t (*evr_file_cb_t)(void *);

// Timer callback
typedef int32_t (*evr_timer_cb_t)(void *);


// -----------------------------------------------
// Event Types
// -----------------------------------------------
// File event
typedef struct evr_file_event_struct {

        // -------------------------------------------
        // TODO Reevaluate Using class member instead
        // of fd specific cb's
        // -------------------------------------------
        //evr_file_cb_t m_read_cb;
        //evr_file_cb_t m_write_cb;
        //evr_file_cb_t m_error_cb; // TODO for EPOLLHUP/EPOLLERR

        void *m_data;

} evr_file_event_t;


// Timer event
typedef struct evr_timer_event_struct {

        uint64_t m_time_ms;
        evr_timer_cb_t m_timer_cb;
        evr_timer_state_t m_state;

        void *m_data;

} evr_timer_event_t;


//: ----------------------------------------------------------------------------
//: Priority queue sorting
//: ----------------------------------------------------------------------------
class evr_compare_timers {
public:
        bool operator()(evr_timer_event_t* t1, evr_timer_event_t* t2) // Returns true if t1 is greater than t2
        {
                return (t1->m_time_ms > t2->m_time_ms);
        }
};

typedef std::priority_queue<evr_timer_event_t *, std::vector<evr_timer_event_t *>, evr_compare_timers> evr_timer_pq_t;


//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class evr_loop
{
public:
        // -------------------------------------------
        // TODO Reevaluate Using class member instead
        // of fd specific cb's
        // -------------------------------------------
        evr_loop(evr_file_cb_t a_read_cb = NULL,
                 evr_file_cb_t a_write_cb = NULL,
                 evr_file_cb_t a_error_cb = NULL,
                 evr_loop_type_t a_type = EVR_LOOP_EPOLL,
                 uint32_t a_max_conn = 1,
                 bool a_use_lock = false,
                 bool a_edge_triggered = false);
        ~evr_loop();
        int32_t run(void);

        // -------------------------------------------
        // File events...
        // -------------------------------------------
        int32_t add_fd(int a_fd, uint32_t a_attr_mask, void *a_data);
        int32_t mod_fd(int a_fd, uint32_t a_attr_mask, void *a_data);
        int32_t del_fd(int a_fd);

        uint64_t get_pq_size(void) { return m_timer_pq.size();};

        // -------------------------------------------
        // Timer events...
        // -------------------------------------------
        int32_t add_timer(uint64_t a_time_ms, evr_timer_cb_t a_timer_cb, void *a_data, void **ao_timer);
        int32_t cancel_timer(void **a_timer);
        int32_t stop(void);

        // -------------------------------------------
        // Event events... :)
        // -------------------------------------------
        int32_t add_event(void *a_data);
        int32_t signal_event(int a_fd);

private:
        evr_loop(const evr_loop&);
        evr_loop& operator=(const evr_loop&);

        // Timer priority queue -used as min heap
        evr_timer_pq_t m_timer_pq;
        pthread_mutex_t m_timer_pq_mutex;
        bool m_use_lock;
        uint32_t m_max_connections;
        evr_loop_type_t m_loop_type;
        bool m_is_running;

        // TODO EPOLL Specific
        struct epoll_event *m_epoll_event_vector;
        bool m_stopped;
        evr* m_evr;

        // Control fd -used primarily for cancelling an existing epooll_wait
        int m_control_fd;
        bool m_edge_triggered;

        // -------------------------------------------
        // TODO Reevaluate Using class member instead
        // of fd specific cb's
        // -------------------------------------------
        evr_file_cb_t m_read_cb;
        evr_file_cb_t m_write_cb;
        evr_file_cb_t m_error_cb; // TODO for EPOLLHUP/EPOLLERR

};

} //namespace ns_hlx {

#endif

