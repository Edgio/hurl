//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    resp.cc
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
#include "hlx/resp.h"
#include "nbq.h"
#include "ndebug.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
resp::resp(void):
        hmsg(),
        m_p_status(),
        m_tls_info_protocol_str(NULL),
        m_tls_info_cipher_str(NULL),
        m_status()
{
        m_type = hmsg::TYPE_RESP;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
resp::~resp(void)
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void resp::clear(void)
{
        hmsg::clear();
        m_p_status.clear();
        m_tls_info_protocol_str = NULL;
        m_tls_info_cipher_str =  NULL;
        m_status = HTTP_STATUS_NONE;
}

//: ----------------------------------------------------------------------------
//:                               Getters
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint16_t resp::get_status(void)
{
        return m_status;
}

//: ----------------------------------------------------------------------------
//:                               Setters
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void resp::set_status(http_status_t a_code)
{
        m_status = a_code;
}

//: ----------------------------------------------------------------------------
//:                               Debug
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void resp::show(void)
{
        m_q->reset_read();
        NDBG_OUTPUT("HTTP/%d.%d %u ", m_http_major, m_http_minor, m_status);
        print_part(*m_q, m_p_status.m_off, m_p_status.m_len);
        NDBG_OUTPUT("\r\n");
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
