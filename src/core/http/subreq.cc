//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    t_client.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    03/11/2015
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
#include "http_parser/http_parser.h"
#include "resolver.h"
#include "string_util.h"

#include <string.h>
#include <string>

#include <stdint.h>

#include "hlx/hlx.h"

// json support
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: Constructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subreq::subreq(std::string a_id):
        m_id(a_id),
        m_type(SUBREQ_TYPE_NONE),
        m_host(),
        m_path(),
        m_query(),
        m_fragment(),
        m_userinfo(),
        m_hostname(),
        m_port(),
        m_verb("GET"),
        m_timeout_s(10),
        m_keepalive(false),
        m_connect_only(false),
        m_error_cb(NULL),
        m_completion_cb(NULL),
        m_child_completion_cb(NULL),
        m_create_req_cb(create_request),
        m_where(),
        m_scheme(),
        m_host_info(),
        m_headers(),
        m_save_response(true),
        m_body_data(NULL),
        m_body_data_len(0),
        m_multipath(false),
        m_summary_info(),
        m_parent(NULL),
        m_parent_mutex(),
        m_child_list(),
        m_is_resolved_flag(),
        m_resp(NULL),
        m_req_nconn(NULL),
        m_req_resp(NULL),
        m_num_reqs_per_conn(1),
        m_num_to_request(1),
        m_num_requested(0),
        m_num_completed(0),
        m_host_list()
{
}

//: ----------------------------------------------------------------------------
//: \details: Copy constructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subreq::subreq(const subreq &a_subreq):
        m_id(a_subreq.m_id),
        m_type(a_subreq.m_type),
        m_host(a_subreq.m_host),
        m_path(a_subreq.m_path),
        m_query(a_subreq.m_query),
        m_fragment(a_subreq.m_fragment),
        m_userinfo(a_subreq.m_userinfo),
        m_hostname(a_subreq.m_hostname),
        m_port(a_subreq.m_port),
        m_verb(a_subreq.m_verb),
        m_timeout_s(a_subreq.m_timeout_s),
        m_keepalive(a_subreq.m_keepalive),
        m_connect_only(a_subreq.m_connect_only),
        m_error_cb(a_subreq.m_error_cb),
        m_completion_cb(a_subreq.m_completion_cb),
        m_child_completion_cb(a_subreq.m_child_completion_cb),
        m_create_req_cb(a_subreq.m_create_req_cb),
        m_where(a_subreq.m_where),
        m_scheme(a_subreq.m_scheme),
        m_host_info(a_subreq.m_host_info),
        m_headers(a_subreq.m_headers),
        m_save_response(a_subreq.m_save_response),
        m_body_data(NULL),
        m_body_data_len(0),
        m_multipath(a_subreq.m_multipath),
        m_summary_info(a_subreq.m_summary_info),
        m_parent(a_subreq.m_parent),
        m_parent_mutex(a_subreq.m_parent_mutex),
        m_child_list(),
        m_is_resolved_flag(a_subreq.m_is_resolved_flag),
        m_resp(a_subreq.m_resp),
        m_req_nconn(a_subreq.m_req_nconn),
        m_req_resp(a_subreq.m_req_resp),
        m_num_reqs_per_conn(a_subreq.m_num_reqs_per_conn),
        m_num_to_request(a_subreq.m_num_to_request),
        m_num_requested(a_subreq.m_num_requested),
        m_num_completed(a_subreq.m_num_completed),
        m_host_list()
{
        if(a_subreq.m_body_data && (a_subreq.m_body_data_len > 0))
        {
                if(m_body_data)
                {
                        free(m_body_data);
                        m_body_data = NULL;
                        m_body_data_len = 0;
                }
                m_body_data_len = a_subreq.m_body_data_len;
                m_body_data = (char *)malloc(sizeof(char)*m_body_data_len);
                memcpy(m_body_data, a_subreq.m_body_data, m_body_data_len);
        }
}

