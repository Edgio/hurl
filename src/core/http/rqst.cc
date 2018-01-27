//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    rqst.cc
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
#include "hurl/support/uri.h"
#include "hurl/support/nbq.h"
#include "hurl/http/rqst.h"
#include "hurl/support/trace.h"
#include "hurl/status.h"
#include "http_parser/http_parser.h"
#include <string.h>
#include <stdlib.h>
namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
rqst::rqst(void):
        hmsg(),
        m_p_url(),
        m_method(HTTP_GET),
        m_expect(false),
        m_url_parsed(false),
        m_url(),
        m_url_path(),
        m_url_query(),
        m_url_query_map(),
        m_url_fragment()
{
        init(true);
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
rqst::~rqst(void)
{

}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void rqst::init(bool a_save)
{
        hmsg::init(a_save);
        m_type = hmsg::TYPE_RQST;
        m_save = a_save;
        m_p_url.clear();
        m_method = HTTP_GET;
        m_expect = false;
        m_url.clear();
        m_url_path.clear();
        m_url_query.clear();
        m_url_query_map.clear();
        m_url_fragment.clear();
        m_url_parsed = false;
        if(m_http_parser_settings)
        {
                m_http_parser_settings->on_status = hp_on_status;
                m_http_parser_settings->on_message_complete = hp_on_message_complete;
                m_http_parser_settings->on_message_begin = hp_on_message_begin;
                m_http_parser_settings->on_url = hp_on_url;
                m_http_parser_settings->on_header_field = hp_on_header_field;
                m_http_parser_settings->on_header_value = hp_on_header_value;
                m_http_parser_settings->on_headers_complete = hp_on_headers_complete;
                m_http_parser_settings->on_body = hp_on_body;
        }
        if(m_http_parser)
        {
                http_parser_init(m_http_parser, HTTP_REQUEST);
                m_http_parser->data = this;
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
const std::string &rqst::get_url()
{
        static std::string l_empty_str = std::string();
        if(!m_url_parsed)
        {
                int32_t l_status = parse_uri();
                if(l_status != HURL_STATUS_OK)
                {
                        // return empty string
                        return l_empty_str;
                }
        }
        return m_url;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &rqst::get_url_path()
{
        if(!m_url_parsed)
        {
                int32_t l_status = parse_uri();
                if(l_status != HURL_STATUS_OK)
                {
                        // do nothing...
                }
        }
        return m_url_path;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &rqst::get_url_query()
{
        if(!m_url_parsed)
        {
                int32_t l_status = parse_uri();
                if(l_status != HURL_STATUS_OK)
                {
                        // do nothing...
                }
        }
        return m_url_query;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const query_map_t &rqst::get_url_query_map()
{
        if(m_url_query_map.empty())
        {
                int32_t l_status = parse_query(get_url_query(), m_url_query_map);
                if(l_status != HURL_STATUS_OK)
                {
                        // do nothing...
                }
        }
        return m_url_query_map;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &rqst::get_url_fragment()
{
        if(!m_url_parsed)
        {
                int32_t l_status = parse_uri();
                if(l_status != HURL_STATUS_OK)
                {
                        // do nothing...
                }
        }
        return m_url_fragment;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const char *rqst::get_method_str()
{
        // -------------------------------------------------
        // TODO enum cast here is of course uncool -but
        // m_method value is populated by http_parser so
        // "ought" to be safe.
        // Will fix later
        // -------------------------------------------------
        return http_method_str((enum http_method)m_method);
}
//: ----------------------------------------------------------------------------
//:                               Setters
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t rqst::parse_uri()
{
        if(m_url_parsed)
        {
                return HURL_STATUS_OK;
        }

        // Copy out the url...
        // TODO zero copy???
        if(m_q && m_p_url.m_off && m_p_url.m_len)
        {
                char *l_path = NULL;
                l_path = copy_part(*m_q, m_p_url.m_off, m_p_url.m_len);
                if(l_path && strlen(l_path))
                {
                        m_url = l_path;
                        free(l_path);
                }
        }
        std::string l_url_fixed = m_url;
        // Find scheme prefix "://"
        if(m_url.find("://", 0) == std::string::npos)
        {
                l_url_fixed = "http://bloop.com" + m_url;
        }
        http_parser_url l_url;
        http_parser_url_init(&l_url);
        // silence bleating memory sanitizers...
        //memset(&l_url, 0, sizeof(l_url));
        int l_status;
        l_status = http_parser_parse_url(l_url_fixed.c_str(), l_url_fixed.length(), 0, &l_url);
        if(l_status != 0)
        {
                TRC_ERROR("parsing url: %s\n", l_url_fixed.c_str());
                // TODO get error msg from http_parser
                return HURL_STATUS_ERROR;
        }
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
                        case UF_PATH:
                        {
                                m_url_path = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_PATH]: %s\n", m_url_path.c_str());
                                break;
                        }
                        case UF_QUERY:
                        {
                                m_url_query = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_QUERY]: %s\n", m_url_query.c_str());
                                break;
                        }
                        case UF_FRAGMENT:
                        {
                                m_url_fragment = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_FRAGMENT]: %s\n", m_url_fragment.c_str());
                                break;
                        }
                        case UF_USERINFO:
                        case UF_SCHEMA:
                        case UF_HOST:
                        case UF_PORT:
                        default:
                        {
                                break;
                        }
                        }
                }
        }
        m_url_parsed = true;
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t rqst::parse_query(const std::string &a_query, query_map_t &ao_query_map)
{
        //TODO -zero copy version??? -offsets to pos
        size_t l_pos = 0;
        size_t l_last_pos = 0;
        do {
                l_pos = a_query.find("&", l_last_pos);
                std::string l_part = a_query.substr(l_last_pos, l_pos - l_last_pos);

                // Break by "="
                std::string l_key;
                std::string l_val;
                size_t l_eq_pos = l_part.find("=");
                l_key = uri_decode(l_part.substr(0, l_eq_pos));
                if(l_eq_pos != std::string::npos)
                {
                        l_val = uri_decode(l_part.substr(l_eq_pos + 1, std::string::npos));
                }
                //printf("PART: l_key: %s l_val: %s\n", l_key.c_str(), l_val.c_str());
                query_map_t::iterator i_obj = ao_query_map.find(l_key);
                if(i_obj != ao_query_map.end())
                {
                        i_obj->second.push_back(l_val);
                }
                else
                {
                        ns_hurl::str_list_t l_list;
                        l_list.push_back(l_val);
                        ao_query_map[l_key] = l_list;
                }
                l_last_pos = l_pos + 1;
        } while(l_pos != std::string::npos);
        return 0;
}
//: ----------------------------------------------------------------------------
//:                               Debug
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void rqst::show(void)
{
        m_q->reset_read();
        cr_list_t::const_iterator i_k = m_p_h_list_key.begin();
        cr_list_t::const_iterator i_v = m_p_h_list_val.begin();
        print_part(*m_q, m_p_url.m_off, m_p_url.m_len);
        TRC_OUTPUT("\r\n");
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
