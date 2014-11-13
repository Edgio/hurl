//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
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
#include "evr.h"
#include <vector>

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class evr_select: public evr
{
public:
        evr_select(int a_max_connections);
        int wait_events(epoll_event* a_ev, int a_max_events, int a_timeout_msec);
        int add_out(int a_fd, void* a_data);
        int add_in(int a_fd, void* a_data);
        void forget(int a_fd, void* a_data);

private:
        DISALLOW_COPY_AND_ASSIGN(evr_select)

        std::vector<void*> m_conns;
        fd_set m_rfdset;
        fd_set m_wfdset;
};

#endif


