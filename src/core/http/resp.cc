//! ----------------------------------------------------------------------------
//! Copyright Verizon.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include "support/ndebug.h"
#include "http/cb.h"
#include "support/nbq.h"
#include "http/resp.h"
#include "support/trace.h"
#include "status.h"
#include "http_parser/http_parser.h"
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
resp::resp(void):
        hmsg(),
        m_p_status(),
        m_tls_info_protocol_str(NULL),
        m_tls_info_cipher_str(NULL),
        m_status()
{
        init(m_save);
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
resp::~resp(void)
{
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
void resp::clear(void)
{
        init(m_save);
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
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
//! ----------------------------------------------------------------------------
//!                               Getters
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
uint16_t resp::get_status(void)
{
        return m_status;
}
//! ----------------------------------------------------------------------------
//!                               Setters
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
void resp::set_status(http_status_t a_code)
{
        m_status = a_code;
}
//! ----------------------------------------------------------------------------
//!                               Debug
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
void resp::show(bool a_color)
{
        m_q->reset_read();
        if(a_color) TRC_OUTPUT("%s", ANSI_COLOR_FG_BLUE);
        TRC_OUTPUT("HTTP/%d.%d %u ", m_http_major, m_http_minor, m_status);
        if(a_color) TRC_OUTPUT("%s", ANSI_COLOR_FG_GREEN);
        print_part(*m_q, m_p_status.m_off, m_p_status.m_len);
        if(a_color) TRC_OUTPUT("%s", ANSI_COLOR_OFF);
        TRC_OUTPUT("\r\n");
        cr_list_t::const_iterator i_k = m_p_h_list_key.begin();
        cr_list_t::const_iterator i_v = m_p_h_list_val.begin();
        for(;i_k != m_p_h_list_key.end() && i_v != m_p_h_list_val.end(); ++i_k, ++i_v)
        {
                if(a_color) TRC_OUTPUT("%s", ANSI_COLOR_FG_BLUE);
                print_part(*m_q, i_k->m_off, i_k->m_len);
                if(a_color) TRC_OUTPUT("%s", ANSI_COLOR_OFF);
                TRC_OUTPUT(": ");
                if(a_color) TRC_OUTPUT("%s", ANSI_COLOR_FG_GREEN);
                print_part(*m_q, i_v->m_off, i_v->m_len);
                if(a_color) TRC_OUTPUT("%s", ANSI_COLOR_OFF);
                TRC_OUTPUT("\r\n");
        }
        TRC_OUTPUT("\r\n");
        print_part(*m_q, m_p_body.m_off, m_p_body.m_len);
        TRC_OUTPUT("\r\n");
}
} //namespace ns_hurl {
