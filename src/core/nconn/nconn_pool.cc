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
#include "nconn_tls.h"
#include "ndebug.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn_pool::nconn_pool(uint64_t a_max_active_size,
                       uint64_t a_max_idle_size):
                       m_initd(false),
                       m_active_conn_map(),
                       m_active_conn_map_size(0),
                       m_active_conn_map_max_size(a_max_active_size),
                       m_idle_conn_ncache(a_max_idle_size),
                       m_nconn_obj_pool()
{
        //NDBG_PRINT("a_max_active_size: %d\n", a_max_active_size);
        //NDBG_PRINT("a_max_idle_size:   %d\n", a_max_idle_size);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn_pool::~nconn_pool(void)
{
        while(m_idle_conn_ncache.size())
        {
                m_idle_conn_ncache.evict();
        }
        // TODO kill active connections...
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *nconn_pool::get_new_active(const std::string &a_label, scheme_t a_scheme)
{
        if(!m_initd)
        {
                init();
        }
        if(m_active_conn_map_size >= m_active_conn_map_max_size)
        {
                return NULL;
        }
        nconn *l_nconn = m_nconn_obj_pool.get_free();
        // If scheme is different, destroy nconn and recreate
        if(l_nconn &&
           (l_nconn->get_scheme() != a_scheme))
        {
                delete l_nconn;
                l_nconn = NULL;
        }
        if(!l_nconn)
        {
                l_nconn = s_create_new_conn(a_scheme);
                if(!l_nconn)
                {
                        NDBG_PRINT("Error performing create_conn\n");
                        return NULL;
                }
                m_nconn_obj_pool.add(l_nconn);
        }
        l_nconn->set_label(a_label);

        int32_t l_s;
        l_s = add_active(l_nconn);
        if(l_s != STATUS_OK)
        {
                NDBG_PRINT("Error performing add_active.\n");
                //return NULL;
        }
        return l_nconn;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t nconn_pool::get_active_size(void)
{
        return m_active_conn_map_size;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t nconn_pool::get_active_available(void)
{
        return m_active_conn_map_max_size - m_active_conn_map_size;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *nconn_pool::get_idle(const std::string &a_label)
{
        if(!m_initd)
        {
                init();
        }
        //NDBG_PRINT("m_idle_conn_ncache size: %lu\n", m_idle_conn_ncache.size());
        nconn* l_c = m_idle_conn_ncache.get(a_label);
        if(l_c)
        {
                add_active(l_c);
                return l_c;
        }
        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t nconn_pool::get_idle_size(void)
{
        return m_idle_conn_ncache.size();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_pool::add_idle(nconn *a_nconn)
{
        if(!m_initd)
        {
                init();
        }
        if(!a_nconn)
        {
                //NDBG_PRINT("Error a_nconn == NULL\n");
                return STATUS_ERROR;
        }
        int32_t l_s;
        l_s = remove_active(a_nconn);
        if(l_s != STATUS_OK)
        {
                NDBG_PRINT("Error performing remove_active.\n");
                return STATUS_ERROR;
        }
        id_t l_id;
        l_id = m_idle_conn_ncache.insert(a_nconn->get_label(), a_nconn);
        a_nconn->set_id(l_id);
        //NDBG_PRINT("%sADD_IDLE%s: size: %lu LABEL: %s ID: %u\n",
        //                ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF,
        //                m_idle_conn_ncache.size(),
        //                a_nconn->get_label().c_str(),
        //                l_id);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_pool::release(nconn *a_nconn)
{
        if(!a_nconn)
        {
                //NDBG_PRINT("Error a_nconn == NULL\n");
                return STATUS_ERROR;
        }
        //NDBG_PRINT("%sRELEASE%s:\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF);
        if(!m_initd)
        {
                init();
        }
        int32_t l_s;
        l_s = remove_active(a_nconn);
        if(l_s != STATUS_OK)
        {
                NDBG_PRINT("Error performing remove_active\n");
                //return STATUS_ERROR;
        }
        l_s = remove_idle(a_nconn);
        if(l_s != STATUS_OK)
        {
                NDBG_PRINT("Error performing remove_idle\n");
                //return STATUS_ERROR;
        }
        l_s = cleanup(a_nconn);
        if(l_s != STATUS_OK)
        {
                NDBG_PRINT("Error performing cleanup\n");
                //return STATUS_ERROR;
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *nconn_pool::s_create_new_conn(scheme_t a_scheme)
{
        nconn *l_nconn = NULL;
        //NDBG_PRINT("CREATING NEW CONNECTION: a_scheme: %d\n", a_scheme);
        if(a_scheme == SCHEME_TCP)
        {
                l_nconn = new nconn_tcp();
        }
        else if(a_scheme == SCHEME_TLS)
        {
                l_nconn = new nconn_tls();
        }
        return l_nconn;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int nconn_pool::s_delete_cb(void* o_1, void *a_2)
{
        nconn_pool *l_nconn_pool = reinterpret_cast<nconn_pool *>(o_1);
        nconn *l_nconn = reinterpret_cast<nconn *>(a_2);
        int32_t l_s;
        l_s = l_nconn_pool->cleanup(l_nconn);
        if(l_s != STATUS_OK)
        {
                NDBG_PRINT("Error performing cleanup\n");
                //return STATUS_ERROR;
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
        m_idle_conn_ncache.set_delete_cb(s_delete_cb, this);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_pool::add_active(nconn *a_nconn)
{
        active_conn_map_t::iterator i_cl = m_active_conn_map.find(a_nconn->get_label());
        if(i_cl != m_active_conn_map.end())
        {
                i_cl->second.insert(a_nconn);
        }
        else
        {
                nconn_set_t l_cs;
                l_cs.insert(a_nconn);
                m_active_conn_map[a_nconn->get_label()] = l_cs;
        }
        ++m_active_conn_map_size;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_pool::remove_active(nconn *a_nconn)
{
        //if(m_active_conn_map.find(a_nconn->get_label()) != m_active_conn_map.end())
        //{
        //        --m_active_conn_map[a_nconn->get_label()];
        //}
        active_conn_map_t::iterator i_cl = m_active_conn_map.find(a_nconn->get_label());
        if(i_cl == m_active_conn_map.end())
        {
                return STATUS_ERROR;
        }
        nconn_set_t::iterator i_n = i_cl->second.find(a_nconn);
        if(i_n == i_cl->second.end())
        {
                return STATUS_ERROR;
        }
        i_cl->second.erase(i_n);
        if(!i_cl->second.size())
        {
                m_active_conn_map.erase(i_cl);
        }
        --m_active_conn_map_size;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_pool::remove_idle(nconn *a_nconn)
{
        if(m_idle_conn_ncache.size() &&
           a_nconn->get_data())
        {
                uint64_t l_id = a_nconn->get_id();
                m_idle_conn_ncache.remove(l_id);
                //NDBG_PRINT("%sDEL_IDLE%s: size: %lu ID: %lu\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF,
                //           m_idle_conn_ncache.size(),
                //           l_id);
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_pool::cleanup(nconn *a_nconn)
{
        if(!a_nconn)
        {
                NDBG_PRINT("Error a_nconn == NULL\n");
                return STATUS_ERROR;
        }
        if(!m_initd)
        {
                init();
        }
        int32_t l_s;
        l_s = a_nconn->nc_cleanup();
        if(l_s != STATUS_OK)
        {
                NDBG_PRINT("Error perfrorming a_nconn->nc_cleanup()\n");
                //return STATUS_ERROR;
        }
        a_nconn->reset_stats();
        m_nconn_obj_pool.release(a_nconn);
        return STATUS_OK;
}

} //namespace ns_hlx {

