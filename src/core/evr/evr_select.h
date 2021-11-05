//! ----------------------------------------------------------------------------
//! Copyright Edgecast Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _EVR_SELECT_H
#define _EVR_SELECT_H
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include "evr/evr.h"
#include <sys/select.h>
#include <map>
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! \details: TODO
//! ----------------------------------------------------------------------------
class evr_select: public evr
{
public:
        evr_select(void);
        int wait(evr_events_t* a_ev, int a_max_events, int a_timeout_msec);
        int add(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event);
        int mod(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event);
        int del(int a_fd);
        int signal(void);
private:
        typedef std::map<uint32_t, void *> conn_map_t;
        // Disallow copy/assign
        evr_select& operator=(const evr_select &);
        evr_select(const evr_select &);
        conn_map_t m_conn_map;
        fd_set m_rfdset;
        fd_set m_wfdset;
};
} //namespace ns_hurl {
#endif
