//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
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
#include "hurl/support/ndebug.h"
#include "hurl/http/cb.h"
#include "hurl/support/nbq.h"
#include "hurl/http/resp.h"
#include "hurl/support/trace.h"
#include "hurl/status.h"
#include "http_parser/http_parser.h"
namespace ns_hurl {
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
        init(m_save);
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
        init(m_save);
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void resp::init(bool a_save)
{
        hmsg::init(a_save);
        m_type = hmsg::TYPE_RESP;
        m_p_status.clear();
        m_tls_info_protocol_str = NULL;
        m_tls_info_cipher_str =  NULL;
        m_status = HTTP_STATUS_NONE;
        if(m_http_parser_settings)
        {
                m_http_parser_settings->on_status = hp_on_status;
                m_http_parser_settings->on_message_complete = hp_on_message_complete;
                if(m_save)
                {
                        m_http_parser_settings->on_message_begin = hp_on_message_begin;
                        m_http_parser_settings->on_url = hp_on_url;
                        m_http_parser_settings->on_header_field = hp_on_header_field;
                        m_http_parser_settings->on_header_value = hp_on_header_value;
                        m_http_parser_settings->on_headers_complete = hp_on_headers_complete;
                        m_http_parser_settings->on_body = hp_on_body;
                }
                else
                {
                        m_http_parser_settings->on_message_begin = NULL;
                        m_http_parser_settings->on_url = NULL;
                        m_http_parser_settings->on_header_field = NULL;
                        m_http_parser_settings->on_header_value = NULL;
                        m_http_parser_settings->on_headers_complete = NULL;
                        m_http_parser_settings->on_body = NULL;
                }
        }
        if(m_http_parser_settings)
        {
                m_http_parser->data = this;
                http_parser_init(m_http_parser, HTTP_RESPONSE);
        }
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
        TRC_OUTPUT("HTTP/%d.%d %u ", m_http_major, m_http_minor, m_status);
        print_part(*m_q, m_p_status.m_off, m_p_status.m_len);
        TRC_OUTPUT("\r\n");
        cr_list_t::const_iterator i_k = m_p_h_list_key.begin();
        cr_list_t::const_iterator i_v = m_p_h_list_val.begin();
        for(;i_k != m_p_h_list_key.end() && i_v != m_p_h_list_val.end(); ++i_k, ++i_v)
        {
                print_part(*m_q, i_k->m_off, i_k->m_len);
                TRC_OUTPUT(": ");
                print_part(*m_q, i_v->m_off, i_v->m_len);
                TRC_OUTPUT("\r\n");
        }
        TRC_OUTPUT("\r\n");
        print_part(*m_q, m_p_body.m_off, m_p_body.m_len);
        TRC_OUTPUT("\r\n");
}
} //namespace ns_hurl {
