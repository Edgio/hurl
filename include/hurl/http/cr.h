//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    cr.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    03/11/2015
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
#ifndef _CR_H
#define _CR_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
// For fixed size types
#include <stdint.h>
#include <list>

namespace ns_hurl {

//: ----------------------------------------------------------------------------
//: Raw buffer
//: ----------------------------------------------------------------------------
typedef struct cr_struct
{
        uint64_t m_off;
        uint64_t m_len;
        cr_struct():
                m_off(0),
                m_len(0)
        {}
        void clear(void)
        {
                m_off = 0;
                m_len = 0;
        }
} cr_t;

typedef std::list <cr_t> cr_list_t;

}

#endif


