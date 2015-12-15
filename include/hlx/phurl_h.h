//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    phurl_h.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    12/12/2015
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
#ifndef _PHURL_H_H
#define _PHURL_H_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/hlx.h"
#include <pthread.h>

#include <set>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef struct host_struct
{
        std::string m_host;
        uint16_t m_port;
        host_struct():
                m_host(),
                m_port(0)
        {};
} host_t;

typedef std::list <host_t> host_list_t;

class hlx_resp
{
public:
        subr *m_subr;
        resp *m_resp;
        hlx_resp():
                m_subr(NULL),
                m_resp(NULL)
        {}
private:
        // Disallow copy/assign
        hlx_resp& operator=(const hlx_resp &);
        hlx_resp(const hlx_resp &);
};
typedef std::list <hlx_resp *> hlx_resp_list_t;
typedef std::set <uint64_t> resp_uid_set_t;

//: ----------------------------------------------------------------------------
//: fanout resp
//: ----------------------------------------------------------------------------
class phurl_h_resp
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        phurl_h_resp(void);
        ~phurl_h_resp(void);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        pthread_mutex_t m_mutex;
        resp_uid_set_t m_pending_uid_set;
        hlx_resp_list_t m_resp_list;

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        phurl_h_resp& operator=(const phurl_h_resp &);
        phurl_h_resp(const phurl_h_resp &);
};

//: ----------------------------------------------------------------------------
//: phurl_h
//: ----------------------------------------------------------------------------
class phurl_h: public default_rqst_h
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        phurl_h(void);
        ~phurl_h();

        h_resp_t do_get(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap);
        void add_host(const std::string a_host, uint16_t a_port = 80);
        subr &get_subr_template(void);

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------
        static int32_t create_resp(hlx &a_hlx, subr &a_subr, phurl_h_resp *l_fanout_resp);
        static int32_t s_completion_cb(hlx &a_hlx, subr &a_subr, nconn &a_nconn, resp &a_resp);
        static int32_t s_error_cb(hlx &a_hlx, subr &a_subr, nconn &a_nconn);
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        phurl_h& operator=(const phurl_h &);
        phurl_h(const phurl_h &);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        subr m_subr_template;
        host_list_t m_host_list;
};

} //namespace ns_hlx {

#endif
