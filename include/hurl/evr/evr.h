//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
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
#include <stddef.h>
#include <stdint.h>
// for std::priority_queue
#include <queue>
//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define EVR_DEFAULT_TIME_WAIT_MS (-1)
#define EVR_EVENT_FD_MAGIC 0xDEADF154
namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------
// loop type
typedef enum evr_loop_type
{
#if defined(__linux__)
        EVR_LOOP_EPOLL,
#elif defined(__FreeBSD__) || defined(__APPLE__)
        EVR_LOOP_KQUEUE,
#endif
        EVR_LOOP_SELECT
} evr_loop_type_t;
// event state
typedef enum evr_event_state
{
        EVR_EVENT_ACTIVE,
        EVR_EVENT_CANCELLED
} evr_event_state_t;
// file attr
typedef enum evr_file_attr
{
        EVR_FILE_ATTR_MASK_READ = 1 << 0,
        EVR_FILE_ATTR_MASK_WRITE = 1 << 1,
        EVR_FILE_ATTR_MASK_STATUS_ERROR = 1 << 2,
        EVR_FILE_ATTR_MASK_RD_HUP = 1 << 3,
        EVR_FILE_ATTR_MASK_HUP = 1 << 4,
        EVR_FILE_ATTR_MASK_ET = 1 << 5
} evr_file_attr_t;
// evr mode
typedef enum mode_enum {
        EVR_MODE_NONE = 0,
        EVR_MODE_READ,
        EVR_MODE_WRITE,
        EVR_MODE_TIMEOUT,
        EVR_MODE_ERROR
} evr_mode_t;
//: ----------------------------------------------------------------------------
//: \details: Types -copied from epoll
//: ----------------------------------------------------------------------------
// event data
typedef union evr_data_union
{
        void *ptr;
        int fd;
        uint32_t u32;
        uint64_t u64;
} evr_data_t;
// event
struct evr_event_struct
{
        uint32_t events;
        evr_data_t data;
} __attribute__ ((__packed__));
typedef evr_event_struct evr_events_t;
//: ----------------------------------------------------------------------------
//: \details: Event Types -copied from epoll
//: ----------------------------------------------------------------------------
typedef enum evr_event_types_enum
{
        EVR_EV_IN = 0x001,
#define EVR_EV_IN EVR_EV_IN
        EVR_EV_OUT = 0x004,
#define EVR_EV_OUT EVR_EV_OUT
        EVR_EV_ERR = 0x008,
#define EVR_EV_ERR EVR_EV_ERR
        EVR_EV_HUP = 0x010,
#define EVR_EV_HUP EVR_EV_HUP
        EVR_EV_RDHUP = 0x2000,
#define EVR_EV_RDHUP EVR_EV_RDHUP
} evr_event_types_t;
//: ----------------------------------------------------------------------------
//: event attributes crib'd from epoll
//: ----------------------------------------------------------------------------
// file attr
#define EVR_FILE_ATTR_VAL_READABLE  0x001D
#define EVR_FILE_ATTR_VAL_WRITEABLE 0x0002
// event attr
#define EVR_EV_VAL_READABLE  0x2019
#define EVR_EV_VAL_WRITEABLE 0x0004
//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
// -----------------------------------------------
// Callback Types
// -----------------------------------------------
// file callback
typedef int32_t (*evr_event_cb_t)(void *);
// -----------------------------------------------
// Event Types
// -----------------------------------------------
// file event
typedef struct evr_fd {
        uint32_t m_magic;
        evr_event_cb_t m_read_cb;
        evr_event_cb_t m_write_cb;
        evr_event_cb_t m_error_cb;
        void *m_data;
        uint32_t m_attr_mask;
} evr_fd_t;
// event
typedef struct evr_event {
        uint64_t m_time_ms;
        evr_event_cb_t m_cb;
        evr_event_state_t m_state;
        void *m_data;
} evr_event_t;
//: ----------------------------------------------------------------------------
//: Priority queue sorting
//: ----------------------------------------------------------------------------
class evr_compare_events {
public:
        // Returns true if t1 is greater than t2
        bool operator()(evr_event_t* t1, evr_event_t* t2)
        {
                return (t1->m_time_ms > t2->m_time_ms);
        }
};
typedef std::priority_queue<evr_event_t *, std::vector<evr_event_t *>, evr_compare_events> evr_event_pq_t;
//: ----------------------------------------------------------------------------
//: \details: evr object -wraps OS specific implementations
//: ----------------------------------------------------------------------------
class evr
{
public:
        evr() {};
        virtual ~evr() {};
        virtual int wait(evr_events_t* a_ev, int a_max_events, int a_timeout_msec) = 0;
        virtual int add(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event) = 0;
        virtual int mod(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event) = 0;
        virtual int del(int a_fd) = 0;
        virtual int signal(void) = 0;
};
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
        evr_loop(evr_loop_type_t a_type = EVR_LOOP_SELECT,
                 uint32_t a_max_events = 512);
        ~evr_loop();
        int32_t run(void);
        evr_loop_type get_loop_type(void) { return m_loop_type; }
        // -------------------------------------------
        // File events...
        // -------------------------------------------
        int32_t add_fd(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event);
        int32_t mod_fd(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event);
        int32_t del_fd(int a_fd);
        uint64_t get_pq_size(void) { return m_event_pq.size();};
        // -------------------------------------------
        // Timer events...
        // -------------------------------------------
        int32_t add_event(uint32_t a_time_ms,
                          evr_event_cb_t a_cb,
                          void *a_data,
                          evr_event_t **ao_event);
        int32_t cancel_event(evr_event_t *a_event);
        int32_t signal(void);
private:
        evr_loop(const evr_loop&);
        evr_loop& operator=(const evr_loop&);
        uint32_t dequeue_events(void);
        // Timer priority queue -used as min heap
        evr_event_pq_t m_event_pq;
        uint32_t m_max_events;
        evr_loop_type_t m_loop_type;
        evr_events_t *m_events;
        bool m_stopped;
        evr* m_evr;
};
} //namespace ns_hurl {
#endif
