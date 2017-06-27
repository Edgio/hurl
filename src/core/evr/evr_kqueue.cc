//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    evr_kqueue.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    02/29/2016
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
#include "evr_kqueue.h"
#include "ndebug.h"
#include "hurl/status.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
evr_kqueue::evr_kqueue(void):
                m_fd(-1)
{
        // TODO
        m_fd = kqueue();
        if (m_fd == -1)
        {
                printf("Error: kqueue() failed: %s\n", strerror(errno));
                exit(-1);
        }
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_kqueue::wait(evr_events_t* a_ev, int a_max_events, int a_timeout_msec)
{
        int l_ne;
        struct timespec l_to;
        struct timespec *l_to_p = NULL;
        if(a_timeout_msec > -1)
        {
                l_to.tv_sec = a_timeout_msec/1000;
                l_to.tv_nsec = (a_timeout_msec%1000)*1000000;
                l_to_p = &l_to;
        }
        l_ne = kevent(m_fd, NULL, 0, m_events, m_setsize, l_to_p);
#if 0

        if (l_ne > 0)
        {
                int j;
                for(j = 0; j < l_ne; j++)
                {
                        int mask = 0;
                        struct kevent *e = state->events+j;
                        if (e->filter == EVFILT_READ) mask |= AE_READABLE;
                        if (e->filter == EVFILT_WRITE) mask |= AE_WRITABLE;
                        eventLoop->fired[j].fd = e->ident;
                        eventLoop->fired[j].mask = mask;
                }
        }
        return l_ne;
#endif
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_kqueue::add(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event)
{
        struct kevent l_ke;
        // TODO -map attributes...
        if(a_attr_mask & AE_READABLE)
        {
                EV_SET(&l_ke, a_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
                if (kevent(m_fd, &l_ke, 1, NULL, 0, NULL) == -1)
                {
                        return HURL_STATUS_ERROR;
                }
        }
        if(a_attr_mask & AE_WRITABLE)
        {
                EV_SET(&l_ke, a_fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
                if(kevent(m_fd, &l_ke, 1, NULL, 0, NULL) == -1)
                {
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
int evr_kqueue::mod(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event)
{
        // TODO -map attributes...
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int evr_kqueue::del(int a_fd)
{
        struct kevent l_ke;
        // TODO del with attrs???
        if (mask & AE_READABLE)
        {
                EV_SET(&l_ke, a_fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                kevent(m_fd, &l_ke, 1, NULL, 0, NULL);
        }
        if (mask & AE_WRITABLE)
        {
                EV_SET(&l_ke, a_fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
                kevent(m_fd, &l_ke, 1, NULL, 0, NULL);
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t evr_kqueue::signal(void)
{
        // TODO
        return HURL_STATUS_OK;
}
} //namespace ns_hurl {
