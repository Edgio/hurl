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
#include "hlo/hlx_common.h"
#include "ndebug.h"
#include "nconn.h"
#include "http_parser.h"
#include "nbq.h"
#include <string.h>

//: ----------------------------------------------------------------------------
//: http-parser callbacks
//: ----------------------------------------------------------------------------

namespace ns_hlx {

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
                //if(l_conn->m_verbose)
                //{
                //        NDBG_OUTPUT("%s: message begin\n", l_conn->m_host.c_str());
                //}
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
                //if(l_conn->m_verbose)
                //{
                //        if(l_conn->m_color)
                //        {
                //                NDBG_OUTPUT("%s: url:    %s%.*s%s\n", l_conn->m_host.c_str(), ANSI_COLOR_FG_YELLOW, (int)a_length, a_at, ANSI_COLOR_OFF);
                //        }
                //        else
                //        {
                //                NDBG_OUTPUT("%s: url:    %.*s\n", l_conn->m_host.c_str(), (int)a_length, a_at);
                //        }
                //}
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
                //if(l_conn->m_verbose)
                //{
                //        if(l_conn->m_color)
                //        {
                //                NDBG_OUTPUT("%s: status: %s%.*s%s --STATUS = HTTP %d.%d METHOD[%d] %d--\n",
                //                                l_conn->m_host.c_str(),
                //                                ANSI_COLOR_FG_YELLOW, (int)a_length, a_at, ANSI_COLOR_OFF,
                //                                a_parser->http_major,
                //                                a_parser->http_minor,
                //                                a_parser->method,
                //                                a_parser->status_code);
                //        }
                //        else
                //        {
                //                NDBG_OUTPUT("%s: status: %.*s --STATUS = HTTP %d.%d METHOD[%d] %d--\n",
                //                                l_conn->m_host.c_str(),
                //                                (int)a_length, a_at,
                //                                a_parser->http_major,
                //                                a_parser->http_minor,
                //                                a_parser->method,
                //                                a_parser->status_code);
                //        }
                //}

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
                //if(l_conn->m_verbose)
                //{
                //        if(l_conn->m_color)
                //        {
                //                NDBG_OUTPUT("%s: field:  %s%.*s%s\n", l_conn->m_host.c_str(), ANSI_COLOR_FG_BLUE, (int)a_length, a_at, ANSI_COLOR_OFF);
                //        }
                //        else
                //        {
                //                NDBG_OUTPUT("%s: field:  %.*s\n", l_conn->m_host.c_str(), (int)a_length, a_at);
                //        }
                //}

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
                //if(l_conn->m_verbose)
                //{
                //        if(l_conn->m_color)
                //        {
                //                NDBG_OUTPUT("%s: value:  %s%.*s%s\n", l_conn->m_host.c_str(), ANSI_COLOR_FG_GREEN, (int)a_length, a_at, ANSI_COLOR_OFF);
                //        }
                //        else
                //        {
                //                NDBG_OUTPUT("%s: value:  %.*s\n", l_conn->m_host.c_str(), (int)a_length, a_at);
                //        }
                //}

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
                //if(l_conn->m_verbose)
                //{
                //        NDBG_OUTPUT("%s: headers_complete\n", l_conn->m_host.c_str());
                //        NDBG_OUTPUT("http_major: %d\n", a_parser->http_major);
                //        NDBG_OUTPUT("http_minor: %d\n", a_parser->http_minor);
                //        NDBG_OUTPUT("status_code: %d\n", a_parser->status_code);
                //        NDBG_OUTPUT("method: %d\n", a_parser->method);
                //        NDBG_OUTPUT("http_errno: %d\n", a_parser->http_errno);
                //}

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
                //if(l_conn->m_verbose)
                //{
                //        if(l_conn->m_color)
                //        {
                //                NDBG_OUTPUT("%s: body:  %s%.*s%s\n", l_conn->m_host.c_str(), ANSI_COLOR_FG_YELLOW, (int)a_length, a_at, ANSI_COLOR_OFF);
                //        }
                //        else
                //        {
                //                NDBG_OUTPUT("%s: body:  %.*s\n", l_conn->m_host.c_str(), (int)a_length, a_at);
                //        }
                //}

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
                //if(l_conn->m_verbose)
                //{
                //        NDBG_OUTPUT("%s: message complete\n", l_conn->m_host.c_str());
                //}

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
