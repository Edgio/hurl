//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn_pool.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    05/27/2015
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

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "nconn_pool.h"
#include "nconn_tcp.h"
#include "nconn_ssl.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *nconn_pool::create_conn(scheme_t a_scheme)
{
        nconn *l_nconn = NULL;

        //NDBG_PRINT("CREATING NEW CONNECTION: a_scheme: %d\n", a_scheme);
        if(a_scheme == SCHEME_TCP)
        {
                l_nconn = new nconn_tcp();
        }
        else if(a_scheme == SCHEME_SSL)
        {
                l_nconn = new nconn_ssl();
        }

        return l_nconn;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *nconn_pool::get(scheme_t a_scheme)
{
        if(!m_initd)
        {
                init();
        }

        //NDBG_PRINT("TID[%lu]: %sGET_CONNECTION%s: l_nconn: %p m_nconn_obj_pool.used_size() = %lu\n",
        //                pthread_self(),
        //                ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF,
        //                ao_nconn, m_nconn_obj_pool.used_size());
        if((m_pool_size != -1) &&
            m_idle_conn_ncache.size() &&
           (m_nconn_obj_pool.used_size() >= (uint64_t)m_pool_size))
        {
                // Evict from idle_conn_ncache
                m_idle_conn_ncache.evict();
        }

        if(m_nconn_obj_pool.used_size() >= (uint64_t)m_pool_size)
        {
                // TODO REMOVE
                //NDBG_PRINT("%sEAGAIN%s\n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF);
                return NULL;
        }

        // -----------------------------------------
        // Get connection for reqlet
        // Either return existing connection or
        // null for new connection
        // -----------------------------------------
        nconn *l_nconn = m_nconn_obj_pool.get_free();
        //NDBG_PRINT("%sGET_CONNECTION%s: GET[%u] l_nconn: %p m_conn_free_list.size() = %lu\n",
        //                ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, l_conn_idx, l_nconn, m_conn_idx_free_list.size());
        if(l_nconn &&
           (l_nconn->m_scheme != a_scheme))
        {
                // Destroy nconn and recreate
                //NDBG_PRINT("Destroy nconn and recreate: %u -- l_nconn->m_scheme: %d -- a_scheme: %d\n",
                //                *i_conn,
                //                l_nconn->m_scheme,
                //                a_scheme);
                delete l_nconn;
                l_nconn = NULL;
        }

        if(!l_nconn)
        {
                l_nconn = create_conn(a_scheme);
                if(!l_nconn)
                {
                        NDBG_PRINT("Error performing create_conn\n");
                        return NULL;
                }
                m_nconn_obj_pool.add(l_nconn);
        }

        //NDBG_PRINT("%sGET_CONNECTION%s: ERASED[%u] l_nconn: %p m_conn_free_list.size() = %lu\n",
        //                ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_conn_idx, l_nconn, m_conn_idx_free_list.size());
        return l_nconn;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *nconn_pool::get_idle(const std::string &a_host, scheme_t a_scheme)
{
        //NDBG_PRINT("%sGET_CONNECTION%s: a_host: %s\n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, a_host.c_str());
        if(!m_initd)
        {
                init();
        }

        // TODO --fix using label...
        std::string l_label;
        if(a_scheme == SCHEME_TCP)
        {
                l_label = "http://";
        }
        else if(a_scheme == SCHEME_SSL)
        {
                l_label = "https://";
        }
        l_label += a_host;

        // ---------------------------------------
        // Try grab from conn cache
        // ---------------------------------------
        //NDBG_PRINT("%sGET_CONNECTION%s: m_idle_conn_ncache.size(): %d\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, (int)m_idle_conn_ncache.size());
        //NDBG_PRINT("%sGET_CONNECTION%s: l_label:                   %s\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, l_label.c_str());
        // Lookup label
        //NDBG_PRINT(" ::NCACHE::%sINSERT%s: LABEL[%s] --size: %d\n", ANSI_COLOR_FG_WHITE, ANSI_COLOR_OFF, l_label.c_str(), (int)m_idle_conn_ncache.size());
        nconn* l_nconn_lkp = m_idle_conn_ncache.get(l_label);
        if(l_nconn_lkp)
        {
                //NDBG_PRINT("%sGET_IDLE%s: a_nconn: %p\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, l_nconn_lkp);
                //NDBG_PRINT(" ::NCACHE::%sINSERT%s: LABEL[%s] --size: %d\n", ANSI_COLOR_FG_WHITE, ANSI_COLOR_OFF, l_label.c_str(), (int)m_idle_conn_ncache.size());
                return l_nconn_lkp;
        }
        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_pool::add_idle(nconn *a_nconn)
{
        //NDBG_PRINT("%sADD_IDLE%s: a_nconn: %p\n", ANSI_COLOR_FG_MAGENTA, ANSI_COLOR_OFF, a_nconn);
        if(!m_initd)
        {
                init();
        }

        if(!a_nconn)
        {
                NDBG_PRINT("Error a_nconn == NULL\n");
                return STATUS_ERROR;
        }

        // TODO --fix using label...
        std::string l_label;
        if(a_nconn->m_scheme == SCHEME_TCP)
        {
                l_label = "http://";
        }
        else if(a_nconn->m_scheme == SCHEME_SSL)
        {
                l_label = "https://";
        }
        l_label += a_nconn->m_host;
        id_t l_id = m_idle_conn_ncache.insert(l_label, a_nconn);
        //NDBG_PRINT(" ::NCACHE::%sINSERT%s: LABEL[%s] --size: %d\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, l_label.c_str(), (int)m_idle_conn_ncache.size());
        a_nconn->set_id(l_id);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_pool::cleanup(nconn *a_nconn)
{
        //NDBG_PRINT("%sRELEASE%s:\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF);
        if(!m_initd)
        {
                init();
        }

        if(!a_nconn)
        {
                NDBG_PRINT("Error a_nconn == NULL\n");
                return STATUS_ERROR;
        }

        if(STATUS_OK != a_nconn->nc_cleanup())
        {
                NDBG_PRINT("Error a_nconn == NULL\n");
                return STATUS_ERROR;
        }

        a_nconn->reset_stats();
        m_nconn_obj_pool.release(a_nconn);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int nconn_pool::delete_cb(void* o_1, void *a_2)
{
        //NDBG_PRINT("::NCACHE::%sDELETE%s: %p %p\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, o_1, a_2);
        nconn_pool *l_nconn_pool = reinterpret_cast<nconn_pool *>(o_1);
        nconn *l_nconn = reinterpret_cast<nconn *>(a_2);
        int32_t l_status = l_nconn_pool->cleanup(l_nconn);
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error performing cleanup\n");
                return STATUS_ERROR;
        }
        l_nconn_pool->get_nconn_obj_pool().release(l_nconn);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_pool::release(nconn *a_nconn)
{
        //NDBG_PRINT("%sRELEASE%s:\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF);
        if(!m_initd)
        {
                init();
        }

        if(!a_nconn)
        {
                NDBG_PRINT("Error a_nconn == NULL\n");
                return STATUS_ERROR;
        }

        if(STATUS_OK != cleanup(a_nconn))
        {
                NDBG_PRINT("Error performing cleanup\n");
                return STATUS_ERROR;
        }

        if(m_idle_conn_ncache.size() &&
           a_nconn->m_data)
        {
                m_idle_conn_ncache.remove(a_nconn->get_id());
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nconn_pool::init(void)
{
        m_idle_conn_ncache.set_delete_cb(delete_cb, this);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn_pool::nconn_pool(int32_t a_size):
                       m_nconn_obj_pool(),
                       m_idle_conn_ncache(a_size),
                       m_initd(false),
                       m_pool_size(a_size)
{
        //NDBG_PRINT("a_size: %d\n", a_size);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn_pool::~nconn_pool(void)
{
        // TODO rethink this...
#if 0
        for(nconn_obj_pool_t::obj_idx_set_t::iterator i_idx = m_nconn_obj_pool.m_used_idx_set.begin();
                        i_idx != m_nconn_obj_pool.m_used_idx_set.end();
                        ++i_idx)
        {
                int32_t l_status = cleanup(m_nconn_obj_pool.m_obj_vec[*i_idx]);
                if(l_status != STATUS_OK)
                {
                        NDBG_PRINT("Error performing cleanup\n");
                }
        }
#endif
}

} //namespace ns_hlx {

