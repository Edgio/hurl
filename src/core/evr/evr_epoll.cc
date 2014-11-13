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
int evr_epoll::wait_events(epoll_event* a_ev, int a_max_events, int a_timeout_msec)
{
        //NDBG_PRINT("%sepoll_wait waitms%s = %d a_max_events = %d\n",  ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF, a_timeout_msec, a_max_events);

        int l_epoll_status = 0;
        l_epoll_status = epoll_wait(m_epoll_fd, a_ev, a_max_events, a_timeout_msec);

        //NDBG_PRINT("%sepoll_wait status%s = %d\n",  ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF, l_epoll_status);

        return l_epoll_status;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_epoll::add_out(int a_fd, void* a_data)
{

        //NDBG_PRINT("%sADD_OUT%s: fd: %d\n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, a_fd);
        struct epoll_event ev;
        ev.events = EPOLLOUT | EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
        //ev.events = EPOLLOUT | EPOLLET | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
        ev.data.ptr = a_data;
        if (0 != epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, a_fd, &ev))
        {
                NDBG_PRINT("Error: epoll_fd[%d] EPOLL_CTL_ADD fd[%d] failed (%s)\n", m_epoll_fd, a_fd, strerror(errno));
                return -1;
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_epoll::add_in(int a_fd, void* a_data)
{
        //NDBG_PRINT("%sADD_IN%s: fd: %d\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, a_fd);

        struct epoll_event ev;
        //ev.events = EPOLLIN/* | EPOLLET */;
        //ev.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
        ev.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
        ev.data.ptr = a_data;
        if (0 != epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, a_fd, &ev))
        {
                NDBG_PRINT("Error: epoll_fd[%d] EPOLL_CTL_MOD fd[%d] failed (%s)\n", m_epoll_fd, a_fd, strerror(errno));
                return -1;
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void evr_epoll::forget(int a_fd, void* a_data)
{

        //NDBG_PRINT("%sFORGET%s: fd: %d\n", ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF, a_fd);

        struct epoll_event ev;
        ev.data.ptr = a_data;

        if((m_epoll_fd > 0) && (a_fd > 0))
        {
                if (0 != epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, a_fd, &ev))
                {
                        NDBG_PRINT("Error: epoll_fd[%d] EPOLL_CTL_DEL fd[%d] failed (%s)\n", m_epoll_fd, a_fd, strerror(errno));
                        return;
                }
        }
}

