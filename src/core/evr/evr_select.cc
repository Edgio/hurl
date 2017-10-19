//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
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
#include "hurl/support/ndebug.h"
#include "hurl/status.h"
#include <string.h>
#include <errno.h>
#include <assert.h>
namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
evr_select::evr_select(void) :
	m_conn_map(),
	m_rfdset(),
	m_wfdset()
{
	FD_ZERO(&m_rfdset);
	FD_ZERO(&m_wfdset);
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_select::wait(evr_events_t* a_ev, int a_max_events, int a_timeout_msec)
{
        //NDBG_PRINT("%swait%s = %d a_max_events = %d\n",
        //           ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF,
        //           a_timeout_msec, a_max_events);
        fd_set l_rfdset = m_rfdset;
        fd_set l_wfdset = m_wfdset;
        struct timeval l_timeout;
        if (a_timeout_msec >= 0)
        {
                l_timeout.tv_sec = a_timeout_msec / 1000L;
                l_timeout.tv_usec = (a_timeout_msec % 1000L) * 1000L;
        }
        int l_sstat = 0;
        uint32_t l_fdsize = 1;
        if(m_conn_map.size())
        {
                l_fdsize = m_conn_map.rbegin()->first + 1;
        }
        l_sstat = select(l_fdsize,
                         &l_rfdset,
                         &l_wfdset,
                         NULL,
                         a_timeout_msec >= 0 ? &l_timeout : NULL);
        //NDBG_PRINT("l_sstat: %d\n", l_sstat);
        if (l_sstat < 0)
        {
                //NDBG_PRINT("Error select() failed. Reason: %s\n", strerror(errno));
                return HURL_STATUS_ERROR;
        }
        if (l_sstat > a_max_events)
        {
                //NDBG_PRINT("Error select() returned too many events (got %d, expected <= %d)\n",
                //                l_sstat, a_max_events);
                return HURL_STATUS_ERROR;
        }
        int l_p = 0;
        for(conn_map_t::iterator i_conn = m_conn_map.begin(); i_conn != m_conn_map.end(); ++i_conn)
        {
                int l_fd = i_conn->first;
                bool l_inset = false;
                if (FD_ISSET(l_fd, &l_wfdset))
                {
                        //NDBG_PRINT("INSET: EPOLLOUT fd: %d\n", l_fd);
                        a_ev[l_p].events |= EVR_EV_OUT;
                        l_inset = true;
                }
                if (FD_ISSET(l_fd, &l_rfdset))
                {
                        //NDBG_PRINT("INSET: EPOLLIN fd: %d\n", l_fd);
                        a_ev[l_p].events |= EVR_EV_IN;
                        l_inset = true;
                }
                if (l_inset)
                {
                        //NDBG_PRINT("INSET: fd: %d\n", l_fd);
                        a_ev[l_p].data.ptr = i_conn->second;
                        ++l_p;
                        if(l_p > l_sstat)
                        {
                                //NDBG_PRINT("Error num events exceeds select result.\n");
                                return HURL_STATUS_ERROR;
                        }
                }
        }
        //NDBG_PRINT("%swait%s = %d\n",
        //           ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF,
        //           l_p);
        return l_p;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_select::add(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event)
{
        //NDBG_PRINT("%sadd%s: fd: %d --attr: 0x%08X --data: %p\n",
        //           ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF,
        //           a_fd, a_attr_mask, a_data);
        m_conn_map[a_fd] = a_evr_fd_event;
        mod(a_fd, a_attr_mask, a_evr_fd_event);
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_select::mod(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event)
{
        //NDBG_PRINT("%smod%s: fd: %d --attr: 0x%08X\n",
        //           ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF,
        //           a_fd, a_attr_mask);
        FD_CLR(a_fd, &m_wfdset);
        FD_CLR(a_fd, &m_rfdset);
        if(a_attr_mask & EVR_FILE_ATTR_MASK_READ)         { FD_SET(a_fd, &m_rfdset); }
        if(a_attr_mask & EVR_FILE_ATTR_MASK_WRITE)        { FD_SET(a_fd, &m_wfdset); }
        if(a_attr_mask & EVR_FILE_ATTR_MASK_RD_HUP)       { FD_SET(a_fd, &m_rfdset); }
        if(a_attr_mask & EVR_FILE_ATTR_MASK_HUP)          { FD_SET(a_fd, &m_rfdset); }
        if(a_attr_mask & EVR_FILE_ATTR_MASK_STATUS_ERROR) { FD_SET(a_fd, &m_rfdset); }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_select::del(int a_fd)
{
        //NDBG_PRINT("%sdel%s: fd: %d\n",
        //           ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
        //           a_fd);
        m_conn_map.erase(a_fd);
        FD_CLR(a_fd, &m_rfdset);
        FD_CLR(a_fd, &m_wfdset);
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_select::signal(void)
{
        // TODO use a pipe for signalling...
        return HURL_STATUS_OK;
}
} //namespace ns_hurl {
