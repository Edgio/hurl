//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    proxy_u.h
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
#ifndef _PROXY_U_H
#define _PROXY_U_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <stdint.h>
#include <sys/types.h>
#include "hlx/base_u.h"
#include "hlx/subr.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------
class nbq;
class subr;

//: ----------------------------------------------------------------------------
//: proxy_u upstream proxy object
//: ----------------------------------------------------------------------------
class proxy_u: public base_u
{

public:
        // -------------------------------------------------
        // const
        // -------------------------------------------------
        static const uint32_t S_UPS_TYPE_PROXY = 0xFFFF000B;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        proxy_u();
        ~proxy_u();
        void set_subr(subr *a_subr);

        // -------------------------------------------------
        // upstream methods
        // -------------------------------------------------
        ssize_t ups_read(char *ao_dst, size_t a_len);
        ssize_t ups_read(nbq &ao_q, size_t a_len);
        bool ups_done(void);
        int32_t ups_cancel(void);
        uint32_t get_type(void) { return S_UPS_TYPE_PROXY;}

        // -------------------------------------------------
        // Public Class methods
        // -------------------------------------------------
        static int32_t s_completion_cb(subr &a_subr, nconn &a_nconn, resp &a_resp);
        static int32_t s_error_cb(subr &a_subr,
                                  nconn *a_nconn,
                                  http_status_t a_status,
                                  const char *a_error_str);


private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        proxy_u& operator=(const proxy_u &);
        proxy_u(const proxy_u &);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        subr *m_subr;
};

} //namespace ns_hlx {

#endif
