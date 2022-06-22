//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _EVR_KQUEUE_H
#define _EVR_KQUEUE_H
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include "evr/evr.h"
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
class evr_kqueue: public evr
{
public:
        evr_kqueue(void);
        int wait(evr_events_t* a_ev, int a_max_events, int a_timeout_msec);
        int add(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event);
        int mod(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event);
        int del(int a_fd);
        int signal(void);
private:
        // Disallow copy/assign
        evr_kqueue& operator=(const evr_kqueue &);
        evr_kqueue(const evr_kqueue &);
        int m_fd;
};
} //namespace ns_hurl {
#endif
