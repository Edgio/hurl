//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    http_resp.cc
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
#include "ndebug.h"
#include "nbq.h"
#include <string.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
http_resp::http_resp(void):
        m_p_status(),
        m_p_h_list_key(),
        m_p_h_list_val(),
        m_p_body(),
        m_complete(false),
        m_ssl_info_protocol_str(NULL),
        m_ssl_info_cipher_str(NULL),
        m_status(0),
        m_headers(),
        m_body(),
        m_q(NULL)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
http_resp::~http_resp()
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void http_resp::clear(void)
{
        m_p_status.clear();
        m_p_h_list_key.clear();
        m_p_h_list_val.clear();
        m_p_body.clear();
        m_status = 0;
        m_ssl_info_protocol_str = NULL;
        m_ssl_info_cipher_str = NULL;
        m_complete = false;
        m_headers.clear();
        m_body.clear();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void http_resp::show(void)
{
        NDBG_OUTPUT("%u %.*s\r\n", m_status, m_p_status.m_len, m_p_status.m_ptr);
        cr_list_t::const_iterator i_k = m_p_h_list_key.begin();
        cr_list_t::const_iterator i_v = m_p_h_list_val.begin();
        for(;i_k != m_p_h_list_key.end() && i_v != m_p_h_list_val.end(); ++i_k, ++i_v)
        {
                NDBG_OUTPUT("%.*s: %.*s\r\n", i_k->m_len, i_k->m_ptr, i_v->m_len, i_v->m_ptr);
        }
        NDBG_OUTPUT("\r\n%.*s\r\n", m_p_body.m_len, m_p_body.m_ptr);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void http_resp::show_headers(void)
{
        NDBG_OUTPUT("%u %.*s\r\n", m_status, m_p_status.m_len, m_p_status.m_ptr);
        cr_list_t::const_iterator i_k = m_p_h_list_key.begin();
        cr_list_t::const_iterator i_v = m_p_h_list_val.begin();
        for(;i_k != m_p_h_list_key.end() && i_v != m_p_h_list_val.end(); ++i_k, ++i_v)
        {
                NDBG_OUTPUT("%.*s: %.*s\r\n", i_k->m_len, i_k->m_ptr, i_v->m_len, i_v->m_ptr);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_resp::write_status(http_status_t a_status)
{
        // TODO Make map...
        ns_hlx::nbq *l_q = get_q();
        if(!l_q)
        {
                NDBG_PRINT("Error getting response\n");
                return STATUS_ERROR;
        }
        switch(a_status)
        {
        case HTTP_STATUS_OK:
        {
                l_q->write("HTTP/1.1 200 OK\r\n", strlen("HTTP/1.1 200 OK\r\n"));
                break;
        }
        case HTTP_STATUS_NOT_FOUND:
        {
                l_q->write("HTTP/1.1 404 Not Found\r\n", strlen("HTTP/1.1 404 Not Found\r\n"));
                break;
        }
        default:
        {
                break;
        }
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_resp::write_header(const char *a_key, const char *a_val)
{
        return write_header(a_key, strlen(a_key), a_val, strlen(a_val));
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_resp::write_header(const char *a_key, uint32_t a_key_len, const char *a_val, uint32_t a_val_len)
{
        //NDBG_PRINT("WRITE_HEADER: %s: %s\n", a_key, a_val);
        ns_hlx::nbq *l_q = get_q();
        if(!l_q)
        {
                NDBG_PRINT("Error getting response q\n");
                return STATUS_ERROR;
        }
        l_q->write(a_key, a_key_len);
        l_q->write(": ", 2);
        l_q->write(a_val, a_val_len);
        l_q->write("\r\n", 2);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_resp::write_body(const char *a_body, uint32_t a_body_len)
{
        ns_hlx::nbq *l_q = get_q();
        if(!l_q)
        {
                NDBG_PRINT("Error getting response q\n");
                return STATUS_ERROR;
        }
        l_q->write("\r\n", strlen("\r\n"));
        l_q->write(a_body, a_body_len);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const kv_map_list_t &http_resp::get_headers(void)
{
        if(!m_headers.empty())
        {
                return m_headers;
        }

        // TODO how to deal with empty headers???
        // for now assuming same length
        // -->headers wo values will muck this up.
        cr_list_t::const_iterator i_key;
        cr_list_t::const_iterator i_val;
        for(i_key = m_p_h_list_key.begin(), i_val = m_p_h_list_val.begin();
            (i_key != m_p_h_list_key.end()) && (i_val != m_p_h_list_val.end());
            ++i_key, ++i_val)
        {
                std::string l_key = std::string(i_key->m_ptr, i_key->m_len);
                std::string l_val = std::string(i_val->m_ptr, i_val->m_len);
                kv_map_list_t::iterator i_key_list = m_headers.find(l_key);
                if(i_key_list == m_headers.end())
                {
                        str_list_t l_list;
                        l_list.push_back(l_val);
                        m_headers[l_key] = l_list;
                }
                else
                {
                        i_key_list->second.push_back(l_val);
                }
        }
        // Else parse...
        return m_headers;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_resp::get_body(char **a_buf, uint32_t &a_len)
{
        // Read from offset
        a_len = m_p_body.m_len;
        m_q->reset_read();
        uint32_t l_offset = (uint32_t)(m_p_body.m_ptr - m_q->b_read_ptr());
        m_q->b_read_incr(l_offset);
        NDBG_PRINT("MQ_READ: l_offset: %u\n", l_offset);
        NDBG_PRINT("MQ_READ: a_len:    %u\n", a_len);
        *a_buf = (char *)malloc(sizeof(char)*a_len);
        m_q->read(*a_buf, a_len);
        return a_len;
}

} //namespace ns_hlx {
