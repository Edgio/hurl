//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    evr_select.cc
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
#include "evr_select.h"
#include "ndebug.h"

#include <string.h>
#include <errno.h>
#include <assert.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
evr_select::evr_select(int a_max_connections) :
	m_conns(),
	m_rfdset(),
	m_wfdset()
{
	m_conns.resize(a_max_connections + 10);
	FD_ZERO(&m_rfdset);
	FD_ZERO(&m_wfdset);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_select::wait(epoll_event* a_ev, int a_max_events, int a_timeout_msec)
{
        fd_set l_rfdset_tmp = m_rfdset;
        fd_set l_wfdset_tmp = m_wfdset;

        struct timeval l_timeout;

        if (a_timeout_msec >= 0)
        {
                l_timeout.tv_sec = a_timeout_msec / 1000L;
                l_timeout.tv_usec = (a_timeout_msec % 1000L) * 1000L;
        }

        int l_select_status = 0;

        l_select_status = select(FD_SETSIZE, &l_rfdset_tmp, &l_wfdset_tmp, NULL, a_timeout_msec >= 0 ? &l_timeout : NULL);

        if (l_select_status < 0)
        {
                fprintf(stderr, "select() failed: %s\n", strerror(errno));
                return -1;
        }

        if (l_select_status > a_max_events)
        {
                fprintf(stderr, "select() returned too many events (got %d, expected <= %d)\n", l_select_status, a_max_events);
                return -1;
        }

        int p = 0;

        for (size_t i_conn = 0; i_conn < m_conns.size(); ++i_conn)
        {
                if (m_conns[i_conn] != NULL)
                {
                        bool gotit = false;

                        if (FD_ISSET(i_conn, &l_wfdset_tmp))
                        {
                                a_ev[p].events |= EPOLLOUT;
                                gotit = true;
                        }

                        if (FD_ISSET(i_conn, &l_rfdset_tmp))
                        {
                                a_ev[p].events |= EPOLLIN;
                                gotit = true;
                        }

                        if (gotit)
                        {
                                a_ev[p].data.ptr = m_conns[i_conn];
                                ++p;
                                assert(p <= l_select_status);
                        }
                }
        }

        return p;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_select::add(int a_fd, uint32_t a_attr_mask, void* a_data)
{
        int l_fd = a_fd;
        if (size_t(l_fd) >= m_conns.size())
        {
                m_conns.resize(size_t(l_fd) + 1);
        }
        m_conns[size_t(l_fd)] = a_data;
        FD_SET(a_fd, &m_wfdset);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_select::mod(int a_fd, uint32_t a_attr_mask, void* a_data)
{
        int l_fd = a_fd;
        FD_CLR(l_fd, &m_wfdset);
        FD_SET(l_fd, &m_rfdset);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_select::del(int a_fd)
{
        int l_fd = a_fd;
        if (size_t(l_fd) < m_conns.size())
                m_conns[size_t(l_fd)] = NULL;

        FD_CLR(a_fd, &m_rfdset);
        FD_CLR(a_fd, &m_wfdset);
        return STATUS_OK;
}

} //namespace ns_hlx {

