//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    cgi_u.h
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
#ifndef _CGI_U_H
#define _CGI_U_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/base_u.h"
#include <stdio.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------
struct evr_fd;
typedef struct evr_fd evr_fd_t;
class nbq;

//: ----------------------------------------------------------------------------
//: cgi_u: non-blocking cgi object
//: ----------------------------------------------------------------------------
class cgi_u: public base_u
{
public:
        // -------------------------------------------------
        // const
        // -------------------------------------------------
        static const uint32_t S_UPS_TYPE_CGI = 0xFFFF000D;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        cgi_u(clnt_session &a_clnt_session,
              int32_t a_timeout_ms = -1);
        ~cgi_u();
        int cginit(const char *a_path, const query_map_t &a_query_map);
        int32_t flush(void);
        // buffered
        int32_t init_q(void);

        // -------------------------------------------------
        // upstream methods
        // -------------------------------------------------
        ssize_t ups_read(size_t a_len);
        int32_t ups_cancel(void);
        uint32_t get_type(void) { return S_UPS_TYPE_CGI;}

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        pid_t m_pid;
        int m_fd;
        evr_fd_t m_evr_fd;
        evr_loop *m_evr_loop;
        int32_t m_timeout_ms;
        evr_timer_t *m_timer_obj;
        // buffered output q
        nbq *m_nbq;
        bool m_supports_keep_alives;
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        cgi_u& operator=(const cgi_u &);
        cgi_u(const cgi_u &);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------

};

} //namespace ns_hlx {

#endif
