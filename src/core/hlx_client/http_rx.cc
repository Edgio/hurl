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
#include "http_rx.h"
#include "ndebug.h"
#include "http_parser/http_parser.h"
#include "resolver.h"

#include <errno.h>
#include <string.h>
#include <string>

#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <algorithm>

#include "tinymt64.h"
#include "city.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: Copy constructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
http_rx::http_rx(const http_rx &a_http_rx):
        m_id(a_http_rx.m_id),
        m_label(a_http_rx.m_label),
        m_host(a_http_rx.m_host),

        m_path(a_http_rx.m_path),
        m_query(a_http_rx.m_query),
        m_fragment(a_http_rx.m_fragment),
        m_userinfo(a_http_rx.m_userinfo),

        m_hostname(a_http_rx.m_hostname),
        m_port(a_http_rx.m_port),
        m_where(a_http_rx.m_where),
        m_scheme(a_http_rx.m_scheme),
        m_host_info(a_http_rx.m_host_info),
        m_idx(a_http_rx.m_idx),

        m_stat_agg(a_http_rx.m_stat_agg),
        m_multipath(a_http_rx.m_multipath),

        m_is_resolved_flag(a_http_rx.m_is_resolved_flag),
        m_num_to_req(a_http_rx.m_num_to_req),
        m_num_reqd(a_http_rx.m_num_reqd),
        m_repeat_path_flag(a_http_rx.m_repeat_path_flag),
        m_path_vector(a_http_rx.m_path_vector),
        m_path_order(a_http_rx.m_path_order),
        m_path_vector_last_idx(a_http_rx.m_path_vector_last_idx),
        m_label_hash(a_http_rx.m_label_hash),
        m_resp(a_http_rx.m_resp)
{

}

