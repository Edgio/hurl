//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    evr_select.h
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
#ifndef _EVR_SELECT_H
#define _EVR_SELECT_H
//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hurl/evr/evr.h"
#include <sys/select.h>
#include <map>
namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
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
