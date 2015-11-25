//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hmsg.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    07/20/2015
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
#include "hlx/hlx.h"
#include "nbq.h"
#include "ndebug.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hmsg::hmsg(void):
        m_p_h_list_key(),
        m_p_h_list_val(),
        m_p_body(),
        m_http_major(),
        m_http_minor(),
        m_complete(false),
        m_supports_keep_alives(false),
        m_type(TYPE_NONE),
        m_q(NULL),
        m_idx(0)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hmsg::~hmsg(void)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hmsg::clear(void)
{
        m_p_h_list_key.clear();
        m_p_h_list_val.clear();
        m_p_body.clear();
        m_http_major = 0;
        m_http_minor = 0;
        m_complete = false;
        m_supports_keep_alives = false;
}

//: ----------------------------------------------------------------------------
//:                               Getters
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hmsg::type_t hmsg::get_type(void)
{
        return m_type;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nbq *hmsg::get_q(void)
{
        return m_q;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
char *hmsg::get_body_allocd(char **ao_buf, uint64_t &ao_len)
{
        *ao_buf = copy_part(*m_q, m_p_body.m_off, m_p_body.m_len);
        ao_len = m_p_body.m_len + 1;
        return *ao_buf;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
kv_map_list_t *hmsg::get_headers_allocd(void)
{
        kv_map_list_t *l_kv_map_list = new kv_map_list_t();
        ns_hlx::cr_list_t::const_iterator i_k = m_p_h_list_key.begin();
        ns_hlx::cr_list_t::const_iterator i_v = m_p_h_list_val.begin();
        for(;i_k != m_p_h_list_key.end() && i_v != m_p_h_list_val.end(); ++i_k, ++i_v)
        {
                char *l_h_k_b = copy_part(*m_q, i_k->m_off, i_k->m_len);
                std::string l_h_k = l_h_k_b;
                if(l_h_k_b)
                {
                        free(l_h_k_b);
                        l_h_k_b = NULL;
                }
                char *l_h_v_b = copy_part(*m_q, i_v->m_off, i_v->m_len);
                std::string l_h_v = l_h_v_b;
                if(l_h_v_b)
                {
                        free(l_h_v_b);
                        l_h_v_b = NULL;
                }
                ns_hlx::kv_map_list_t::iterator i_obj = l_kv_map_list->find(l_h_k);
                if(i_obj != l_kv_map_list->end())
                {
                        i_obj->second.push_back(l_h_v);
                }
                else
                {
                        ns_hlx::str_list_t l_list;
                        l_list.push_back(l_h_v);
                        (*l_kv_map_list)[l_h_k] = l_list;
                }
        }
        return l_kv_map_list;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t hmsg::get_idx(void)
{
        return m_idx;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hmsg::set_idx(uint64_t a_idx)
{
        m_idx = a_idx;
}

//: ----------------------------------------------------------------------------
//:                               Setters
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hmsg::set_q(nbq *a_q)
{
        m_q = a_q;
}

} //namespace ns_hlx {
