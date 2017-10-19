//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
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
#include "hurl/support/ndebug.h"
#include "hurl/status.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
evr_epoll::evr_epoll(void):
        m_fd(-1),
        m_ctrl_fd(-1)
{
        //NDBG_PRINT("%sCREATE_EPOLL%s: a_max_events = %d\n",
        //           ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF, a_max_connections);
        m_fd = epoll_create1(0);
        if (m_fd == -1)
        {
                printf("Error: epoll_create() failed: %s\n", strerror(errno));
                exit(-1);
        }

        // Create ctrl fd
        m_ctrl_fd = eventfd(0, EFD_NONBLOCK);
        if (m_ctrl_fd == -1)
        {
                printf("Error: eventfd() failed: %s\n", strerror(errno));
                exit(-1);
        }
        
        int32_t l_status = 0;
        l_status = add(m_ctrl_fd, EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_ET, NULL);
        if(l_status != 0)
        {
                printf("Error: failed to add ctrl fd.\n");
                exit(-1);
        }

}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_epoll::wait(evr_events_t* a_ev, int a_max_events, int a_timeout_msec)
{
        //NDBG_PRINT("%swait%s = %d a_max_events = %d\n",
        //           ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF,
        //           a_timeout_msec, a_max_events);
        int l_epoll_status = 0;
        l_epoll_status = epoll_wait(m_fd, (epoll_event *)a_ev, a_max_events, a_timeout_msec);
        //NDBG_PRINT("%swait%s = %d\n",
        //           ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF,
        //           l_epoll_status);
        return l_epoll_status;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static inline uint32_t get_epoll_attr(uint32_t a_attr_mask)
{
        uint32_t l_attr = 0;
        if(a_attr_mask & EVR_FILE_ATTR_MASK_READ)         l_attr |= EPOLLIN;
        if(a_attr_mask & EVR_FILE_ATTR_MASK_WRITE)        l_attr |= EPOLLOUT;
        if(a_attr_mask & EVR_FILE_ATTR_MASK_STATUS_ERROR) l_attr |= EPOLLERR;
        if(a_attr_mask & EVR_FILE_ATTR_MASK_RD_HUP)       l_attr |= EPOLLRDHUP;
        if(a_attr_mask & EVR_FILE_ATTR_MASK_HUP)          l_attr |= EPOLLRDHUP;
        if(a_attr_mask & EVR_FILE_ATTR_MASK_ET)           l_attr |= EPOLLET;
        return l_attr;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_epoll::add(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event)
{
        //NDBG_PRINT("%sadd%s: fd: %d --attr: 0x%08X\n",
        //           ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF,
        //           a_fd, a_attr_mask);
        //NDBG_PRINT_BT();
        struct epoll_event ev;
        ev.events = get_epoll_attr(a_attr_mask);
        ev.data.ptr = a_evr_fd_event;
        if (0 != epoll_ctl(m_fd, EPOLL_CTL_ADD, a_fd, &ev))
        {
                NDBG_PRINT("Error: epoll_fd[%d] EPOLL_CTL_ADD fd[%d] failed (%s)\n",
                           m_fd, a_fd, strerror(errno));
                return HURL_STATUS_ERROR;
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_epoll::mod(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event)
{
        //NDBG_PRINT("%smod%s: fd: %d --attr: 0x%08X\n",
        //           ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF,
        //           a_fd, a_attr_mask);
        //NDBG_PRINT_BT();
        struct epoll_event ev;
        ev.events = get_epoll_attr(a_attr_mask);
        ev.data.ptr = a_evr_fd_event;
        if (0 != epoll_ctl(m_fd, EPOLL_CTL_MOD, a_fd, &ev))
        {
                NDBG_PRINT("Error: epoll_fd[%d] EPOLL_CTL_MOD fd[%d] failed (%s)\n",
                           m_fd, a_fd, strerror(errno));
                return HURL_STATUS_ERROR;
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_epoll::del(int a_fd)
{
        // TODO Can skip del for close(fd) -since is removed automagically
        //NDBG_PRINT("%sdel%s: fd: %d\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, a_fd);
        struct epoll_event ev;
        if((m_fd > 0) && (a_fd > 0))
        {
                if (0 != epoll_ctl(m_fd, EPOLL_CTL_DEL, a_fd, &ev))
                {
                        //NDBG_PRINT("Error: epoll_fd[%d] EPOLL_CTL_DEL fd[%d] failed (%s)\n",
                        //           m_fd, a_fd, strerror(errno));
                        return HURL_STATUS_ERROR;
                }
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t evr_epoll::signal(void)
{
        // Wake up epoll_wait by writing to control fd
        uint64_t l_value = 1;
        ssize_t l_write_status = 0;
        //NDBG_PRINT("WRITING m_ctrl_fd: %d\n", m_ctrl_fd);
        l_write_status = write(m_ctrl_fd, &l_value, sizeof (l_value));
        if(l_write_status == -1)
        {
                //NDBG_PRINT("l_write_status: %ld\n", l_write_status);
                return HURL_STATUS_ERROR;
        }
        return HURL_STATUS_OK;
}
} //namespace ns_hurl {