//: ----------------------------------------------------------------------------
//: \details: Destructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subreq::~subreq()
{
        if(!m_child_list.empty())
        {
                for(subreq_list_t::iterator i_s = m_child_list.begin();
                                i_s != m_child_list.end();
                                ++i_s)
                {
                        if(*i_s)
                        {
                                delete (*i_s);
                                *i_s = NULL;
                        }
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subreq::reset(void)
{
        m_num_requested = 0;
        m_num_completed = 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subreq::set_host(const std::string &a_host)
{
        m_host = a_host;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subreq::set_response(uint16_t a_response_status, const char *a_response)
{
        // Set response
        std::string l_response(a_response);
        if(m_resp)
        {
                if(!l_response.empty())
                {
                        m_resp->set_body("\"" + l_response + "\"");
                }
                m_resp->set_status(a_response_status);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int subreq::set_header(const std::string &a_header)
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
        set_header(l_header_key, l_header_val);
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int subreq::set_header(const std::string &a_key, const std::string &a_val)
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
void subreq::clear_headers(void)
{
        m_headers.clear();
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int subreq::set_host_list(const host_list_t &a_host_list)
{
        m_host_list.clear();
        m_host_list = a_host_list;
        m_num_to_request = m_host_list.size();
        m_type = SUBREQ_TYPE_FANOUT;
        return HLX_SERVER_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int subreq::set_server_list(const server_list_t &a_server_list)
{
        m_host_list.clear();
        // Create the reqlet list
        for(server_list_t::const_iterator i_server = a_server_list.begin();
            i_server != a_server_list.end();
            ++i_server)
        {
                // Create host_t
                host_t l_host;
                l_host.m_host = (*i_server);
                l_host.m_hostname = (*i_server);
                l_host.m_id = "";
                l_host.m_where = "";
                m_host_list.push_back(l_host);
        }
        m_type = SUBREQ_TYPE_FANOUT;
        m_num_to_request = m_host_list.size();
        return HLX_SERVER_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subreq::set_keepalive(bool a_val)
{
        m_keepalive = a_val;
        if(m_keepalive)
        {
                set_header("Connection", "keep-alive");
                if(m_num_reqs_per_conn == 1)
                {
                        set_num_reqs_per_conn(-1);
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool subreq::get_keepalive(void)
{
        return m_keepalive;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t subreq::resolve(resolver *a_resolver)
{
        if(!a_resolver)
        {
                return STATUS_ERROR;
        }
        if(m_is_resolved_flag)
        {
                return STATUS_OK;
        }

        int32_t l_status = STATUS_OK;
        std::string l_error;

        // TODO REMOVE!!!
        //NDBG_PRINT("m_url.m_host: %s\n", m_url.m_host.c_str());
        //NDBG_PRINT("m_url.m_port: %d\n", m_url.m_port);

        l_status = a_resolver->cached_resolve(m_host, m_port, m_host_info, l_error);
        if(l_status != STATUS_OK)
        {
                //set_response(900, "Address resolution failed.");
                m_is_resolved_flag = false;
                return STATUS_ERROR;
        }

        //show_host_info();

        m_is_resolved_flag = true;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t subreq::init_with_url(const std::string &a_url)
{

        std::string l_url_fixed = a_url;
        // Find scheme prefix "://"
        if(a_url.find("://", 0) == std::string::npos)
        {
                l_url_fixed = "http://" + a_url;
        }

        //NDBG_PRINT("Parse url:           %s\n", a_url.c_str());
        //NDBG_PRINT("Parse a_wildcarding: %d\n", a_wildcarding);
        http_parser_url l_url;
        int l_status;
        l_status = http_parser_parse_url(l_url_fixed.c_str(), l_url_fixed.length(), 0, &l_url);
        if(l_status != 0)
        {
                NDBG_PRINT("Error parsing url: %s\n", l_url_fixed.c_str());
                // TODO get error msg from http_parser
                return STATUS_ERROR;
        }

        // Set no port
        m_port = 0;

        for(uint32_t i_part = 0; i_part < UF_MAX; ++i_part)
        {
                //NDBG_PRINT("i_part: %d offset: %d len: %d\n", i_part, l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                //NDBG_PRINT("len+off: %d\n",       l_url.field_data[i_part].len + l_url.field_data[i_part].off);
                //NDBG_PRINT("a_url.length(): %d\n", (int)a_url.length());
                if(l_url.field_data[i_part].len &&
                  // TODO Some bug with parser -parsing urls like "http://127.0.0.1" sans paths
                  ((l_url.field_data[i_part].len + l_url.field_data[i_part].off) <= l_url_fixed.length()))
                {
                        switch(i_part)
                        {
                        case UF_SCHEMA:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part: %s\n", l_part.c_str());
                                if(l_part == "http")
                                {
                                        m_scheme = SCHEME_TCP;
                                }
                                else if(l_part == "https")
                                {
                                        m_scheme = SCHEME_SSL;
                                }
                                else
                                {
                                        NDBG_PRINT("Error schema[%s] is unsupported\n", l_part.c_str());
                                        return STATUS_ERROR;
                                }
                                break;
                        }
                        case UF_HOST:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_HOST]: %s\n", l_part.c_str());
                                m_host = l_part;
                                break;
                        }
                        case UF_PORT:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_PORT]: %s\n", l_part.c_str());
                                m_port = (uint16_t)strtoul(l_part.c_str(), NULL, 10);
                                break;
                        }
                        case UF_PATH:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_PATH]: %s\n", l_part.c_str());
                                m_path = l_part;
                                break;
                        }
                        case UF_QUERY:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_QUERY]: %s\n", l_part.c_str());
                                m_query = l_part;
                                break;
                        }
                        case UF_FRAGMENT:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_FRAGMENT]: %s\n", l_part.c_str());
                                m_fragment = l_part;
                                break;
                        }
                        case UF_USERINFO:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //sNDBG_PRINT("l_part[UF_USERINFO]: %s\n", l_part.c_str());
                                m_userinfo = l_part;
                                break;
                        }
                        default:
                        {
                                break;
                        }
                        }
                }
        }

        // Default ports
        if(!m_port)
        {
                switch(m_scheme)
                {
                case SCHEME_TCP:
                {
                        m_port = 80;
                        break;
                }
                case SCHEME_SSL:
                {
                        m_port = 443;
                        break;
                }
                default:
                {
                        m_port = 80;
                        break;
                }
                }
        }
        //m_num_to_req = m_path_vector.size();
        //NDBG_PRINT("Showing parsed url.\n");
        //m_url.show();
        if (STATUS_OK != l_status)
        {
                // Failure
                NDBG_PRINT("Error parsing url: %s.\n", l_url_fixed.c_str());
                return STATUS_ERROR;
        }
        //NDBG_PRINT("Parsed url: %s\n", l_url_fixed.c_str());
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t subreq::create_request(subreq &a_subreq, http_req &a_req)
{
        std::string l_path_ref = a_subreq.m_path;

        char l_buf[2048];
        int32_t l_len = 0;
        if(l_path_ref.empty())
        {
                l_path_ref = "/";
        }
        if(!(a_subreq.m_query.empty()))
        {
                l_path_ref += "?";
                l_path_ref += a_subreq.m_query;
        }
        //NDBG_PRINT("HOST: %s PATH: %s\n", a_subreq.m_host.c_str(), l_path_ref.c_str());
        l_len = snprintf(l_buf, sizeof(l_buf),
                        "%s %.500s HTTP/1.1", a_subreq.m_verb.c_str(), l_path_ref.c_str());

        a_req.write_request_line(l_buf, l_len);

        // -------------------------------------------
        // Add repo headers
        // -------------------------------------------
        bool l_specd_host = false;

        // Loop over reqlet map
        for(kv_map_list_t::const_iterator i_hl = a_subreq.m_headers.begin();
            i_hl != a_subreq.m_headers.end();
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
                        a_req.write_header(i_hl->first.c_str(), i_v->c_str());
                        if (strcasecmp(i_hl->first.c_str(), "host") == 0)
                        {
                                l_specd_host = true;
                        }
                }
        }

        // -------------------------------------------
        // Default Host if unspecified
        // -------------------------------------------
        if (!l_specd_host)
        {
                if(!a_subreq.m_hostname.empty())
                {
                        a_req.write_header("Host", a_subreq.m_hostname.c_str());
                }
                else
                {
                        a_req.write_header("Host", a_subreq.m_host.c_str());
                }
        }

        // -------------------------------------------
        // body
        // -------------------------------------------
        if(a_subreq.m_body_data && a_subreq.m_body_data_len)
        {
                //NDBG_PRINT("Write: buf: %p len: %d\n", l_buf, l_len);
                a_req.write_body(a_subreq.m_body_data, a_subreq.m_body_data_len);
        }
        else
        {
                a_req.write_body(NULL, 0);
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define ARESP(_str) l_responses_str += _str
std::string subreq::dump_all_responses(bool a_color, bool a_pretty, output_type_t a_output_type, int a_part_map)
{
        std::string l_responses_str = "";
        switch(a_output_type)
        {
        case OUTPUT_LINE_DELIMITED:
        {
                l_responses_str = dump_all_responses_line_dl(a_color, a_pretty, a_part_map);
                break;
        }
        case OUTPUT_JSON:
        {
                l_responses_str = dump_all_responses_json(a_part_map);
                break;
        }
        default:
        {
                break;
        }
        }

        return l_responses_str;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string subreq::dump_all_responses_line_dl(bool a_color,
                                               bool a_pretty,
                                               int a_part_map)
{

        std::string l_responses_str = "";
        std::string l_host_color = "";
        std::string l_server_color = "";
        std::string l_id_color = "";
        std::string l_status_color = "";
        std::string l_header_color = "";
        std::string l_body_color = "";
        std::string l_off_color = "";
        char l_buf[1024*1024];
        if(a_color)
        {
                l_host_color = ANSI_COLOR_FG_BLUE;
                l_server_color = ANSI_COLOR_FG_RED;
                l_id_color = ANSI_COLOR_FG_CYAN;
                l_status_color = ANSI_COLOR_FG_MAGENTA;
                l_header_color = ANSI_COLOR_FG_GREEN;
                l_body_color = ANSI_COLOR_FG_YELLOW;
                l_off_color = ANSI_COLOR_OFF;
        }

        int l_cur_reqlet = 0;
        for(subreq_list_t::const_iterator i_rx = m_child_list.begin();
            i_rx != m_child_list.end();
            ++i_rx, ++l_cur_reqlet)
        {

                http_resp *l_resp = (*i_rx)->get_resp();
                if(!l_resp)
                {
                        continue;
                }

                bool l_fbf = false;
                // Host
                if(a_part_map & PART_HOST)
                {
                        sprintf(l_buf, "\"%shost%s\": \"%s\"",
                                        l_host_color.c_str(), l_off_color.c_str(),
                                        (*i_rx)->m_host.c_str());
                        ARESP(l_buf);
                        l_fbf = true;
                }

                // Server
                if(a_part_map & PART_SERVER)
                {

                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        sprintf(l_buf, "\"%sserver%s\": \"%s:%d\"",
                                        l_server_color.c_str(), l_server_color.c_str(),
                                        (*i_rx)->m_host.c_str(),
                                        (*i_rx)->m_port
                                        );
                        ARESP(l_buf);
                        l_fbf = true;

                        if(!(*i_rx)->m_id.empty())
                        {
                                if(l_fbf) {ARESP(", "); l_fbf = false;}
                                sprintf(l_buf, "\"%sid%s\": \"%s\"",
                                                l_id_color.c_str(), l_id_color.c_str(),
                                                (*i_rx)->m_id.c_str()
                                                );
                                ARESP(l_buf);
                                l_fbf = true;
                        }

                        if(!(*i_rx)->m_where.empty())
                        {
                                if(l_fbf) {ARESP(", "); l_fbf = false;}
                                sprintf(l_buf, "\"%swhere%s\": \"%s\"",
                                                l_id_color.c_str(), l_id_color.c_str(),
                                                (*i_rx)->m_where.c_str()
                                                );
                                ARESP(l_buf);
                                l_fbf = true;
                        }


                        l_fbf = true;
                }

                // Status Code
                if(a_part_map & PART_STATUS_CODE)
                {
                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        const char *l_status_val_color = "";
                        if(a_color)
                        {
                                if(l_resp->get_status() == 200) l_status_val_color = ANSI_COLOR_FG_GREEN;
                                else l_status_val_color = ANSI_COLOR_FG_RED;
                        }
                        sprintf(l_buf, "\"%sstatus-code%s\": %s%d%s",
                                        l_status_color.c_str(), l_off_color.c_str(),
                                        l_status_val_color, l_resp->get_status(), l_off_color.c_str());
                        ARESP(l_buf);
                        l_fbf = true;
                }

                // Headers
                // TODO -only in json mode for now
                if(a_part_map & PART_HEADERS)
                {
                        // nuthin
                }

                // Body
                if(a_part_map & PART_BODY)
                {
                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        //NDBG_PRINT("RESPONSE SIZE: %ld\n", (*i_rx)->m_response_body.length());
                        char *l_body_buf = NULL;
                        uint32_t l_body_len;
                        l_resp->get_body(&l_body_buf, l_body_len);
                        if(l_body_len)
                        {
                                sprintf(l_buf, "\"%sbody%s\": %s",
                                                l_body_color.c_str(), l_off_color.c_str(),
                                                l_body_buf);
                        }
                        else
                        {
                                sprintf(l_buf, "\"%sbody%s\": \"NO_RESPONSE\"",
                                                l_body_color.c_str(), l_off_color.c_str());
                        }
                        if(l_body_buf)
                        {
                                free(l_body_buf);
                                l_body_buf = NULL;
                        }
                        ARESP(l_buf);
                        l_fbf = true;
                }
                ARESP("\n");
        }

        return l_responses_str;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define JS_ADD_MEMBER(_key, _val)\
l_obj.AddMember(_key,\
                rapidjson::Value(_val, l_js_allocator).Move(),\
                l_js_allocator)

std::string subreq::dump_all_responses_json(int a_part_map)
{
        rapidjson::Document l_js_doc;
        l_js_doc.SetObject();
        rapidjson::Value l_js_array(rapidjson::kArrayType);
        rapidjson::Document::AllocatorType& l_js_allocator = l_js_doc.GetAllocator();

        for(subreq_list_t::const_iterator i_rx = m_child_list.begin();
            i_rx != m_child_list.end();
            ++i_rx)
        {

                http_resp *l_resp = (*i_rx)->get_resp();
                if(!l_resp)
                {
                        continue;
                }
                const kv_map_list_t &l_headers = l_resp->get_headers();
                rapidjson::Value l_obj;
                l_obj.SetObject();
                bool l_content_type_json = false;

                // Search for json
                kv_map_list_t::const_iterator i_h = l_headers.find("Content-type");
                if(i_h != l_headers.end())
                {
                        for(str_list_t::const_iterator i_val = i_h->second.begin();
                            i_val != i_h->second.end();
                            ++i_val)
                        {
                                if((*i_val) == "application/json")
                                {
                                        l_content_type_json = true;
                                }
                        }
                }

                // Host
                if(a_part_map & PART_HOST)
                {
                        JS_ADD_MEMBER("host", (*i_rx)->m_host.c_str());
                }

                // Server
                if(a_part_map & PART_SERVER)
                {
                        char l_server_buf[1024];
                        sprintf(l_server_buf, "%s:%d",
                                        (*i_rx)->m_host.c_str(),
                                        (*i_rx)->m_port);
                        JS_ADD_MEMBER("server", l_server_buf);

                        if(!(*i_rx)->m_id.empty())
                        {
                                JS_ADD_MEMBER("id", (*i_rx)->m_id.c_str());
                        }

                        if(!(*i_rx)->m_where.empty())
                        {
                                JS_ADD_MEMBER("where", (*i_rx)->m_where.c_str());
                        }
                        if((*i_rx)->m_port != 0)
                        {
                                l_obj.AddMember("port", (*i_rx)->m_port, l_js_allocator);
                        }
                }

                // Status Code
                if(a_part_map & PART_STATUS_CODE)
                {
                        l_obj.AddMember("status-code", l_resp->get_status(), l_js_allocator);
                }

                // Headers
                if(a_part_map & PART_HEADERS)
                {
                        for(kv_map_list_t::const_iterator i_header = l_headers.begin();
                            i_header != l_headers.end();
                            ++i_header)
                        {

                                for(str_list_t::const_iterator i_val = i_header->second.begin();
                                    i_val != i_header->second.end();
                                    ++i_val)
                                {
                                        l_obj.AddMember(rapidjson::Value(i_header->first.c_str(), l_js_allocator).Move(),
                                                        rapidjson::Value(i_val->c_str(), l_js_allocator).Move(),
                                                        l_js_allocator);
                                }
                        }
                }

                // ---------------------------------------------------
                // SSL connection info
                // ---------------------------------------------------
                // TODO Add flag -only add to output if flag
                // ---------------------------------------------------
                if(l_resp->m_ssl_info_cipher_str)
                {
                        l_obj.AddMember(rapidjson::Value("Cipher", l_js_allocator).Move(),
                                        rapidjson::Value(l_resp->m_ssl_info_cipher_str, l_js_allocator).Move(),
                                        l_js_allocator);
                }
                if(l_resp->m_ssl_info_cipher_str)
                {
                        l_obj.AddMember(rapidjson::Value("Protocol", l_js_allocator).Move(),
                                        rapidjson::Value(l_resp->m_ssl_info_protocol_str, l_js_allocator).Move(),
                                        l_js_allocator);
                }

                // Body
                if(a_part_map & PART_BODY)
                {

                        //NDBG_PRINT("RESPONSE SIZE: %ld\n", (*i_rx)->m_response_body.length());
                        char *l_body_buf = NULL;
                        uint32_t l_body_len = 0;
                        l_resp->get_body(&l_body_buf, l_body_len);
                        if(l_body_len)
                        {
                                // Append json
                                if(l_content_type_json)
                                {
                                        rapidjson::Document l_doc_body;
                                        l_doc_body.Parse(l_body_buf);
                                        l_obj.AddMember("body",
                                                        rapidjson::Value(l_doc_body, l_js_allocator).Move(),
                                                        l_js_allocator);

                                }
                                else
                                {
                                        JS_ADD_MEMBER("body", l_body_buf);
                                }
                        }
                        else
                        {
                                JS_ADD_MEMBER("body", "NO_RESPONSE");
                        }
                        if(l_body_buf)
                        {
                                free(l_body_buf);
                                l_body_buf = NULL;
                        }
                }

                l_js_array.PushBack(l_obj, l_js_allocator);

        }

        // TODO -Can I just create an array -do I have to stick in a document?
        l_js_doc.AddMember("array", l_js_array, l_js_allocator);
        rapidjson::StringBuffer l_strbuf;
        rapidjson::Writer<rapidjson::StringBuffer> l_js_writer(l_strbuf);
        l_js_doc["array"].Accept(l_js_writer);

        //NDBG_PRINT("Document: \n%s\n", l_strbuf.GetString());
        std::string l_responses_str = l_strbuf.GetString();

        return l_responses_str;
}

#if 0
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subreq::set_uri(const std::string &a_uri)
{
        m_uri = a_uri;

        // -------------------------------------------------
        // TODO Zero copy with something like substring...
        // This is pretty awful performance wise
        // -------------------------------------------------
        // Read uri up to first '?'
        size_t l_query_pos = 0;
        if((l_query_pos = m_uri.find('?', 0)) == std::string::npos)
        {
                // No query string -path == uri
                m_path = m_uri;
                return;
        }

        m_path = m_uri.substr(0, l_query_pos);

        // TODO Url decode???

        std::string l_query = m_uri.substr(l_query_pos + 1, m_uri.length() - l_query_pos + 1);

        // Split the query by '&'
        if(!l_query.empty())
        {

                //NDBG_PRINT("%s__QUERY__%s: l_query: %s\n", ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF, l_query.c_str());

                size_t l_qi_begin = 0;
                size_t l_qi_end = 0;
                bool l_last = false;
                while (!l_last)
                {
                        l_qi_end = l_query.find('&', l_qi_begin);
                        if(l_qi_end == std::string::npos)
                        {
                                l_last = true;
                                l_qi_end = l_query.length();
                        }

                        std::string l_query_item = l_query.substr(l_qi_begin, l_qi_end - l_qi_begin);

                        // Search for '='
                        size_t l_qi_val_pos = 0;
                        l_qi_val_pos = l_query_item.find('=', 0);
                        std::string l_q_k;
                        std::string l_q_v;
                        if(l_qi_val_pos != std::string::npos)
                        {
                                l_q_k = l_query_item.substr(0, l_qi_val_pos);
                                l_q_v = l_query_item.substr(l_qi_val_pos + 1, l_query_item.length());
                        }
                        else
                        {
                                l_q_k = l_query_item;
                        }

                        //NDBG_PRINT("%s__QUERY__%s: k[%s]: %s\n",
                        //                ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF, l_q_k.c_str(), l_q_v.c_str());

                        // Add to list
                        kv_list_map_t::iterator i_key = m_query.find(l_q_k);
                        if(i_key == m_query.end())
                        {
                                value_list_t l_list;
                                l_list.push_back(l_q_v);
                                m_query[l_q_k] = l_list;
                        }
                        else
                        {
                                i_key->second.push_back(l_q_v);
                        }

                        // Move fwd
                        l_qi_begin = l_qi_end + 1;

                }
        }

}
#endif

#if 0
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const kv_list_map_t &subreq::get_uri_decoded_query(void)
{

        if(m_query_uri_decoded.empty() && !m_query.empty())
        {

                // Decode the arguments for now
                for(kv_list_map_t::const_iterator i_kv = m_query.begin();
                    i_kv != m_query.end();
                    ++i_kv)
                {
                        value_list_t l_value_list;
                        for(value_list_t::const_iterator i_v = i_kv->second.begin();
                            i_v != i_kv->second.end();
                            ++i_v)
                        {
                                std::string l_v = uri_decode(*i_v);
                                l_value_list.push_back(l_v);
                        }

                        std::string l_k = uri_decode(i_kv->first);
                        m_query_uri_decoded[l_k] = l_value_list;
                }
        }

        return m_query_uri_decoded;

}
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#if 0
void subreq::show(bool a_color)
{
        std::string l_host_color = "";
        std::string l_query_color = "";
        std::string l_header_color = "";
        std::string l_body_color = "";
        std::string l_off_color = "";
        if(a_color)
        {
                l_host_color = ANSI_COLOR_FG_BLUE;
                l_query_color = ANSI_COLOR_FG_MAGENTA;
                l_header_color = ANSI_COLOR_FG_GREEN;
                l_body_color = ANSI_COLOR_FG_YELLOW;
                l_off_color = ANSI_COLOR_OFF;
        }

        // Host
        NDBG_OUTPUT("%sUri%s:  %s\n", l_host_color.c_str(), l_off_color.c_str(), m_uri.c_str());
        NDBG_OUTPUT("%sPath%s: %s\n", l_host_color.c_str(), l_off_color.c_str(), m_path.c_str());

        // Query
        for(kv_list_map_t::iterator i_key = m_query.begin();
                        i_key != m_query.end();
            ++i_key)
        {
                NDBG_OUTPUT("%s%s%s: %s\n",
                                l_query_color.c_str(), i_key->first.c_str(), l_off_color.c_str(),
                                i_key->second.begin()->c_str());
        }


        // Headers
        for(kv_list_map_t::iterator i_key = m_headers.begin();
            i_key != m_headers.end();
            ++i_key)
        {
                NDBG_OUTPUT("%s%s%s: %s\n",
                                l_header_color.c_str(), i_key->first.c_str(), l_off_color.c_str(),
                                i_key->second.begin()->c_str());
        }

        // Body
        NDBG_OUTPUT("%sBody%s: %s\n", l_body_color.c_str(), l_off_color.c_str(), m_body.c_str());

}
#endif

} //namespace ns_hlx {
