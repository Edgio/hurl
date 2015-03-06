//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    02/07/2014
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
#include "ndebug.h"
#include "http_cb.h"
#include "nconn.h"
#include "req_stat.h"
#include "reqlet.h"
#include "util.h"

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
                                NDBG_OUTPUT("%s: url:   %s%.*s%s\n", l_conn->m_host.c_str(), ANSI_COLOR_FG_YELLOW, (int)a_length, a_at, ANSI_COLOR_OFF);
                        else
                                NDBG_OUTPUT("%s: url:   %.*s\n", l_conn->m_host.c_str(), (int)a_length, a_at);
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
                                NDBG_OUTPUT("%s: status: %s%.*s%s --STATUS = HTTP %d.%d METHOD[%d] %d--\n",
                                                l_conn->m_host.c_str(),
                                                ANSI_COLOR_FG_YELLOW, (int)a_length, a_at, ANSI_COLOR_OFF,
                                                a_parser->http_major,
                                                a_parser->http_minor,
                                                a_parser->method,
                                                a_parser->status_code);
                        else
                                NDBG_OUTPUT("%s: status: %.*s --STATUS = HTTP %d.%d METHOD[%d] %d--\n",
                                                l_conn->m_host.c_str(),
                                                (int)a_length, a_at,
                                                a_parser->http_major,
                                                a_parser->http_minor,
                                                a_parser->method,
                                                a_parser->status_code);
                }

                // Set status code
                l_conn->m_stat.m_status_code = a_parser->status_code;

                if(l_conn->m_save_response_in_reqlet)
                {
                        // Get reqlet
                        reqlet *l_reqlet = static_cast<reqlet *>(l_conn->m_data1);
                        if(l_reqlet)
                        {
                                std::string l_status;
                                l_status.append(a_at, a_length);
                                l_reqlet->m_response_headers["Status"] = l_status;
                                l_reqlet->m_response_status = a_parser->status_code;
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
                                NDBG_OUTPUT("%s: field:  %s%.*s%s\n", l_conn->m_host.c_str(), ANSI_COLOR_FG_BLUE, (int)a_length, a_at, ANSI_COLOR_OFF);
                        else
                                NDBG_OUTPUT("%s: field:  %.*s\n", l_conn->m_host.c_str(), (int)a_length, a_at);
                }

                if(l_conn->m_save_response_in_reqlet)
                {
                        // Get reqlet
                        reqlet *l_reqlet = static_cast<reqlet *>(l_conn->m_data1);
                        if(l_reqlet)
                        {
                                std::string l_header;
                                l_header.append(a_at, a_length);
                                l_reqlet->m_response_headers[l_header] = "";
                                l_reqlet->m_next_response_value = l_reqlet->m_response_headers.find(l_header);
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
                                NDBG_OUTPUT("%s: value:  %s%.*s%s\n", l_conn->m_host.c_str(), ANSI_COLOR_FG_GREEN, (int)a_length, a_at, ANSI_COLOR_OFF);
                        else
                                NDBG_OUTPUT("%s: value:  %.*s\n", l_conn->m_host.c_str(), (int)a_length, a_at);
                }

                if(l_conn->m_save_response_in_reqlet)
                {
                        // Get reqlet
                        reqlet *l_reqlet = static_cast<reqlet *>(l_conn->m_data1);
                        if(l_reqlet)
                        {
                                (l_reqlet->m_next_response_value)->second.append(a_at, a_length);
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
                }

                // Stats
                if(l_conn->m_collect_stats_flag)
                {
                        l_conn->m_stat.m_tt_header_completion_us = get_delta_time_us(l_conn->m_request_start_time_us);
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
                                NDBG_OUTPUT("%s: body:  %s%.*s%s\n", l_conn->m_host.c_str(), ANSI_COLOR_FG_YELLOW, (int)a_length, a_at, ANSI_COLOR_OFF);
                        else
                                NDBG_OUTPUT("%s: body:  %.*s\n", l_conn->m_host.c_str(), (int)a_length, a_at);
                }

                // Stats
                if(l_conn->m_collect_stats_flag)
                {
                        l_conn->m_stat.m_body_bytes += a_length;
                }

                if(l_conn->m_save_response_in_reqlet)
                {
                        // Get reqlet
                        reqlet *l_reqlet = static_cast<reqlet *>(l_conn->m_data1);
                        if(l_reqlet)
                        {
                                l_reqlet->m_response_body.append(a_at, a_length);
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

                // Stats
                // Stats
                if(l_conn->m_collect_stats_flag)
                {
                        l_conn->m_stat.m_tt_completion_us = get_delta_time_us(l_conn->m_request_start_time_us);
                }

                //NDBG_PRINT("CONN[%u--%d] m_request_start_time_us: %" PRIu64 " m_tt_completion_us: %" PRIu64 "\n",
                //              l_conn->m_connection_id,
                //              l_conn->m_fd,
                //              l_conn->m_request_start_time_us,
                //              l_conn->m_stat.m_tt_completion_us);
                //if(!l_conn->m_stat.m_connect_start_time_us) abort();

                if(http_should_keep_alive(a_parser))
                {
                        l_conn->m_server_response_supports_keep_alives = true;
                }
                else
                {
                        l_conn->m_server_response_supports_keep_alives = false;
                }

                // we outtie
                l_conn->set_state_done();

        }


        return 0;
}
