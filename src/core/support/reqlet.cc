//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    reqlet.cc
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
#include "reqlet.h"
#include "resolver.h"
#include "ndebug.h"

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

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t reqlet::init_with_url(const std::string &a_url, bool a_wildcarding)
{

        int32_t l_status;

        l_status = m_url.parse(a_url);

        // -----------------------------------------------------
        // SPECIAL path processing for Wildcards and meta...
        // -----------------------------------------------------
        if(a_wildcarding)
        {
                special_effects_parse();
                // TODO Check status...
        }

        m_num_to_req = m_path_vector.size();

        //NDBG_PRINT("Showing parsed url.\n");
        //m_url.show();

        if (STATUS_OK != l_status)
        {
                // Failure
                NDBG_PRINT("Error parsing url: %s.\n", a_url.c_str());
                return STATUS_ERROR;
        }

        // Set default tag if none specified
        if(m_tag == "UNDEFINED")
        {
                //char l_buf[256];
                //snprintf(l_buf, 256, "%s-%" PRIu64 "", m_url.m_host.c_str(), m_id);
                m_tag = a_url;
        }

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t reqlet::resolve(void)
{

        int32_t l_status = STATUS_OK;
        std::string l_error;

        // TODO REMOVE!!!
        //NDBG_PRINT("m_url.m_host: %s\n", m_url.m_host.c_str());
        //NDBG_PRINT("m_url.m_port: %d\n", m_url.m_port);

        l_status = resolver::get()->cached_resolve(m_url.m_host, m_url.m_port, m_host_info, l_error);
        if(l_status != STATUS_OK)
        {
                set_response(900, "Address resolution failed.");
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
void reqlet::show_host_info(void)
{
        printf("+-----------+\n");
        printf("| Host Info |\n");
        printf("+-----------+-------------------------\n");
        printf(": m_sock_family:   %d\n", m_host_info.m_sock_family);
        printf(": m_sock_type:     %d\n", m_host_info.m_sock_type);
        printf(": m_sock_protocol: %d\n", m_host_info.m_sock_protocol);
        printf(": m_sa_len:        %d\n", m_host_info.m_sa_len);
        printf("+-------------------------------------\n");

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &reqlet::get_path(void *a_rand)
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
                return m_url.m_path;
        }

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t reqlet::add_option(const char *a_key, const char *a_val)
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
        else if (l_key == "header")
        {
                // Split by ":"
                std::size_t l_pos = l_val.find(":");
                if(l_pos == std::string::npos)
                {
                        NDBG_PRINT("STATUS_ERROR: Bad header string[%s]\n", l_val.c_str());
                        return STATUS_ERROR;
                }

                //NDBG_PRINT("HEADER: %s: %s\n", l_val.substr(0,l_pos).c_str(), l_val.substr(l_pos+1,l_val.length()).c_str());
                m_extra_headers[l_val.substr(0,l_pos)] = l_val.substr(l_pos+1,l_val.length());
        }
        //: ------------------------------------------------
        //:
        //: ------------------------------------------------
        else if (l_key == "tag")
        {

                //NDBG_PRINT("HEADER: %s: %s\n", l_val.substr(0,l_pos).c_str(), l_val.substr(l_pos+1,l_val.length()).c_str());
                m_tag = l_val;

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

int32_t reqlet::special_effects_parse(void)
{

        // -------------------------------------------
        // 1. Break by separator ";"
        // 2. Check for exploded path
        // 3. For each bit after path
        //        Split by Key "=" Value
        // -------------------------------------------
        // Bail out if no path
        if(m_url.m_path.empty())
                return STATUS_OK;

        // -------------------------------------------
        // TODO This seems hacky...
        // -------------------------------------------
        // strtok is NOT thread-safe but not sure it matters here...
        char l_path[2048];
        char *l_save_ptr;
        strncpy(l_path, m_url.m_path.c_str(), 2048);
        char *l_p = strtok_r(l_path, SPECIAL_EFX_OPT_SEPARATOR, &l_save_ptr);
        //printf("Path: %s\n", l_p);
        m_url.m_path = l_p;

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
                //		i_path != m_path_vector.end();
                //		++i_path, ++i_path_cnt)
                //{
                //	NDBG_OUTPUT(": [%6d]: %s\n", i_path_cnt, i_path->c_str());
                //}
        } else {

                m_path_vector.push_back(m_url.m_path);

        }

        // Options...
        while (l_p)
        {
                l_p = strtok_r(NULL, SPECIAL_EFX_OPT_SEPARATOR, &l_save_ptr);
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
int32_t reqlet::path_exploder(std::string a_path_part,
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
int32_t reqlet::parse_path(const char *a_path,
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
const std::string &reqlet::get_label(void)
{
        return m_tag;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void reqlet::set_response(uint16_t a_response_status, const char *a_response)
{
        // Set reqlet response
        if(a_response_status)
        {
                m_response_status = a_response_status;
        }

        std::string l_response(a_response);
        if(!l_response.empty())
        {
                m_response_body = "\"" + l_response + "\"";
        }

        if(m_response_status == 502)
        {
                ++(m_stat_agg.m_num_idle_killed);
        }
        if(m_response_status >= 500)
        {
                ++(m_stat_agg.m_num_errors);
        }

        ++(m_stat_agg.m_num_conn_completed);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void add_stat_to_agg(total_stat_agg_t &ao_stat_agg, const req_stat_t &a_req_stat)
{

        update_stat(ao_stat_agg.m_stat_us_connect, a_req_stat.m_tt_connect_us);
        update_stat(ao_stat_agg.m_stat_us_ssl_connect, a_req_stat.m_tt_ssl_connect_us);
        update_stat(ao_stat_agg.m_stat_us_first_response, a_req_stat.m_tt_first_read_us);
        update_stat(ao_stat_agg.m_stat_us_download, a_req_stat.m_tt_completion_us - a_req_stat.m_tt_header_completion_us);
        update_stat(ao_stat_agg.m_stat_us_end_to_end, a_req_stat.m_tt_completion_us);

        // Totals
        ++ao_stat_agg.m_total_reqs;
        ao_stat_agg.m_total_bytes += a_req_stat.m_total_bytes;

        // Status code
        //NDBG_PRINT("%sSTATUS_CODE%s: %d\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, a_req_stat.m_status_code);
        ++ao_stat_agg.m_status_code_count_map[a_req_stat.m_status_code];

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void add_to_total_stat_agg(total_stat_agg_t &ao_stat_agg, const total_stat_agg_t &a_add_total_stat)
{

        // Stats
        add_stat(ao_stat_agg.m_stat_us_connect , a_add_total_stat.m_stat_us_connect);
        add_stat(ao_stat_agg.m_stat_us_ssl_connect , a_add_total_stat.m_stat_us_ssl_connect);
        add_stat(ao_stat_agg.m_stat_us_first_response , a_add_total_stat.m_stat_us_first_response);
        add_stat(ao_stat_agg.m_stat_us_download , a_add_total_stat.m_stat_us_download);
        add_stat(ao_stat_agg.m_stat_us_end_to_end , a_add_total_stat.m_stat_us_end_to_end);

        ao_stat_agg.m_total_bytes += a_add_total_stat.m_total_bytes;
        ao_stat_agg.m_total_reqs += a_add_total_stat.m_total_reqs;

        ao_stat_agg.m_num_conn_started += a_add_total_stat.m_num_conn_started;
        ao_stat_agg.m_num_conn_completed += a_add_total_stat.m_num_conn_completed;
        ao_stat_agg.m_num_idle_killed += a_add_total_stat.m_num_idle_killed;
        ao_stat_agg.m_num_errors += a_add_total_stat.m_num_errors;
        ao_stat_agg.m_num_bytes_read += a_add_total_stat.m_num_bytes_read;

        for(status_code_count_map_t::const_iterator i_code = a_add_total_stat.m_status_code_count_map.begin();
                        i_code != a_add_total_stat.m_status_code_count_map.end();
                        ++i_code)
        {
                status_code_count_map_t::iterator i_code2;
                if((i_code2 = ao_stat_agg.m_status_code_count_map.find(i_code->first)) == ao_stat_agg.m_status_code_count_map.end())
                {
                        ao_stat_agg.m_status_code_count_map[i_code->first] = i_code->second;
                }
                else
                {
                        i_code2->second += i_code->second;
                }
        }

}



//: ----------------------------------------------------------------------------
//: TODO REMOVE TODO REMOVE TODO REMOVE TODO REMOVE TODO REMOVE TODO REMOVE
//: ----------------------------------------------------------------------------
#if 0
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
url_factory::url_factory(const url_options& config, t_client_batch_stats* peer):
	                m_stats			(peer),
	                m_config		(config),
	                m_nickname		(config.m_str),
	                m_weight		(1.0),
	                m_urls			(),
	                m_do_sequential		(false),
	                m_do_oneshot		(false),
	                m_next_url_num		(0),
	                m_headers			(new Headers)
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

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
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


