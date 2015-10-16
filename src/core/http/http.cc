//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    http.cc
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
#include "nconn.h"
#include "nbq.h"
#include <string.h>

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define PARSE_IF_UNPARSED() do { \
        if(!m_url_parsed) \
        { \
                if(parse_url() != STATUS_OK) \
                { \
                        NDBG_PRINT("Error performing parse_url\n"); \
                } \
        } \
} while(0)

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
http_req::http_req(void):
        m_p_url(),
        m_p_status(),
        m_p_h_list_key(),
        m_p_h_list_val(),
        m_p_body(),
        m_http_major(),
        m_http_minor(),
        m_method(),
        m_status(),
        m_complete(false),
        m_supports_keep_alives(false),
        m_subreq(NULL),
        m_headers(),
        m_body(),
        m_q(NULL),
        m_url_parsed(false),
        m_parsed_url_path()
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
http_req::~http_req()
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void http_req::clear(void)
{
        m_p_url.clear();
        m_p_status.clear();
        m_p_h_list_key.clear();
        m_p_h_list_val.clear();
        m_p_body.clear();
        m_status = 0;
        m_complete = false;
        m_supports_keep_alives = false;
        m_headers.clear();
        m_body.clear();

        // Parsed members
        m_url_parsed = false;
        m_parsed_url_path.clear();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const kv_map_list_t &http_req::get_headers(void)
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
const std::string &http_req::get_body(void)
{
        if(!m_body.empty())
        {
                return m_body;
        }

        // Else parse...
        m_body.assign(m_p_body.m_ptr, m_p_body.m_len);
        return m_body;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_req::parse_url(void)
{
        if(m_url_parsed)
        {
                return STATUS_OK;
        }

        m_parsed_url_path.assign(m_p_url.m_ptr, m_p_url.m_len);
        m_url_parsed = true;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &http_req::get_url_path(void)
{
        PARSE_IF_UNPARSED();
        return m_parsed_url_path;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_req::write_request_line(char *a_buf, uint32_t a_len)
{
        //NDBG_PRINT("WRITE_HEADER: %s: %s\n", a_key, a_val);
        ns_hlx::nbq *l_q = get_q();
        if(!l_q)
        {
                NDBG_PRINT("Error getting response q");
                return STATUS_ERROR;
        }
        l_q->write(a_buf, a_len);
        l_q->write("\r\n", 2);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_req::write_header(const char *a_key, const char *a_val)
{
        return write_header(a_key, strlen(a_key), a_val, strlen(a_val));
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_req::write_header(const char *a_key, uint32_t a_key_len, const char *a_val, uint32_t a_val_len)
{
        //NDBG_PRINT("WRITE_HEADER: %s: %s\n", a_key, a_val);
        ns_hlx::nbq *l_q = get_q();
        if(!l_q)
        {
                NDBG_PRINT("Error getting response q");
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
int32_t http_req::write_body(const char *a_body, uint32_t a_body_len)
{
        ns_hlx::nbq *l_q = get_q();
        if(!l_q)
        {
                NDBG_PRINT("Error getting response q");
                return STATUS_ERROR;
        }
        l_q->write("\r\n", strlen("\r\n"));
        l_q->write(a_body, a_body_len);
        return STATUS_OK;

        return STATUS_OK;
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
                NDBG_PRINT("Error getting response q");
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
                NDBG_PRINT("Error getting response q");
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
                NDBG_PRINT("Error getting response q");
                return STATUS_ERROR;
        }
        l_q->write("\r\n", strlen("\r\n"));
        l_q->write(a_body, a_body_len);
        return STATUS_OK;

        return STATUS_OK;
}

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
        // TODO Hack to support getting connection meta
        m_conn_info(),
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

        // TODO Hack to support getting connection meta
        m_conn_info.clear();

        m_complete = false;
        m_headers.clear();
        m_body.clear();

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
const std::string &http_resp::get_body(void)
{
        if(!m_body.empty())
        {
                return m_body;
        }

        // Else parse...
        m_body.assign(m_p_body.m_ptr, m_p_body.m_len);
        return m_body;
}

} //namespace ns_hlx {
