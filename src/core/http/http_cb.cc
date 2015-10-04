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
#include "http_cb.h"
#include "hlo/hlx_common.h"
#include "ndebug.h"
#include "nconn.h"
#include <string.h>

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define CHECK_FOR_NULL_OK(_data) \
        do {\
                if(!_data) {\
                        return 0;\
                }\
        } while(0);

#define CHECK_FOR_NULL_ERROR(_data) \
        do {\
                if(!_data) {\
                        return STATUS_ERROR;\
                }\
        } while(0);

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_parse(void *a_data, char *a_buf, uint32_t a_len)
{
        CHECK_FOR_NULL_ERROR(a_data);
        http_data_t *l_data = static_cast<http_data_t *>(a_data);

        size_t l_parse_status = 0;
        //NDBG_PRINT("%sHTTP_PARSER%s: m_read_buf: %p, m_read_buf_idx: %d, l_bytes_read: %d\n",
        //                ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF,
        //                a_buf, (int)0, (int)a_len);
        l_parse_status = http_parser_execute(&(l_data->m_http_parser),
                                             &(l_data->m_http_parser_settings),
                                             a_buf,
                                             a_len);
        //m_read_buf_idx += l_bytes_read;
        if(l_parse_status < (size_t)a_len)
        {
                //if(m_verbose)
                //{
                //        NCONN_ERROR("HOST[%s]: Error: parse error.  Reason: %s: %s\n",
                //                        m_host.c_str(),
                //                        //"","");
                //                        http_errno_name((enum http_errno)m_http_parser.http_errno),
                //                        http_errno_description((enum http_errno)m_http_parser.http_errno));
                //        //NDBG_PRINT("%s: %sl_bytes_read%s[%d] <= 0 total = %u idx = %u\n",
                //        //              m_host.c_str(),
                //        //              ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_bytes_read, l_total_bytes_read, m_read_buf_idx);
                //        //ns_hlx::mem_display((const uint8_t *)m_read_buf + m_read_buf_idx, l_bytes_read);
                //
                //}
                return STATUS_ERROR;
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_message_begin(http_parser* a_parser)
{
        http_data_t *l_data = static_cast <http_data_t *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_data);
        if(l_data->m_verbose)
        {
                NDBG_OUTPUT(": message begin\n");
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
        http_data_t *l_data = static_cast <http_data_t *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_data);
        if(l_data->m_verbose)
        {
                if(l_data->m_color)
                {
                        NDBG_OUTPUT(": url:    %s%.*s%s\n", ANSI_COLOR_FG_YELLOW, (int)a_length, a_at, ANSI_COLOR_OFF);
                }
                else
                {
                        NDBG_OUTPUT(": url:    %.*s\n", (int)a_length, a_at);
                }
        }
        if(l_data->m_save && l_data->m_type == HTTP_DATA_TYPE_SERVER)
        {
                l_data->m_http_req.m_p_url.m_ptr = a_at;
                l_data->m_http_req.m_p_url.m_len = a_length;
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
        http_data_t *l_data = static_cast <http_data_t *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_data);
        if(l_data->m_verbose)
        {
                if(l_data->m_color)
                {
                        NDBG_OUTPUT(": status: %s%.*s%s --STATUS = HTTP %d.%d METHOD[%d] %d--\n",
                                        ANSI_COLOR_FG_YELLOW, (int)a_length, a_at, ANSI_COLOR_OFF,
                                        a_parser->http_major,
                                        a_parser->http_minor,
                                        a_parser->method,
                                        a_parser->status_code);
                }
                else
                {
                        NDBG_OUTPUT(": status: %.*s --STATUS = HTTP %d.%d METHOD[%d] %d--\n",
                                        (int)a_length, a_at,
                                        a_parser->http_major,
                                        a_parser->http_minor,
                                        a_parser->method,
                                        a_parser->status_code);
                }
        }
        l_data->m_status_code = a_parser->status_code;
        if(l_data->m_type == HTTP_DATA_TYPE_CLIENT)
        {
                l_data->m_http_resp.set_status(a_parser->status_code);
                if(l_data->m_save)
                {
                        l_data->m_http_resp.m_p_status.m_ptr = a_at;
                        l_data->m_http_resp.m_p_status.m_len = a_length;
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
        http_data_t *l_data = static_cast <http_data_t *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_data);
        if(l_data->m_verbose)
        {
                if(l_data->m_color)
                {
                        NDBG_OUTPUT(": field:  %s%.*s%s\n", ANSI_COLOR_FG_BLUE, (int)a_length, a_at, ANSI_COLOR_OFF);
                }
                else
                {
                        NDBG_OUTPUT(": field:  %.*s\n", (int)a_length, a_at);
                }
        }
        if(l_data->m_save)
        {
                cr_struct l_cr;
                l_cr.m_ptr = a_at;
                l_cr.m_len = a_length;
                if(l_data->m_type == HTTP_DATA_TYPE_CLIENT)
                {
                        l_data->m_http_resp.m_p_h_list_key.push_back(l_cr);
                }
                else if(l_data->m_type == HTTP_DATA_TYPE_SERVER)
                {
                        l_data->m_http_req.m_p_h_list_key.push_back(l_cr);
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
        http_data_t *l_data = static_cast <http_data_t *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_data);
        if(l_data->m_verbose)
        {
                if(l_data->m_color)
                {
                        NDBG_OUTPUT(": value:  %s%.*s%s\n", ANSI_COLOR_FG_GREEN, (int)a_length, a_at, ANSI_COLOR_OFF);
                }
                else
                {
                        NDBG_OUTPUT(": value:  %.*s\n", (int)a_length, a_at);
                }
        }
        if(l_data->m_save)
        {
                cr_struct l_cr;
                l_cr.m_ptr = a_at;
                l_cr.m_len = a_length;
                if(l_data->m_type == HTTP_DATA_TYPE_CLIENT)
                {
                        l_data->m_http_resp.m_p_h_list_val.push_back(l_cr);
                }
                else if(l_data->m_type == HTTP_DATA_TYPE_SERVER)
                {
                        l_data->m_http_req.m_p_h_list_val.push_back(l_cr);
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
        http_data_t *l_data = static_cast <http_data_t *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_data);
        if(l_data->m_verbose)
        {
                NDBG_OUTPUT(": headers_complete\n");
                NDBG_OUTPUT("http_major: %d\n", a_parser->http_major);
                NDBG_OUTPUT("http_minor: %d\n", a_parser->http_minor);
                NDBG_OUTPUT("status_code: %d\n", a_parser->status_code);
                NDBG_OUTPUT("method: %d\n", a_parser->method);
                NDBG_OUTPUT("http_errno: %d\n", a_parser->http_errno);
        }
        if(l_data->m_save && l_data->m_type == HTTP_DATA_TYPE_SERVER)
        {
                l_data->m_http_req.m_http_major = a_parser->http_major;
                l_data->m_http_req.m_http_minor = a_parser->http_minor;
                l_data->m_http_req.m_method = a_parser->method;
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
        http_data_t *l_data = static_cast <http_data_t *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_data);
        if(l_data->m_verbose)
        {
                if(l_data->m_color)
                {
                        NDBG_OUTPUT(": body:  %s%.*s%s\n", ANSI_COLOR_FG_YELLOW, (int)a_length, a_at, ANSI_COLOR_OFF);
                }
                else
                {
                        NDBG_OUTPUT(": body:  %.*s\n", (int)a_length, a_at);
                }
        }
        if(l_data->m_save)
        {
                if(l_data->m_type == HTTP_DATA_TYPE_CLIENT)
                {
                        if(!l_data->m_http_resp.m_p_body.m_ptr)
                        {
                                l_data->m_http_resp.m_p_body.m_ptr = a_at;
                                l_data->m_http_resp.m_p_body.m_len = a_length;
                        }
                        else
                        {
                                l_data->m_http_resp.m_p_body.m_len += a_length;
                        }
                }
                else if(l_data->m_type == HTTP_DATA_TYPE_SERVER)
                {
                        if(!l_data->m_http_req.m_p_body.m_ptr)
                        {
                                l_data->m_http_req.m_p_body.m_ptr = a_at;
                                l_data->m_http_req.m_p_body.m_len = a_length;
                        }
                        else
                        {
                                l_data->m_http_req.m_p_body.m_len += a_length;
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
        http_data_t *l_data = static_cast <http_data_t *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_data);
        if(l_data->m_verbose)
        {
                NDBG_OUTPUT(": message complete\n");
        }
        //NDBG_PRINT("CONN[%u--%d] m_request_start_time_us: %" PRIu64 " m_tt_completion_us: %" PRIu64 "\n",
        //              l_data->m_connection_id,
        //              l_data->m_fd,
        //              l_data->m_request_start_time_us,
        //              l_data->m_stat.m_tt_completion_us);
        //if(!l_data->m_stat.m_connect_start_time_us) abort();
        if(http_should_keep_alive(a_parser))
        {
                l_data->m_supports_keep_alives = true;
        }
        else
        {
                l_data->m_supports_keep_alives = false;
        }
        if(l_data->m_type == HTTP_DATA_TYPE_CLIENT)
        {
                l_data->m_http_resp.m_complete = true;
        }
        else if(l_data->m_type == HTTP_DATA_TYPE_SERVER)
        {
                l_data->m_http_req.m_complete = true;
                l_data->m_http_req.m_supports_keep_alives = l_data->m_supports_keep_alives;
        }
        return 0;
}

} //namespace ns_hlx {
