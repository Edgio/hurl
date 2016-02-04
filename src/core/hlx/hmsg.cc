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
        m_idx(0),
        m_headers(NULL),
        m_body(NULL),
        m_body_len(0)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hmsg::~hmsg(void)
{
        if(m_body)
        {
                free(m_body);
                m_body = NULL;
                m_body_len = 0;
        }
        if(m_headers)
        {
                delete m_headers;
        }
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

        if(m_headers)
        {
                delete m_headers;
                m_headers = NULL;
        }

        if(NULL != m_body)
        {
                // body to deal with
                free(m_body);
        }
        m_body = NULL;
        m_body_len = 0;
}

//: ----------------------------------------------------------------------------
//:                               Getters
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hmsg::type_t hmsg::get_type(void) const
{
        return m_type;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nbq *hmsg::get_q(void) const
{
        return m_q;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const char *hmsg::get_body_data(void)
{
        if(NULL == m_q){
                // nothing here yet
                return m_body;
        }
        if(NULL == m_body){
                // body not initialized yet
                m_body = copy_part(*m_q, m_p_body.m_off, m_p_body.m_len);
                m_body_len = m_p_body.m_len + 1;
        }
        return m_body;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t hmsg::get_body_len(void) const
{
        return m_body_len;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   ao_headers   Pointer to the headers object to populate.  ASSUMPTION: is valid
//: ----------------------------------------------------------------------------
void hmsg::get_headers(kv_map_list_t *ao_headers) const
{
        ns_hlx::cr_list_t::const_iterator i_k = m_p_h_list_key.begin();
        ns_hlx::cr_list_t::const_iterator i_v = m_p_h_list_val.begin();
        for(; i_k != m_p_h_list_key.end() &&
                    i_v != m_p_h_list_val.end();
            ++i_k, ++i_v)
        {
                // for each entry in the lists of headers
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
                ns_hlx::kv_map_list_t::iterator i_obj = ao_headers->find(l_h_k);
                if(i_obj != ao_headers->end()){
                        i_obj->second.push_back(l_h_v);
                } else {
                        ns_hlx::str_list_t l_list;
                        l_list.push_back(l_h_v);
                        (*ao_headers)[l_h_k] = l_list;
                }
        }
        return;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   ao_headers   Pointer to the headers object to populate.  ASSUMPTION: is valid
//: ----------------------------------------------------------------------------
const kv_map_list_t &hmsg::get_headers()
{
        if(NULL == m_headers)
        {
                // need to initialize
                m_headers = new kv_map_list_t();
                get_headers(m_headers);
        }
        return *m_headers;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t hmsg::get_idx(void) const
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
