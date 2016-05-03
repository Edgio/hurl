//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    file_u.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    11/20/2015
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
#ifndef _BASE_U_H
#define _BASE_U_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------
class nbq;

//: ----------------------------------------------------------------------------
//: base class for all upstream objects
//: ----------------------------------------------------------------------------
class base_u
{

public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        virtual ~base_u() {};
        virtual ssize_t ups_read(char *ao_dst, size_t a_len) = 0;
        virtual ssize_t ups_read(nbq &ao_q, size_t a_len) = 0;
        virtual bool ups_done(void) = 0;
        virtual int32_t ups_cancel(void) = 0;
        virtual uint32_t get_type(void) = 0;
};

} //namespace ns_hlx {

#endif
