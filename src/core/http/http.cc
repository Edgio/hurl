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
#include "http.h"
#include "ndebug.h"
#include "nconn.h"
#include "util.h"
#include "http_parser/http_parser.h"
#include "nbq.h"
#include <string.h>

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

//: ----------------------------------------------------------------------------
//: http-parser callbacks
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_message_begin(http_parser* a_parser)
{
        nconn *l_conn = static_cast <nconn *>(a_parser->data);
        if(l_conn)
        {
                if(l_conn->m_verbose)
                {
                        NDBG_OUTPUT("%s: message begin\n", l_conn->m_host.c_str());
                }
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_url(http_parser* a_parser, const char *a_at, size_t a_length)
{
        nconn *l_conn = static_cast <nconn *>(a_parser->data);
        if(l_conn)
        {
                if(l_conn->m_verbose)
                {
                        if(l_conn->m_color)
                        {
                                NDBG_OUTPUT("%s: url:    %s%.*s%s\n", l_conn->m_host.c_str(), ANSI_COLOR_FG_YELLOW, (int)a_length, a_at, ANSI_COLOR_OFF);
                        }
                        else
                        {
                                NDBG_OUTPUT("%s: url:    %.*s\n", l_conn->m_host.c_str(), (int)a_length, a_at);
                        }
                }
                if(l_conn->m_save && l_conn->m_data1)
                {
                        if(l_conn->m_type == nconn::TYPE_SERVER)
                        {
                                http_req *l_req = static_cast<http_req *>(l_conn->m_data1);
                                l_req->m_p_url.m_ptr = a_at;
                                l_req->m_p_url.m_len = a_length;
                        }
                }

        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_status(http_parser* a_parser, const char *a_at, size_t a_length)
{
        nconn *l_conn = static_cast <nconn *>(a_parser->data);
        if(l_conn)
        {
                if(l_conn->m_verbose)
                {
                        if(l_conn->m_color)
                        {
                                NDBG_OUTPUT("%s: status: %s%.*s%s --STATUS = HTTP %d.%d METHOD[%d] %d--\n",
                                                l_conn->m_host.c_str(),
                                                ANSI_COLOR_FG_YELLOW, (int)a_length, a_at, ANSI_COLOR_OFF,
                                                a_parser->http_major,
                                                a_parser->http_minor,
                                                a_parser->method,
                                                a_parser->status_code);
                        }
                        else
                        {
                                NDBG_OUTPUT("%s: status: %.*s --STATUS = HTTP %d.%d METHOD[%d] %d--\n",
                                                l_conn->m_host.c_str(),
                                                (int)a_length, a_at,
                                                a_parser->http_major,
                                                a_parser->http_minor,
                                                a_parser->method,
                                                a_parser->status_code);
                        }
                }

                // Set status code
                l_conn->m_stat.m_status_code = a_parser->status_code;
                if(l_conn->m_data1)
                {
                        if(l_conn->m_type == nconn::TYPE_CLIENT)
                        {
                                http_resp *l_resp = static_cast<http_resp *>(l_conn->m_data1);
                                if(l_resp)
                                {
                                        if(l_conn->m_save)
                                        {
                                                l_resp->m_p_status.m_ptr = a_at;
                                                l_resp->m_p_status.m_len = a_length;
                                        }
                                        l_resp->set_status(a_parser->status_code);
                                }
                        }
                }
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_header_field(http_parser* a_parser, const char *a_at, size_t a_length)
{
        nconn *l_conn = static_cast <nconn *>(a_parser->data);
        if(l_conn)
        {
                if(l_conn->m_verbose)
                {
                        if(l_conn->m_color)
                        {
                                NDBG_OUTPUT("%s: field:  %s%.*s%s\n", l_conn->m_host.c_str(), ANSI_COLOR_FG_BLUE, (int)a_length, a_at, ANSI_COLOR_OFF);
                        }
                        else
                        {
                                NDBG_OUTPUT("%s: field:  %.*s\n", l_conn->m_host.c_str(), (int)a_length, a_at);
                        }
                }

                if(l_conn->m_save && l_conn->m_data1)
                {
                        cr_struct l_cr;
                        l_cr.m_ptr = a_at;
                        l_cr.m_len = a_length;
                        if(l_conn->m_type == nconn::TYPE_CLIENT)
                        {
                                http_resp *l_resp = static_cast<http_resp *>(l_conn->m_data1);
                                l_resp->m_p_h_list_key.push_back(l_cr);
                        }
                        else if(l_conn->m_type == nconn::TYPE_SERVER)
                        {
                                http_req *l_req = static_cast<http_req *>(l_conn->m_data1);
                                l_req->m_p_h_list_key.push_back(l_cr);
                        }
                }
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_header_value(http_parser* a_parser, const char *a_at, size_t a_length)
{
        nconn *l_conn = static_cast <nconn *>(a_parser->data);
        if(l_conn)
        {
                if(l_conn->m_verbose)
                {
                        if(l_conn->m_color)
                        {
                                NDBG_OUTPUT("%s: value:  %s%.*s%s\n", l_conn->m_host.c_str(), ANSI_COLOR_FG_GREEN, (int)a_length, a_at, ANSI_COLOR_OFF);
                        }
                        else
                        {
                                NDBG_OUTPUT("%s: value:  %.*s\n", l_conn->m_host.c_str(), (int)a_length, a_at);
                        }
                }

                if(l_conn->m_save && l_conn->m_data1)
                {
                        cr_struct l_cr;
                        l_cr.m_ptr = a_at;
                        l_cr.m_len = a_length;
                        if(l_conn->m_type == nconn::TYPE_CLIENT)
                        {
                                http_resp *l_resp = static_cast<http_resp *>(l_conn->m_data1);
                                l_resp->m_p_h_list_val.push_back(l_cr);
                        }
                        else if(l_conn->m_type == nconn::TYPE_SERVER)
                        {
                                http_req *l_req = static_cast<http_req *>(l_conn->m_data1);
                                l_req->m_p_h_list_val.push_back(l_cr);
                        }
                }

        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_headers_complete(http_parser* a_parser)
{
        nconn *l_conn = static_cast <nconn *>(a_parser->data);
        if(l_conn)
        {
                if(l_conn->m_verbose)
                {
                        NDBG_OUTPUT("%s: headers_complete\n", l_conn->m_host.c_str());
                        NDBG_OUTPUT("http_major: %d\n", a_parser->http_major);
                        NDBG_OUTPUT("http_minor: %d\n", a_parser->http_minor);
                        NDBG_OUTPUT("status_code: %d\n", a_parser->status_code);
                        NDBG_OUTPUT("method: %d\n", a_parser->method);
                        NDBG_OUTPUT("http_errno: %d\n", a_parser->http_errno);
                }

                if(l_conn->m_save && l_conn->m_data1)
                {
                        if(l_conn->m_type == nconn::TYPE_SERVER)
                        {
                                http_req *l_req = static_cast<http_req *>(l_conn->m_data1);
                                l_req->m_http_major = a_parser->http_major;
                                l_req->m_http_minor = a_parser->http_minor;
                                l_req->m_method = a_parser->method;
                        }
                }
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_body(http_parser* a_parser, const char *a_at, size_t a_length)
{
        nconn *l_conn = static_cast <nconn *>(a_parser->data);
        if(l_conn)
        {
                if(l_conn->m_verbose)
                {
                        if(l_conn->m_color)
                        {
                                NDBG_OUTPUT("%s: body:  %s%.*s%s\n", l_conn->m_host.c_str(), ANSI_COLOR_FG_YELLOW, (int)a_length, a_at, ANSI_COLOR_OFF);
                        }
                        else
                        {
                                NDBG_OUTPUT("%s: body:  %.*s\n", l_conn->m_host.c_str(), (int)a_length, a_at);
                        }
                }

                if(l_conn->m_save && l_conn->m_data1)
                {
                        if(l_conn->m_type == nconn::TYPE_CLIENT)
                        {
                                http_resp *l_resp = static_cast<http_resp *>(l_conn->m_data1);
                                if(!l_resp->m_p_body.m_ptr)
                                {
                                        l_resp->m_p_body.m_ptr = a_at;
                                        l_resp->m_p_body.m_len = a_length;
                                }
                                else
                                {
                                        l_resp->m_p_body.m_len += a_length;
                                }
                        }
                        else if(l_conn->m_type == nconn::TYPE_SERVER)
                        {
                                http_req *l_req = static_cast<http_req *>(l_conn->m_data1);
                                if(!l_req->m_p_body.m_ptr)
                                {
                                        l_req->m_p_body.m_ptr = a_at;
                                        l_req->m_p_body.m_len = a_length;
                                }
                                else
                                {
                                        l_req->m_p_body.m_len += a_length;
                                }
                        }
                }
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_message_complete(http_parser* a_parser)
{
        nconn *l_conn = static_cast <nconn *>(a_parser->data);
        if(l_conn)
        {
                if(l_conn->m_verbose)
                {
                        NDBG_OUTPUT("%s: message complete\n", l_conn->m_host.c_str());
                }

                //NDBG_PRINT("CONN[%u--%d] m_request_start_time_us: %" PRIu64 " m_tt_completion_us: %" PRIu64 "\n",
                //              l_conn->m_connection_id,
                //              l_conn->m_fd,
                //              l_conn->m_request_start_time_us,
                //              l_conn->m_stat.m_tt_completion_us);
                //if(!l_conn->m_stat.m_connect_start_time_us) abort();

                if(http_should_keep_alive(a_parser))
                {
                        l_conn->m_supports_keep_alives = true;
                }
                else
                {
                        l_conn->m_supports_keep_alives = false;
                }

                // we complete!!!
                if(l_conn->m_data1)
                {
                        if(l_conn->m_type == nconn::TYPE_CLIENT)
                        {
                                http_resp *l_resp = static_cast<http_resp *>(l_conn->m_data1);
                                l_resp->m_complete = true;
                        }
                        else if(l_conn->m_type == nconn::TYPE_SERVER)
                        {
                                http_req *l_req = static_cast<http_req *>(l_conn->m_data1);
                                l_req->m_complete = true;
                                l_req->m_supports_keep_alives = l_conn->m_supports_keep_alives;
                        }
                }
        }
        return 0;
}

} //namespace ns_hlx {
