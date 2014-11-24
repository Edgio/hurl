//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    evr_epoll.cc
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
#include "evr_epoll.h"
#include "ndebug.h"

#include <string.h>
#include <errno.h>

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
evr_epoll::evr_epoll(int a_max_connections) :
m_epoll_fd(-1)
{
        //NDBG_PRINT("%sCREATE_EPOLL%s: a_max_events = %d\n",  ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF, a_max_connections);
        m_epoll_fd = epoll_create(a_max_connections);
        if (m_epoll_fd == -1)
        {
                fprintf(stderr, "Error: epoll_create() failed: %s --max_connections = %d\n", strerror(errno), a_max_connections);
                exit(-1);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_epoll::wait(epoll_event* a_ev, int a_max_events, int a_timeout_msec)
{
        //NDBG_PRINT("%swait%s = %d a_max_events = %d\n",  ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF, a_timeout_msec, a_max_events);
        int l_epoll_status = 0;
        l_epoll_status = epoll_wait(m_epoll_fd, a_ev, a_max_events, a_timeout_msec);
        //NDBG_PRINT("%swait%s = %d\n",  ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF, l_epoll_status);
        return l_epoll_status;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static inline uint32_t get_epoll_attr(uint32_t a_attr_mask)
{
        // EPOLLET???
        uint32_t l_attr = 0;
        if(a_attr_mask & EVR_FILE_ATTR_MASK_READ)         l_attr |= EPOLLIN;
        if(a_attr_mask & EVR_FILE_ATTR_MASK_WRITE)        l_attr |= EPOLLOUT;
        if(a_attr_mask & EVR_FILE_ATTR_MASK_STATUS_ERROR) l_attr |= EPOLLHUP | EPOLLRDHUP | EPOLLERR;
        return l_attr;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_epoll::add(int a_fd, uint32_t a_attr_mask, void* a_data)
{
        //NDBG_PRINT("%sadd%s: fd: %d --attr: 0x%08X\n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, a_fd, a_attr_mask);
        //NDBG_PRINT_BT();
        struct epoll_event ev;
        ev.events = get_epoll_attr(a_attr_mask);
        ev.data.ptr = a_data;
        if (0 != epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, a_fd, &ev))
        {
                NDBG_PRINT("Error: epoll_fd[%d] EPOLL_CTL_ADD fd[%d] failed (%s)\n", m_epoll_fd, a_fd, strerror(errno));
                return STATUS_ERROR;
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_epoll::mod(int a_fd, uint32_t a_attr_mask, void* a_data)
{
        //NDBG_PRINT("%smod%s: fd: %d --attr: 0x%08X\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, a_fd, a_attr_mask);
        //NDBG_PRINT_BT();
        struct epoll_event ev;
        ev.events = get_epoll_attr(a_attr_mask);
        ev.data.ptr = a_data;
        if (0 != epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, a_fd, &ev))
        {
                NDBG_PRINT("Error: epoll_fd[%d] EPOLL_CTL_MOD fd[%d] failed (%s)\n", m_epoll_fd, a_fd, strerror(errno));
                return STATUS_ERROR;
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_epoll::del(int a_fd)
{
        //NDBG_PRINT("%sdel%s: fd: %d\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, a_fd);
        struct epoll_event ev;
        if((m_epoll_fd > 0) && (a_fd > 0))
        {
                if (0 != epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, a_fd, &ev))
                {
                        NDBG_PRINT("Error: epoll_fd[%d] EPOLL_CTL_DEL fd[%d] failed (%s)\n", m_epoll_fd, a_fd, strerror(errno));
                        return STATUS_OK;
                }
        }
        return STATUS_OK;
}
