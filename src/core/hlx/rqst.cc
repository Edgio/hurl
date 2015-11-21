//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    rqst.cc
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
rqst::rqst(void):
        hmsg(),
        m_url(),
        m_p_url(),
        m_method(),
        m_path()
{
        m_type = hmsg::TYPE_RQST;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
rqst::~rqst(void)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void rqst::clear(void)
{
        hmsg::clear();
        m_url.clear();
        m_p_url.clear();
        m_method = 0;
        m_path.clear();
}

//: ----------------------------------------------------------------------------
//:                               Getters
//: ----------------------------------------------------------------------------
const std::string &rqst::get_path()
{
        if(m_path.empty())
        {
                char *l_path = copy_part(*m_q, m_p_url.m_off, m_p_url.m_len);
                if(l_path)
                {
                        m_path = l_path;
                        free(l_path);
                }
        }
        return m_path;
}


//: ----------------------------------------------------------------------------
//:                               Setters
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//:                               Debug
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void rqst::show(void)
{
        cr_list_t::const_iterator i_k = m_p_h_list_key.begin();
        cr_list_t::const_iterator i_v = m_p_h_list_val.begin();
        for(;i_k != m_p_h_list_key.end() && i_v != m_p_h_list_val.end(); ++i_k, ++i_v)
        {
                print_part(*m_q, i_k->m_off, i_k->m_len);
                NDBG_OUTPUT(": ");
                print_part(*m_q, i_v->m_off, i_v->m_len);
                NDBG_OUTPUT("\r\n");
        }
        NDBG_OUTPUT("\r\n");
        print_part(*m_q, m_p_body.m_off, m_p_body.m_len);
        NDBG_OUTPUT("\r\n");
}

} //namespace ns_hlx {
