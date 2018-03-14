//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    req_stat.h
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
#ifndef _REQ_STAT_H
#define _REQ_STAT_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
// Sockets...
#include <stdint.h>
#include <strings.h>

namespace ns_hurl {

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
// Single stat
typedef struct req_stat_struct
{
        uint32_t m_body_bytes;
        uint32_t m_total_bytes;
        uint64_t m_tt_connect_us;
        uint64_t m_tt_first_read_us;
        uint64_t m_tt_completion_us;
        int32_t m_last_state;
        int32_t m_error;

} req_stat_t;



} //namespace ns_hurl {

#endif



