//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
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
#include "nlru.h"
#include "obj_pool.h"
#include <set>
#include "hlx/scheme.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------
class nconn;

//: ----------------------------------------------------------------------------
//: \class: nconn_pool
//: ----------------------------------------------------------------------------
class nconn_pool
{
public:
        // -------------------------------------------------
        // Types
        // -------------------------------------------------
        typedef nlru <nconn> idle_conn_lru_t;
        typedef std::set<nconn *> nconn_set_t;
        typedef std::map <std::string, nconn_set_t> active_conn_map_t;
        typedef obj_pool<nconn> nconn_obj_pool_t;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        nconn_pool(uint32_t a_id,
                   uint64_t a_max_active_size,
                   uint64_t a_max_idle_size);
        ~nconn_pool();
        void evict_all_idle(void);
        nconn * get_new_active(const std::string &a_label, scheme_t a_scheme);
        uint64_t get_active_size(void);
        uint64_t get_active_available(void);
        uint64_t get_active_label(const std::string &a_label);
        nconn * get_idle(const std::string &a_label);
        uint64_t get_idle_size(void);
        uint32_t get_id(void) { return m_id;}
        int32_t add_idle(nconn *a_nconn);
        int32_t release(nconn *a_nconn);

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------
        static nconn *s_create_new_conn(scheme_t a_scheme);
        static int s_delete_cb(void* o_1, void *a_2);
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        nconn_pool& operator=(const nconn_pool &);
        nconn_pool(const nconn_pool &);

        void init(void);
        int32_t add_active(nconn *a_nconn);
        int32_t remove_active(nconn *a_nconn);
        int32_t remove_idle(nconn *a_nconn);
        int32_t cleanup(nconn *a_nconn);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        uint32_t m_id;
        bool m_initd;

        // Active connections
        active_conn_map_t m_active_conn_map;
        uint64_t m_active_conn_map_size;
        uint64_t m_active_conn_map_max_size;

        // Idle connections
        idle_conn_lru_t m_idle_conn_ncache;

        // Connection pool for reuse
        nconn_obj_pool_t m_nconn_obj_pool;

};

} //namespace ns_hlx {

#endif