//: ----------------------------------------------------------------------------
//: \details: Copy constructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
http_rx &http_rx::operator=(const http_rx &a_http_rx)
{

        // check for self-assignment by comparing the address of the
        // implicit object and the parameter
        if (this == &a_http_rx)
        {
            return *this;
        }

        m_id = a_http_rx.m_id;
        m_label = a_http_rx.m_label;
        m_host = a_http_rx.m_host;

        m_path = a_http_rx.m_path;
        m_query = a_http_rx.m_query;
        m_fragment = a_http_rx.m_fragment;
        m_userinfo = a_http_rx.m_userinfo;

        m_hostname = a_http_rx.m_hostname;
        m_port = a_http_rx.m_port;
        m_where = a_http_rx.m_where;
        m_scheme = a_http_rx.m_scheme;
        m_host_info = a_http_rx.m_host_info;
        m_idx = a_http_rx.m_idx;

        m_stat_agg = a_http_rx.m_stat_agg;
        m_multipath = a_http_rx.m_multipath;

        m_is_resolved_flag = a_http_rx.m_is_resolved_flag;
        m_num_to_req = a_http_rx.m_num_to_req;
        m_num_reqd = a_http_rx.m_num_reqd;
        m_repeat_path_flag = a_http_rx.m_repeat_path_flag;
        m_path_vector = a_http_rx.m_path_vector;
        m_path_order = a_http_rx.m_path_order;
        m_path_vector_last_idx = a_http_rx.m_path_vector_last_idx;
        m_label_hash = a_http_rx.m_label_hash;
        m_resp = a_http_rx.m_resp;

        return *this;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &http_rx::get_path(void *a_rand)
{

        // TODO -make this threadsafe -mutex per???
        if(m_path_vector.size())
        {
                // Rollover..
                if(m_path_order == EXPLODED_PATH_ORDER_SEQUENTIAL)
                {
                        if(m_path_vector_last_idx >= m_path_vector.size())
                        {
                                m_path_vector_last_idx = 0;
                        }
                        return m_path_vector[m_path_vector_last_idx++];
                }
                else
                {
                        uint32_t l_rand_idx = 0;
                        if(a_rand)
                        {
                                tinymt64_t *l_rand_ptr = (tinymt64_t*)a_rand;
                                l_rand_idx = (uint32_t)(tinymt64_generate_uint64(l_rand_ptr) % m_path_vector.size());
                        }
                        else
                        {
                                l_rand_idx = (random() * m_path_vector.size()) / RAND_MAX;
                        }

                        return m_path_vector[l_rand_idx];
                }
        }
        else
        {
                return m_path;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_rx::add_option(const char *a_key, const char *a_val)
{

        std::string l_key(a_key);
        std::string l_val(a_val);

        //NDBG_PRINT("%s: %s\n",a_key, a_val);

        //: ------------------------------------------------
        //:
        //: ------------------------------------------------
        if (l_key == "order")
        {
                if (l_val == "sequential")
                        m_path_order = EXPLODED_PATH_ORDER_SEQUENTIAL;
                else if (l_val == "random")
                        m_path_order = EXPLODED_PATH_ORDER_RANDOM;
                else
                {
                        NDBG_PRINT("STATUS_ERROR: Bad value[%s] for key[%s]\n", l_val.c_str(), l_key.c_str());
                        return STATUS_ERROR;
                }
        }
        //: ------------------------------------------------
        //:
        //: ------------------------------------------------
        else if (l_key == "tag")
        {

                //NDBG_PRINT("HEADER: %s: %s\n", l_val.substr(0,l_pos).c_str(), l_val.substr(l_pos+1,l_val.length()).c_str());
                m_label = l_val;

        }
#if 0
        //: ------------------------------------------------
        //:
        //: ------------------------------------------------
        else if (l_key == "sampling")
        {
                if (l_val == "oneshot") m_run_only_once_flag = true;
                else if (l_val == "reuse") m_run_only_once_flag = false;
                else
                {
                        NDBG_PRINT("STATUS_ERROR: Bad value[%s] for key[%s]\n", l_val.c_str(), l_key.c_str());
                        return STATUS_ERROR;
                }
        }
#endif
        else
        {
                NDBG_PRINT("STATUS_ERROR: Unrecognized key[%s]\n", l_key.c_str());
                return STATUS_ERROR;
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define SPECIAL_EFX_OPT_SEPARATOR ";"
#define SPECIAL_EFX_KV_SEPARATOR "="
int32_t http_rx::special_effects_parse(void)
{

        //NDBG_PRINT("SPECIAL_FX_PARSE: path: %s\n", m_path.c_str());
        // -------------------------------------------
        // 1. Break by separator ";"
        // 2. Check for exploded path
        // 3. For each bit after path
        //        Split by Key "=" Value
        // -------------------------------------------
        // Bail out if no path
        if(m_path.empty())
                return STATUS_OK;

        // -------------------------------------------
        // TODO This seems hacky...
        // -------------------------------------------
        // strtok is NOT thread-safe but not sure it matters here...
        char l_path[2048];
        char *l_save_ptr;
        strncpy(l_path, m_path.c_str(), 2048);
        char *l_p = strtok_r(l_path, SPECIAL_EFX_OPT_SEPARATOR, &l_save_ptr);

        if( m_path.front() != *SPECIAL_EFX_OPT_SEPARATOR ) {
            // Rule out special cases that m_path only contains options
            //
            m_path = l_p;

            int32_t l_status;
            path_substr_vector_t l_path_substr_vector;
            range_vector_t l_range_vector;
            if(l_p)
            {
                l_status = parse_path(l_p, l_path_substr_vector, l_range_vector);
                if(l_status != STATUS_OK)
                {
                    NDBG_PRINT("STATUS_ERROR: Performing parse_path(%s)\n", l_p);
                    return STATUS_ERROR;
                }
            }

            // If empty path explode
            if(l_range_vector.size())
            {
                l_status = path_exploder(std::string(""), l_path_substr_vector, 0, l_range_vector, 0);
                if(l_status != STATUS_OK)
                {
                    NDBG_PRINT("STATUS_ERROR: Performing explode_path(%s)\n", l_p);
                    return STATUS_ERROR;
                }

                // DEBUG show paths
                //NDBG_OUTPUT(" -- Displaying Paths --\n");
                //uint32_t i_path_cnt = 0;
                //for(path_vector_t::iterator i_path = m_path_vector.begin();
                //              i_path != m_path_vector.end();
                //              ++i_path, ++i_path_cnt)
                //{
                //      NDBG_OUTPUT(": [%6d]: %s\n", i_path_cnt, i_path->c_str());
                //}
            }
            else
            {
                m_path_vector.push_back(m_path);
            }
            l_p = strtok_r(NULL, SPECIAL_EFX_OPT_SEPARATOR, &l_save_ptr);
        } else {
            m_path.clear();
        }

        // Options...
        while (l_p)
        {
                if(l_p)
                {
                        char l_options[1024];
                        char *l_options_save_ptr;

                        strncpy(l_options, l_p, 1024);
                        //printf("Options: %s\n", l_options);

                        char *l_k = strtok_r(l_options, SPECIAL_EFX_KV_SEPARATOR, &l_options_save_ptr);
                        char *l_v = strtok_r(NULL, SPECIAL_EFX_KV_SEPARATOR, &l_options_save_ptr);

                        int32_t l_add_option_status;
                        l_add_option_status = add_option(l_k, l_v);
                        if(l_add_option_status != STATUS_OK)
                        {
                                NDBG_PRINT("STATUS_ERROR: Performing add_option(%s,%s)\n", l_k,l_v);
                                // Nuttin doing???

                        }

                        //printf("Key: %s\n", l_k);
                        //printf("Val: %s\n", l_v);

                }
                l_p = strtok_r(NULL, SPECIAL_EFX_OPT_SEPARATOR, &l_save_ptr);
        }


        //NDBG_OUTPUT("m_path: %s\n", m_path.c_str());

        // TODO REMOVE
        //exit(0);


        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
// Assumes expr in form [<val>-<val>] where val can be either int or hex
static int32_t convert_exp_to_range(const std::string &a_range_exp, range_t &ao_range)
{

        char l_expr[64];
        strncpy(l_expr, a_range_exp.c_str(), 64);

        uint32_t l_start;
        uint32_t l_end;
        int32_t l_status;

        l_status = sscanf(l_expr, "[%u-%u]", &l_start, &l_end);
        if(2 == l_status) goto success;

        // Else lets try hex...
        l_status = sscanf(l_expr, "[%x-%x]", &l_start, &l_end);
        if(2 == l_status) goto success;

        return STATUS_ERROR;

success:
        // Check range...
        if(l_start > l_end)
        {
                NDBG_PRINT("STATUS_ERROR: Bad range start[%u] > end[%u]\n", l_start, l_end);
                return STATUS_ERROR;
        }

        // Successfully matched we outie
        ao_range.m_start = l_start;
        ao_range.m_end = l_end;
        return STATUS_OK;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_rx::path_exploder(std::string a_path_part,
                const path_substr_vector_t &a_substr_vector, uint32_t a_substr_idx,
                const range_vector_t &a_range_vector, uint32_t a_range_idx)
{

        //a_path_part

        if(a_substr_idx >= a_substr_vector.size())
        {
                m_path_vector.push_back(a_path_part);
                return STATUS_OK;
        }

        a_path_part.append(a_substr_vector[a_substr_idx]);
        ++a_substr_idx;

        if(a_range_idx >= a_range_vector.size())
        {
                m_path_vector.push_back(a_path_part);
                return STATUS_OK;
        }

        range_t l_range = a_range_vector[a_range_idx];
        ++a_range_idx;
        for(uint32_t i_r = l_range.m_start; i_r <= l_range.m_end; ++i_r)
        {
                char l_num_str[32];
                sprintf(l_num_str, "%d", i_r);
                std::string l_path = a_path_part;
                l_path.append(l_num_str);
                path_exploder(l_path, a_substr_vector, a_substr_idx,a_range_vector,a_range_idx);
        }

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_rx::parse_path(const char *a_path,
                path_substr_vector_t &ao_substr_vector,
                range_vector_t &ao_range_vector)
{

        //NDBG_PRINT("a_path: %s\n", a_path);

        // If has range '[' char assume it's explodable path
        std::string l_path(a_path);
        size_t l_range_start_pos = 0;
        size_t l_range_end_pos = 0;
        size_t l_cur_str_pos = 0;

        while((l_range_start_pos = l_path.find("[", l_cur_str_pos)) != std::string::npos)
        {
                if((l_range_end_pos = l_path.find("]", l_range_start_pos)) == std::string::npos)
                {
                        NDBG_PRINT("STATUS_ERROR: Bad range for path: %s at pos: %lu\n", a_path, l_range_start_pos);
                        return STATUS_ERROR;
                }

                // Push path back...
                std::string l_substr = l_path.substr(l_cur_str_pos, l_range_start_pos - l_cur_str_pos);
                //NDBG_PRINT("l_substr: %s from %lu -- %lu\n", l_substr.c_str(), l_cur_str_pos, l_range_start_pos);
                ao_substr_vector.push_back(l_substr);

                // Convert to range
                std::string l_range_exp = l_path.substr(l_range_start_pos, l_range_end_pos - l_range_start_pos + 1);

                std::string::iterator end_pos = std::remove(l_range_exp.begin(), l_range_exp.end(), ' ');
                l_range_exp.erase(end_pos, l_range_exp.end());

                //NDBG_PRINT("l_range_exp: %s\n", l_range_exp.c_str());

                range_t l_range;
                int l_status = STATUS_OK;
                l_status = convert_exp_to_range(l_range_exp, l_range);
                if(STATUS_OK != l_status)
                {
                        NDBG_PRINT("STATUS_ERROR: performing convert_exp_to_range(%s)\n", l_range_exp.c_str());
                        return STATUS_ERROR;
                }

                ao_range_vector.push_back(l_range);

                l_cur_str_pos = l_range_end_pos + 1;

                // Searching next at:
                //NDBG_PRINT("Searching next at: %lu -- result: %lu\n", l_cur_str_pos, l_path.find("[", l_cur_str_pos));

        }

        // Get trailing string
        std::string l_substr = l_path.substr(l_cur_str_pos, l_path.length() - l_cur_str_pos);
        //NDBG_PRINT("l_substr: %s from %lu -- %lu\n", l_substr.c_str(), l_cur_str_pos, l_path.length());
        ao_substr_vector.push_back(l_substr);

#if 0
        // -------------------------------------------
        // Explode the lists
        // -------------------------------------------
        for(path_substr_vector_t::iterator i_substr = ao_substr_vector.begin();
                        i_substr != ao_substr_vector.end();
                        ++i_substr)
        {
                NDBG_PRINT("SUBSTR: %s\n", i_substr->c_str());
        }

        for(range_vector_t::iterator i_range = ao_range_vector.begin();
                        i_range != ao_range_vector.end();
                        ++i_range)
        {
                NDBG_PRINT("RANGE: %u -- %u\n", i_range->m_start, i_range->m_end);
        }
#endif


        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &http_rx::get_label(void)
{
        return m_label;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t http_rx::get_label_hash(void)
{
        if(!m_label_hash)
        {
                m_label_hash = CityHash64(m_label.data(), m_label.length());
        }
        return m_label_hash;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void http_rx::set_host(const std::string &a_host)
{
        m_host = a_host;

        // Update tag
        if(m_scheme == nconn::SCHEME_TCP) m_label = "http://";
        else if(m_scheme == nconn::SCHEME_SSL) m_label = "https://";
        m_label += a_host;
        m_label += m_path;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void http_rx::set_response(uint16_t a_response_status, const char *a_response)
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

        if(a_response_status == 502)
        {
                ++m_stat_agg.m_num_idle_killed;
        }
        if(a_response_status >= 500)
        {
                ++m_stat_agg.m_num_errors;
        }

        ++m_stat_agg.m_num_conn_completed;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_rx::resolve(resolver &a_resolver)
{
        if(m_is_resolved_flag)
        {
                return STATUS_OK;
        }

        int32_t l_status = STATUS_OK;
        std::string l_error;

        // TODO REMOVE!!!
        //NDBG_PRINT("m_url.m_host: %s\n", m_url.m_host.c_str());
        //NDBG_PRINT("m_url.m_port: %d\n", m_url.m_port);

        l_status = a_resolver.cached_resolve(m_host, m_port, m_host_info, l_error);
        if(l_status != STATUS_OK)
        {
                //set_response(900, "Address resolution failed.");
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
int32_t http_rx::init_with_url(const std::string &a_url, bool a_wildcarding)
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
                                        m_scheme = nconn::SCHEME_TCP;
                                }
                                else if(l_part == "https")
                                {
                                        m_scheme = nconn::SCHEME_SSL;
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
                case nconn::SCHEME_TCP:
                {
                        m_port = 80;
                        break;
                }
                case nconn::SCHEME_SSL:
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

        // -----------------------------------------------------
        // SPECIAL path processing for Wildcards and meta...
        // -----------------------------------------------------
        if(a_wildcarding)
        {
                special_effects_parse();
                // TODO Check status...
        }

        if(m_path_vector.size() > 1)
        {
                m_multipath = true;
        }
        else
        {
                m_multipath = false;
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

        // Set default tag if none specified
        if(m_label == "UNDEFINED")
        {
                //char l_buf[256];
                //snprintf(l_buf, 256, "%s-%" PRIu64 "", m_url.m_host.c_str(), m_id);
                m_label = l_url_fixed;
        }

        return STATUS_OK;

        //NDBG_PRINT("Parsed url: %s\n", l_url_fixed.c_str());

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#if 0
void http_rx::set_uri(const std::string &a_uri)
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

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#if 0
const kv_list_map_t &http_rx::get_uri_decoded_query(void)
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
void http_rx::show(bool a_color)
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

//: ----------------------------------------------------------------------------
//: TODO REMOVE TODO REMOVE TODO REMOVE TODO REMOVE TODO REMOVE TODO REMOVE
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#if 0
url_factory::url_factory(const url_options& config, t_client_batch_stats* peer):
                        m_stats                 (peer),
                        m_config                (config),
                        m_nickname              (config.m_str),
                        m_weight                (1.0),
                        m_urls                  (),
                        m_do_sequential         (false),
                        m_do_oneshot            (false),
                        m_next_url_num          (0),
                        m_headers                       (new Headers)
{
        std::string servername;
        protocol_t protocol = PROTO_HTTP;
        std::string filelist;
        std::string urlpattern;

        NDBG_PRINT("setting up url batch '%s'\n", config.m_str.c_str());

        for (size_t i = 0; i < config.m_opts.size(); ++i)
        {
                std::string key = config.m_opts[i].first, val = config.m_opts[i].second;

                NDBG_PRINT("considering '%s'='%s'\n", key.c_str(), val.c_str());

                //: ------------------------------------------------
                //:
                //: ------------------------------------------------
                if (key == "weight")
                {
                        std::istringstream iss(val);
                        iss >> m_weight;
                        // fprintf(stderr, "set weight=%f\n", m_weight);
                }

                //: ------------------------------------------------
                //:
                //: ------------------------------------------------
                else if (key == "order")
                {
                        if (val == "sequential") m_do_sequential = true;
                        else if (val == "random") m_do_sequential = false;
                        else
                        {
                                FATAL("in url config '%s': invalid order '%s' (expected 'sequential' or 'random')\n",
                                                config.m_str.c_str(), val.c_str());
                        }
                }

                //: ------------------------------------------------
                //:
                //: ------------------------------------------------
                else if (key == "filelist")
                {
                        filelist = val;
                }

                //: ------------------------------------------------
                //:
                //: ------------------------------------------------
                else if (key == "urlpattern")
                {
                        urlpattern = val;
                }

                //: ------------------------------------------------
                //:
                //: ------------------------------------------------
                else if (key == "server")
                {
                        servername = val;
                }

                //: ------------------------------------------------
                //:
                //: ------------------------------------------------
                else if (key == "protocol")
                {
                        if (val == "http") protocol = PROTO_HTTP;
                        else if (val == "https") protocol = PROTO_HTTPS;
                        else
                        {
                                FATAL("in url config '%s': invalid protocol '%s' (expected 'http'%s)\n",
                                                config.m_str.c_str(), val.c_str(),
                                                " or 'https'"
                                );
                        }
                }

                //: ------------------------------------------------
                //:
                //: ------------------------------------------------
                else if (key == "header")
                {
                        std::istringstream iss(val);
                        std::string hdrname, hdrval;
                        std::getline(iss, hdrname, ':');
                        std::getline(iss, hdrval);
                        m_headers->push_back(std::make_pair(hdrname, hdrval));
                }

                //: ------------------------------------------------
                //:
                //: ------------------------------------------------
                else if (key == "sampling")
                {
                        if (val == "oneshot") m_do_oneshot = true;
                        else if (val == "reuse") m_do_oneshot = false;
                        else
                        {
                                FATAL("in url config '%s': invalid sampling '%s' (expected 'oneshot' or 'reuse')\n",
                                                config.m_str.c_str(), val.c_str());
                        }
                }

                //: ------------------------------------------------
                //:
                //: ------------------------------------------------
                else if (key == "nickname")
                {
                        m_nickname = val;
                }
        }


        //: ------------------------------------------------
        //:
        //: ------------------------------------------------
        std::tr1::shared_ptr<Server> server;
        if (servername.length() > 0)
        {
                server.reset(new Server(servername, protocol));
        }

        //: ------------------------------------------------
        //:
        //: ------------------------------------------------
        if (filelist.length() > 0)
        {
                char line[5000];

                FILE* const fp = fopen(filelist.c_str(), "r");
                if (fp == NULL)
                {
                        perror(filelist.c_str());
                        exit(1);
                }

                int c = 0;

                while (fgets(line, sizeof(line), fp) != (char*) 0)
                {
                        /* Nuke trailing newline. */
                        if (line[strlen(line) - 1] == '\n')
                                line[strlen(line) - 1] = '\0';

                        std::tr1::shared_ptr<Url> newurl(
                                        new Url(line, server, m_headers, &m_stats));

                        m_urls.push_back(newurl);
                        ++c;
                }

                NDBG_PRINT("loaded %d urls from '%s'\n", c, filelist.c_str());
        }

        //: ------------------------------------------------
        //:
        //: ------------------------------------------------
        if (urlpattern.length() > 0)
        {
                int c = 0;

                std::vector<std::string> items;
                items.push_back(urlpattern);
                while (items.size() > 0)
                {

                        std::istringstream iss(items.back());
                        items.pop_back();
                        std::string pfx, rng, sfx;
                        std::getline(iss, pfx, '[');
                        std::getline(iss, rng, ']');
                        std::getline(iss, sfx);

                        if (pfx.size() == iss.str().size())
                        {
                                if (c == 0 || items.size() == 0)
                                        NDBG_PRINT("'%s' -> #%-8d: '%s'\n", urlpattern.c_str(), c, pfx.c_str());

                                std::tr1::shared_ptr<Url> newurl(
                                                new Url(pfx, server, m_headers, &m_stats));

                                m_urls.push_back(newurl);
                                ++c;
                        }
                        else
                        {
                                std::istringstream iss2(rng);
                                std::string startstr, endstr;
                                std::getline(iss2, startstr, '-');
                                std::getline(iss2, endstr);
                                if (startstr.size() == 0 || endstr.size() == 0)
                                {
                                        FATAL("in url pattern '%s': invalid range '%s'\n",
                                                        urlpattern.c_str(), rng.c_str());
                                }

                                bool do_hex = false;
                                bool do_upperhex = true;
                                if (startstr.substr(0, 2) == "0x")
                                {
                                        do_hex = true;
                                        do_upperhex = false;
                                        startstr = startstr.substr(2);
                                }
                                else if (startstr.substr(0, 2) == "0X")
                                {
                                        do_hex = true;
                                        do_upperhex = true;
                                        startstr = startstr.substr(2);
                                }
                                if (endstr.substr(0, 2) == "0x")
                                {
                                        do_hex = true;
                                        do_upperhex = false;
                                        endstr = endstr.substr(2);
                                }
                                else if (endstr.substr(0, 2) == "0X")
                                {
                                        do_hex = true;
                                        do_upperhex = true;
                                        endstr = endstr.substr(2);
                                }

                                int start = -1;
                                if (do_hex)
                                        std::istringstream(startstr) >> std::hex >> start;
                                else
                                        std::istringstream(startstr) >> start;
                                int end = -1;
                                if (do_hex)
                                        std::istringstream(endstr) >> std::hex >> end;
                                else
                                        std::istringstream(endstr) >> end;

                                size_t width = 0;
                                if (startstr.size() > 1 && startstr[0] == '0')
                                        width = std::max(width, startstr.size());
                                if (endstr.size() > 1 && endstr[0] == '0')
                                        width = std::max(width, endstr.size());

                                if (start < 0 || end < 0 || start > end)
                                {
                                        FATAL("in url pattern '%s': invalid range '%s'\n",
                                                        urlpattern.c_str(), rng.c_str());
                                }


                                // push items back into the queue in
                                // reverse order so that we can pop
                                // them off the back in the correct
                                // order:
                                for (int i = end; i >= start; --i)
                                {
                                        std::ostringstream oss;
                                        oss << pfx
                                                        << (do_hex ? std::hex : std::dec)
                                                        << (do_upperhex ? std::uppercase : std::nouppercase)
                                                        << std::setw(width)
                                        << std::setfill('0') << i
                                        << sfx;
                                        items.push_back(oss.str());
                                }
                        }
                }

                NDBG_PRINT("generated %d urls from '%s'\n",c, urlpattern.c_str());

                if (m_do_sequential == false && m_do_oneshot == true)
                {
                        // random oneshot is more efficiently handled
                        // by first random-shuffling the urls, and
                        // then accessing the shuffled urls in sequence

                        std::random_shuffle(m_urls.begin(), m_urls.end());
                        m_do_sequential = true;
                }
        }
}
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#if 0
const Url & url_factory::pick_url()
{
        if (m_urls.size() == 0)
                return Url();

        size_t url_num;

        if (m_do_sequential)
        {
                assert(m_next_url_num < m_urls.size());
                url_num = m_next_url_num;
                if (!m_do_oneshot)
                        m_next_url_num = (m_next_url_num + 1) % m_urls.size();
        }
        else
        {
                url_num = size_t(random()) % m_urls.size();
        }

        const Url &result = m_urls[url_num];

        if (m_do_oneshot)
        {
                m_urls.erase(m_urls.begin() + url_num);
        }

        return result;
}
#endif


} //namespace ns_hlx {
