//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    api_resp.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    08/29/2015
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
#include "string_util.h"

#include <string.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: nbq utilities
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nbq_write_request_line(nbq &ao_q, const char *a_buf, uint32_t a_len)
{
        ao_q.write(a_buf, a_len);
        ao_q.write("\r\n", strlen("\r\n"));
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nbq_write_status(nbq &ao_q, http_status_t a_status)
{
        // TODO all statuses???
        switch(a_status)
        {
        case HTTP_STATUS_OK:
        {
                ao_q.write("HTTP/1.1 200 OK\r\n", strlen("HTTP/1.1 200 OK\r\n"));
                break;
        }
        case HTTP_STATUS_NOT_FOUND:
        {
                ao_q.write("HTTP/1.1 404 Not Found\r\n", strlen("HTTP/1.1 404 Not Found\r\n"));
                break;
        }
        case HTTP_STATUS_NOT_IMPLEMENTED:
        {
                ao_q.write("HTTP/1.1 501 Not Implemented\r\n", strlen("HTTP/1.1 501 Not Implemented\r\n"));
                break;
        }
        default:
        {
                ao_q.write("HTTP/1.1 400 Bad Request\r\n", strlen("HTTP/1.1 400 Bad Request\r\n"));
                break;
        }
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nbq_write_header(nbq &ao_q,
                         const char *a_key_buf, uint32_t a_key_len,
                         const char *a_val_buf, uint32_t a_val_len)
{
        ao_q.write(a_key_buf, a_key_len);
        ao_q.write(": ", 2);
        ao_q.write(a_val_buf, a_val_len);
        ao_q.write("\r\n", 2);
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nbq_write_header(nbq &ao_q,
                         const char *a_key_buf,
                         const char *a_val_buf)
{
        nbq_write_header(ao_q, a_key_buf, strlen(a_key_buf), a_val_buf, strlen(a_val_buf));
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nbq_write_body(nbq &ao_q, const char *a_buf, uint32_t a_len)
{
        ao_q.write("\r\n", strlen("\r\n"));
        ao_q.write(a_buf, a_len);
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
api_resp::api_resp():
                m_status(HTTP_STATUS_OK),
                m_headers(),
                m_body_data(NULL),
                m_body_data_len(0)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
api_resp::~api_resp()
{
        if(m_body_data)
        {
                m_body_data = NULL;
                m_body_data_len = 0;
        }
}

//: ----------------------------------------------------------------------------
//:                            Set Response
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void api_resp::set_status(http_status_t a_status)
{
        m_status = a_status;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int api_resp::set_header(const std::string &a_header)
{
        int32_t l_status;
        std::string l_header_key;
        std::string l_header_val;
        l_status = break_header_string(a_header, l_header_key, l_header_val);
        if(l_status != 0)
        {
                // If verbose???
                //printf("Error header string[%s] is malformed\n", a_header.c_str());
                return -1;
        }
        l_status = set_header(l_header_key, l_header_val);
        {
                // If verbose???
                //printf("Error header string[%s] is malformed\n", a_header.c_str());
                return -1;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int api_resp::set_header(const std::string &a_key, const std::string &a_val)
{
        kv_map_list_t::iterator i_obj = m_headers.find(a_key);
        if(i_obj != m_headers.end())
        {
                i_obj->second.push_back(a_val);
        }
        else
        {
                str_list_t l_list;
                l_list.push_back(a_val);
                m_headers[a_key] = l_list;
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void api_resp::set_body_data(const char *a_ptr, uint32_t a_len)
{
        m_body_data = a_ptr;
        m_body_data_len = a_len;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void api_resp::clear_headers(void)
{
        m_headers.clear();
}

//: ----------------------------------------------------------------------------
//: \details: Serialize to q for sending.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t api_resp::serialize(nbq &ao_q)
{
        // -------------------------------------------
        // Write status
        // -------------------------------------------
        nbq_write_status(ao_q, m_status);

        // -------------------------------------------
        // Write headers
        // -------------------------------------------
        // Loop over reqlet map
        for(kv_map_list_t::const_iterator i_hl = m_headers.begin();
            i_hl != m_headers.end();
            ++i_hl)
        {
                if(i_hl->first.empty() || i_hl->second.empty())
                {
                        continue;
                }
                for(str_list_t::const_iterator i_v = i_hl->second.begin();
                    i_v != i_hl->second.end();
                    ++i_v)
                {
                        nbq_write_header(ao_q, i_hl->first.c_str(), i_hl->first.length(), i_v->c_str(), i_v->length());
                }
        }

        // -------------------------------------------
        // Write body
        // -------------------------------------------
        if(m_body_data && m_body_data_len)
        {
                //NDBG_PRINT("Write: buf: %p len: %d\n", l_buf, l_len);
                nbq_write_body(ao_q, m_body_data, m_body_data_len);
        }
        else
        {
                nbq_write_body(ao_q, NULL, 0);
        }

        return 0;
}

} //namespace ns_hlx {
