//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn.h
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
#ifndef _NCONN_POOL_H
#define _NCONN_POOL_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "ndebug.h"
#include "nconn.h"
#include "nlru.h"
#include "obj_pool.h"

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------

namespace ns_hlx {

typedef nlru <nconn> idle_conn_lru_t;
typedef std::map <std::string, uint32_t> active_conn_map_t;

//: ----------------------------------------------------------------------------
//: \class: nconn_pool
//: ----------------------------------------------------------------------------
class nconn_pool
{
public:
        // -------------------------------------------------
        // Types
        // -------------------------------------------------
        typedef obj_pool<nconn> nconn_obj_pool_t;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        nconn_pool(uint32_t a_size, int32_t a_max_concurrent_conn_per_label = -1);
        ~nconn_pool();
        nconn * get(const std::string &a_label, scheme_t a_scheme);
        nconn * get_idle(const std::string &a_label);
        nconn *create_conn(scheme_t a_scheme);
        int32_t add_idle(nconn *a_nconn);
        int32_t release(nconn *a_nconn);
        int32_t cleanup(nconn *a_nconn);
        uint32_t num_in_use(void) const {return ((uint32_t)m_nconn_obj_pool.used_size());}
        uint32_t num_free(void) const {return (m_pool_size - num_in_use());}
        uint32_t num_idle(void) {return (uint32_t)m_idle_conn_ncache.size();}
        nconn_obj_pool_t &get_nconn_obj_pool(void){return m_nconn_obj_pool;}

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------
        static int delete_cb(void* o_1, void *a_2);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(nconn_pool)
        void init(void);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        nconn_obj_pool_t m_nconn_obj_pool;
        idle_conn_lru_t m_idle_conn_ncache;
        bool m_initd;
        int32_t m_pool_size;
        active_conn_map_t m_active_conn_map;
        int32_t m_max_concurrent_conn_per_label;
};

} //namespace ns_hlx {

#endif
