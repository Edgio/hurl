//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlx_client.cc
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
#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include "hlx_client.h"
#include "reqlet.h"
#include "util.h"
#include "ssl_util.h"
#include "ndebug.h"
#include "resolver.h"
#include "reqlet.h"
#include "nconn_ssl.h"
#include "nconn_tcp.h"
#include "tinymt64.h"
#include "t_client.h"

#include <string.h>

// getrlimit
#include <sys/time.h>
#include <sys/resource.h>

// signal
#include <signal.h>

// Shared pointer
//#include <tr1/memory>

#include <list>
#include <algorithm>
#include <unordered_set>

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h> // For getopt_long
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <math.h>

// json support
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::init_client_list(void)
{

        // -------------------------------------------
        // Bury the config into a settings struct
        // -------------------------------------------
        settings_struct_t l_settings;
        l_settings.m_verbose = m_verbose;
        l_settings.m_color = m_color;
        l_settings.m_quiet = m_quiet;
        l_settings.m_show_summary = m_show_summary;
        l_settings.m_url = m_url;
        l_settings.m_header_map = m_header_map;
        l_settings.m_verb = m_verb;
        l_settings.m_req_body = m_req_body;
        l_settings.m_req_body_len = m_req_body_len;
        l_settings.m_evr_loop_type = (evr_loop_type_t)m_evr_loop_type;
        l_settings.m_num_parallel = m_num_parallel;
        l_settings.m_num_threads = m_num_threads;
        l_settings.m_timeout_s = m_timeout_s;
        l_settings.m_run_time_s = m_run_time_s;
        l_settings.m_request_mode = m_request_mode;
        l_settings.m_num_end_fetches = m_num_end_fetches;
        l_settings.m_connect_only = m_connect_only;
        l_settings.m_save_response = m_save_response;
        l_settings.m_collect_stats = m_collect_stats;
        l_settings.m_use_persistent_pool = m_use_persistent_pool;
        l_settings.m_num_reqs_per_conn = m_num_reqs_per_conn;
        l_settings.m_sock_opt_recv_buf_size = m_sock_opt_recv_buf_size;
        l_settings.m_sock_opt_send_buf_size = m_sock_opt_send_buf_size;
        l_settings.m_sock_opt_no_delay = m_sock_opt_no_delay;
        l_settings.m_ssl_ctx = m_ssl_ctx;
        l_settings.m_ssl_cipher_list = m_ssl_cipher_list;
        l_settings.m_ssl_options_str = m_ssl_options_str;
        l_settings.m_ssl_options = m_ssl_options;
        l_settings.m_ssl_verify = m_ssl_verify;
        l_settings.m_ssl_sni = m_ssl_sni;
        l_settings.m_ssl_ca_file = m_ssl_ca_file;
        l_settings.m_ssl_ca_path = m_ssl_ca_path;
        l_settings.m_resolver = m_resolver;

        if(m_rate > 0)
        {
                l_settings.m_rate = (int32_t)((double)m_rate / (double)m_num_threads);
                if(l_settings.m_rate == 0)
                {
                        l_settings.m_rate = 1;
                }
        }
        else
        {
                l_settings.m_rate = m_rate;
        }

        // -------------------------------------------
        // Create t_client list...
        // -------------------------------------------
        for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
        {
                t_client *l_t_client = NULL;
                l_t_client = new t_client(l_settings);
                m_t_client_list.push_back(l_t_client);
        }

        return STATUS_OK;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::run(void)
{
        int l_status = 0;
        if(!m_is_initd)
        {
                l_status = init();
                if(HLX_CLIENT_STATUS_OK != l_status)
                {
                        return HLX_CLIENT_STATUS_ERROR;
                }
        }
        // at this point m_resolver is a resolver instance

        if(m_t_client_list.empty())
        {
                l_status = init_client_list();
                if(STATUS_OK != l_status)
                {
                        return HLX_CLIENT_STATUS_ERROR;
                }
        }

        // -------------------------------------------
        // Reqlets
        // TODO no copy!
        // -------------------------------------------
        reqlet_vector_t l_reqlet_vector_all(m_host_list.size());
        uint32_t l_reqlet_num = 0;
        for(host_list_t::iterator i_host = m_host_list.begin();
                        i_host != m_host_list.end();
                        ++i_host, ++l_reqlet_num)
        {
                // Create a re
                reqlet *l_reqlet = new reqlet(l_reqlet_num, 1);
                parsed_url l_url;

                // If host has url use that
                if(!i_host->m_url.empty())
                {
                        l_reqlet->init_with_url(i_host->m_url);
                        i_host->m_host = l_reqlet->m_url.m_host;
                        i_host->m_hostname = l_reqlet->m_url.m_hostname;
                        l_url.parse(i_host->m_url);
                }
                else
                {
                        l_reqlet->init_with_url(m_url);
                        l_url.parse(i_host->m_host);
                }

                // Get host and port if exist

                if(strchr(i_host->m_host.c_str(), (int)':'))
                {
                        l_reqlet->set_host(l_url.m_host);
                        l_reqlet->set_port(l_url.m_port);
                }
                else
                {
                        // TODO make set host take const
                        l_reqlet->set_host(i_host->m_host);
                }

                if(!i_host->m_hostname.empty())
                {
                     l_reqlet->m_url.m_hostname = i_host->m_hostname;
                }
                if(!i_host->m_id.empty())
                {
                     l_reqlet->m_url.m_id = i_host->m_id;
                }
                if(!i_host->m_where.empty())
                {
                     l_reqlet->m_url.m_where = i_host->m_where;
                }

                // Overrides if exist
                if(m_port)
                {
                        l_reqlet->set_port(m_port);
                }

                if(m_scheme != SCHEME_NONE)
                {
                        switch(m_scheme)
                        {
                        case SCHEME_HTTPS:
                        {
                                l_reqlet->m_url.m_scheme = nconn::SCHEME_SSL;
                                break;
                        }
                        case SCHEME_HTTP:
                        {
                                l_reqlet->m_url.m_scheme = nconn::SCHEME_TCP;
                                break;
                        }
                        default:
                        {
                                l_reqlet->m_url.m_scheme = nconn::SCHEME_TCP;
                                break;
                        }
                        }
                        l_reqlet->set_port(m_port);
                }

                // Add to vector
                l_reqlet_vector_all[l_reqlet_num] = l_reqlet;
        }


        // -------------------------------------------
        // Setup client requests
        // -------------------------------------------
        uint32_t i_client_idx = 0;
        uint32_t l_reqlet_list_size = l_reqlet_vector_all.size();
        for(t_client_list_t::iterator i_client = m_t_client_list.begin();
                        i_client != m_t_client_list.end();
                        ++i_client, ++i_client_idx)
        {
                if(m_split_requests_by_thread)
                {
                        reqlet_vector_t l_reqlet_vector;

                        // Calculate index
                        uint32_t l_idx = i_client_idx*(l_reqlet_list_size/m_num_threads);
                        uint32_t l_len = l_reqlet_list_size/m_num_threads;

                        if(i_client_idx + 1 == m_num_threads)
                        {
                                // Get remainder
                                l_len = l_reqlet_list_size - (i_client_idx * l_len);
                        }
                        for(uint32_t i_dx = 0; i_dx < l_len; ++i_dx)
                        {
                                l_reqlet_vector.push_back(l_reqlet_vector_all[l_idx + i_dx]);
                        }

                        (*i_client)->set_reqlets(l_reqlet_vector);
                }
                else
                {
                        (*i_client)->set_reqlets(l_reqlet_vector_all);
                }


                //if(a_settings.m_verbose)
                //{
                //        NDBG_PRINT("Creating...\n");
                //}

                // Construct with settings...
                (*i_client)->clear_headers();
                for(header_map_t::iterator i_header = m_header_map.begin();
                    i_header != m_header_map.end();
                    ++i_header)
                {
                        (*i_client)->set_header(i_header->first, i_header->second);
                }

                // Caculate num parallel per thread
                if(m_num_end_fetches != -1)
                {
                        uint32_t l_num_fetches_per_thread = m_num_end_fetches / m_num_threads;
                        uint32_t l_remainder_fetches = m_num_end_fetches % m_num_threads;
                        if (i_client_idx == (m_num_threads - 1))
                        {
                                l_num_fetches_per_thread += l_remainder_fetches;
                        }
                        (*i_client)->set_end_fetches(l_num_fetches_per_thread);
                }
        }

        // -------------------------------------------
        // Delete local copies
        // TODO no copy!
        // -------------------------------------------
        for(size_t i_reqlet = 0;
            i_reqlet < l_reqlet_vector_all.size();
            ++i_reqlet)
        {
                reqlet *i_reqlet_ptr = l_reqlet_vector_all[i_reqlet];
                if(i_reqlet_ptr)
                {
                        delete i_reqlet_ptr;
                        i_reqlet_ptr = NULL;
                }
        }
        l_reqlet_vector_all.clear();


        set_start_time_ms(get_time_ms());

        // -------------------------------------------
        // Run...
        // -------------------------------------------
        for(t_client_list_t::iterator i_t_client = m_t_client_list.begin();
                        i_t_client != m_t_client_list.end();
                        ++i_t_client)
        {
                (*i_t_client)->run();
        }

        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::stop(void)
{
        int32_t l_retval = HLX_CLIENT_STATUS_OK;

        for (t_client_list_t::iterator i_t_client = m_t_client_list.begin();
                        i_t_client != m_t_client_list.end();
                        ++i_t_client)
        {
                (*i_t_client)->stop();
        }

        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::wait_till_stopped(void)
{
        //int32_t l_retval = HLX_CLIENT_STATUS_OK;

        // -------------------------------------------
        // Join all threads before exit
        // -------------------------------------------
        for(t_client_list_t::iterator i_client = m_t_client_list.begin();
            i_client != m_t_client_list.end();
            ++i_client)
        {
                //if(m_verbose)
                //{
                //      NDBG_PRINT("joining...\n");
                //}
                pthread_join(((*i_client)->m_t_run_thread), NULL);

        }
        //return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool hlx_client::is_running(void)
{
        for (t_client_list_t::iterator i_t_client = m_t_client_list.begin();
                        i_t_client != m_t_client_list.end();
                        ++i_t_client)
        {
                if((*i_t_client)->is_running())
                        return true;
        }
        return false;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_quiet(bool a_val)
{
        m_quiet = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_verbose(bool a_val)
{
        m_verbose = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_color(bool a_val)
{
        m_color = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_url(const std::string &a_url)
{
        m_url = a_url;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_wildcarding(bool a_val)
{
        m_wildcarding = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_data(const char *a_data, uint32_t a_len)
{

        // If a_data starts with @ assume file
        if(a_data[0] == '@')
        {
                std::string l_file_str = a_data + 1;

                // ---------------------------------------
                // Check is a file
                // TODO
                // ---------------------------------------
                struct stat l_stat;
                int32_t l_status = STATUS_OK;
                l_status = stat(l_file_str.c_str(), &l_stat);
                if(l_status != 0)
                {
                        //NDBG_PRINT("Error performing stat on file: %s.  Reason: %s\n", a_ai_cache_file.c_str(), strerror(errno));
                        return HLX_CLIENT_STATUS_ERROR;
                }

                // Check if is regular file
                if(!(l_stat.st_mode & S_IFREG))
                {
                        //NDBG_PRINT("Error opening file: %s.  Reason: is NOT a regular file\n", a_ai_cache_file.c_str());
                        return HLX_CLIENT_STATUS_ERROR;
                }

                // ---------------------------------------
                // Open file...
                // ---------------------------------------
                FILE * l_file;
                l_file = fopen(l_file_str.c_str(),"r");
                if (NULL == l_file)
                {
                        //NDBG_PRINT("Error opening file: %s.  Reason: %s\n", a_ai_cache_file.c_str(), strerror(errno));
                        return HLX_CLIENT_STATUS_ERROR;
                }

                // ---------------------------------------
                // Read in file...
                // ---------------------------------------
                int32_t l_size = l_stat.st_size;

                // Bounds check -remove later
                if(l_size > 8*1024)
                {
                        return HLX_CLIENT_STATUS_ERROR;
                }

                m_req_body = (char *)malloc(sizeof(char)*l_size);
                m_req_body_len = l_size;

                int32_t l_read_size;
                l_read_size = fread(m_req_body, 1, l_size, l_file);
                if(l_read_size != l_size)
                {
                        //NDBG_PRINT("Error performing fread.  Reason: %s [%d:%d]\n",
                        //                strerror(errno), l_read_size, l_size);
                        return HLX_CLIENT_STATUS_ERROR;
                }

                // ---------------------------------------
                // Close file...
                // ---------------------------------------
                l_status = fclose(l_file);
                if (STATUS_OK != l_status)
                {
                        //NDBG_PRINT("Error performing fclose.  Reason: %s\n", strerror(errno));
                        return HLX_CLIENT_STATUS_ERROR;
                }
        }
        else
        {
                // Bounds check -remove later
                if(a_len > 8*1024)
                {
                        return HLX_CLIENT_STATUS_ERROR;
                }

                m_req_body = (char *)malloc(sizeof(char)*a_len);
                memcpy(m_req_body, a_data, a_len);
                m_req_body_len = a_len;

        }

        // Add content length
        char l_len_str[64];
        sprintf(l_len_str, "%u", m_req_body_len);
        set_header("Content-Length", l_len_str);

        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_host_list(const host_list_t &a_host_list)
{
        m_host_list.clear();
        m_host_list = a_host_list;
        m_num_end_fetches = m_host_list.size();
        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_server_list(const server_list_t &a_server_list)
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
        m_num_end_fetches = m_host_list.size();
        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx_client::add_url(const std::string &a_url)
{
        host_t l_host;
        l_host.m_url = a_url;
        m_host_list.push_back(l_host);
        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx_client::add_url_file(const std::string &a_url_file)
{

        FILE * l_file;
        int32_t l_status = HLX_CLIENT_STATUS_OK;

        l_file = fopen (a_url_file.c_str(),"r");
        if (NULL == l_file)
        {
                NDBG_PRINT("Error opening file.  Reason: %s\n", strerror(errno));
                return HLX_CLIENT_STATUS_ERROR;
        }

        //NDBG_PRINT("ADD_FILE: ADDING: %s\n", a_url_file.c_str());
        //uint32_t l_num_added = 0;

        ssize_t l_file_line_size = 0;
        char *l_file_line = NULL;
        size_t l_unused;
        while((l_file_line_size = getline(&l_file_line,&l_unused,l_file)) != -1)
        {

                //NDBG_PRINT("LINE: %s", l_file_line);

                std::string l_line(l_file_line);

                if(!l_line.empty())
                {
                        //NDBG_PRINT("Add url: %s\n", l_line.c_str());

                        l_line.erase( std::remove_if( l_line.begin(), l_line.end(), ::isspace ), l_line.end() );
                        if(!l_line.empty())
                        {
                                l_status = add_url(l_line);
                                if(STATUS_OK != l_status)
                                {
                                        NDBG_PRINT("Error performing addurl for url: %s\n", l_line.c_str());

                                        if(l_file_line)
                                        {
                                                free(l_file_line);
                                                l_file_line = NULL;
                                        }
                                        return HLX_CLIENT_STATUS_ERROR;
                                }
                        }
                }

                if(l_file_line)
                {
                        free(l_file_line);
                        l_file_line = NULL;
                }

        }

        //NDBG_PRINT("ADD_FILE: DONE: %s -- last line len: %d\n", a_url_file.c_str(), (int)l_file_line_size);
        //if(l_file_line_size == -1)
        //{
        //        NDBG_PRINT("Error: getline errno: %d Reason: %s\n", errno, strerror(errno));
        //}


        l_status = fclose(l_file);
        if (0 != l_status)
        {
                NDBG_PRINT("Error closing file.  Reason: %s\n", strerror(errno));
                return HLX_CLIENT_STATUS_ERROR;
        }

        return HLX_CLIENT_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_split_requests_by_thread(bool a_val)
{
        m_split_requests_by_thread = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_connect_only(bool a_val)
{
        m_connect_only = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_use_ai_cache(bool a_val)
{
        m_use_ai_cache = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ai_cache(const std::string &a_ai_cache)
{
        m_ai_cache = a_ai_cache;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_timeout_s(uint32_t a_timeout_s)
{
        m_timeout_s = a_timeout_s;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_num_threads(uint32_t a_num_threads)
{
        m_num_threads = a_num_threads;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_num_parallel(uint32_t a_num_parallel)
{
        m_num_parallel = a_num_parallel;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_show_summary(bool a_val)
{
        m_show_summary = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_run_time_s(int32_t a_val)
{
        m_run_time_s = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_end_fetches(int32_t a_val)
{
        m_num_end_fetches = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_num_reqs_per_conn(int32_t a_val)
{
        m_num_reqs_per_conn = a_val;
        if((m_num_reqs_per_conn > 1) ||
           (m_num_reqs_per_conn < 0))
        {
                set_header("Connection", "keep-alive");
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_rate(int32_t a_val)
{
        m_rate = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_request_mode(request_mode_t a_mode)
{
        m_request_mode = a_mode;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_save_response(bool a_val)
{
        m_save_response = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_collect_stats(bool a_val)
{
        m_collect_stats = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_use_persistent_pool(bool a_val)
{
        m_use_persistent_pool = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_sock_opt_no_delay(bool a_val)
{
        m_sock_opt_no_delay = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_sock_opt_send_buf_size(uint32_t a_send_buf_size)
{
        m_sock_opt_send_buf_size = a_send_buf_size;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_sock_opt_recv_buf_size(uint32_t a_recv_buf_size)
{
        m_sock_opt_recv_buf_size = a_recv_buf_size;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_header(const std::string &a_header)
{
        int32_t l_status;
        std::string l_header_key;
        std::string l_header_val;
        l_status = break_header_string(a_header, l_header_key, l_header_val);
        if(l_status != 0)
        {
                // If verbose???
                //printf("Error header string[%s] is malformed\n", a_header.c_str());
                return HLX_CLIENT_STATUS_ERROR;
        }
        set_header(l_header_key, l_header_val);

        return HLX_CLIENT_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_header(const std::string &a_key, const std::string &a_val)
{
        m_header_map[a_key] = a_val;
        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::clear_headers(void)
{
        m_header_map.clear();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_verb(const std::string &a_verb)
{
        m_verb = a_verb;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_port(uint16_t a_port)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_scheme(scheme_type_t a_scheme)
{

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ssl_cipher_list(const std::string &a_cipher_list)
{
        m_ssl_cipher_list = a_cipher_list;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_ssl_options(const std::string &a_ssl_options_str)
{
        int32_t l_status;
        l_status = get_ssl_options_str_val(a_ssl_options_str, m_ssl_options);
        if(l_status != HLX_CLIENT_STATUS_OK)
        {
                return HLX_CLIENT_STATUS_ERROR;
        }
        return HLX_CLIENT_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_ssl_options(long a_ssl_options)
{
        m_ssl_options = a_ssl_options;
        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ssl_ca_path(const std::string &a_ssl_ca_path)
{
        m_ssl_ca_path = a_ssl_ca_path;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ssl_ca_file(const std::string &a_ssl_ca_file)
{
        m_ssl_ca_file = a_ssl_ca_file;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ssl_sni_verify(bool a_val)
{
        m_ssl_sni = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ssl_verify(bool a_val)
{
        m_ssl_verify = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: Constructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlx_client::hlx_client(void):

        // General
        m_verbose(false),
        m_color(false),
        m_quiet(false),

        // Run settings
        m_url(),
        m_url_file(),
        m_wildcarding(false),

        m_req_body(NULL),
        m_req_body_len(0),

        m_header_map(),

        // TODO Make define
        m_verb("GET"),

        m_port(0),
        m_scheme(SCHEME_NONE),

        m_use_ai_cache(true),
        m_ai_cache(),

        // TODO Make define
        m_num_parallel(128),

        // TODO Make define
        m_num_threads(4),

        // TODO Make define
        m_timeout_s(10),

        m_connect_only(false),
        m_show_summary(false),
        m_save_response(false),
        m_collect_stats(false),
        m_use_persistent_pool(false),

        m_rate(-1),
        m_num_end_fetches(-1),
        m_num_reqs_per_conn(1),
        m_run_time_s(-1),

        // TODO Make define
        m_request_mode(REQUEST_MODE_ROUND_ROBIN),
        m_split_requests_by_thread(true),

        m_start_time_ms(),
        m_last_display_time_ms(),
        m_last_stat(NULL),

        // Interval stats
        m_last_responses_count(),

        // Socket options
        m_sock_opt_recv_buf_size(0),
        m_sock_opt_send_buf_size(0),
        m_sock_opt_no_delay(false),

        // SSL
        m_ssl_ctx(NULL),
        m_ssl_cipher_list(),
        m_ssl_options_str(),
        m_ssl_options(0),
        m_ssl_verify(false),
        m_ssl_sni(false),
        m_ssl_ca_file(),
        m_ssl_ca_path(),

        // t_client
        m_t_client_list(),

        // TODO Make define
        m_evr_loop_type(EVR_LOOP_EPOLL),

        // TODO REMOVE!!!
        //m_reqlet_vector(),

        m_host_list(),

        m_resolver(NULL),

        m_is_initd(false)
{
        m_last_stat = new total_stat_agg_struct();

        for(uint32_t i = 0; i < 10; ++i) {m_last_responses_count[i] = 0;}

};

//: ----------------------------------------------------------------------------
//: \details: Constructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlx_client::~hlx_client(void)
{

        // TODO REMOVE!!!
#if 0
        // Delete reqlets
        for(size_t i_reqlet = 0;
            i_reqlet < m_reqlet_vector.size();
            ++i_reqlet)
        {
                reqlet *i_reqlet_ptr = m_reqlet_vector[i_reqlet];
                if(i_reqlet_ptr)
                {
                        delete i_reqlet_ptr;
                        i_reqlet_ptr = NULL;
                }
                m_reqlet_vector.clear();
        }
#endif

        // Delete t_client list...
        for(t_client_list_t::iterator i_client_hle = m_t_client_list.begin();
                        i_client_hle != m_t_client_list.end(); )
        {
                t_client *l_t_client_ptr = *i_client_hle;
                if(l_t_client_ptr)
                {
                        delete l_t_client_ptr;
                        m_t_client_list.erase(i_client_hle++);
                        l_t_client_ptr = NULL;
                }
        }

        // SSL Cleanup
        ssl_kill_locks();

        // TODO Deprecated???
        //EVP_cleanup();

        if(m_last_stat)
        {
                delete m_last_stat;
                m_last_stat = NULL;
        }

        if(m_req_body)
        {
                free(m_req_body);
                m_req_body = NULL;
                m_req_body_len = 0;
        }

        delete m_resolver;
        m_resolver = NULL;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define ARESP(_str) l_responses_str += _str
std::string hlx_client::dump_all_responses(bool a_color, bool a_pretty, output_type_t a_output_type, int a_part_map)
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
std::string hlx_client::dump_all_responses_line_dl(bool a_color,
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

        for(t_client_list_t::const_iterator i_client = m_t_client_list.begin();
           i_client != m_t_client_list.end();)
        {
                const reqlet_vector_t &l_reqlet_vector = (*i_client)->get_reqlet_vector();

                int l_cur_reqlet = 0;
                for(reqlet_vector_t::const_iterator i_reqlet = l_reqlet_vector.begin();
                    i_reqlet != l_reqlet_vector.end();
                    ++i_reqlet, ++l_cur_reqlet)
                {


                        bool l_fbf = false;
                        // Host
                        if(a_part_map & PART_HOST)
                        {
                                sprintf(l_buf, "\"%shost%s\": \"%s\"",
                                                l_host_color.c_str(), l_off_color.c_str(),
                                                (*i_reqlet)->m_url.m_host.c_str());
                                ARESP(l_buf);
                                l_fbf = true;
                        }

                        // Server
                        if(a_part_map & PART_SERVER)
                        {

                                if(l_fbf) {ARESP(", "); l_fbf = false;}
                                sprintf(l_buf, "\"%sserver%s\": \"%s:%d\"",
                                                l_server_color.c_str(), l_server_color.c_str(),
                                                (*i_reqlet)->m_url.m_host.c_str(),
                                                (*i_reqlet)->m_url.m_port
                                                );
                                ARESP(l_buf);
                                l_fbf = true;

                                if(!(*i_reqlet)->m_url.m_id.empty())
                                {
                                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                                        sprintf(l_buf, "\"%sid%s\": \"%s\"",
                                                        l_id_color.c_str(), l_id_color.c_str(),
                                                        (*i_reqlet)->m_url.m_id.c_str()
                                                        );
                                        ARESP(l_buf);
                                        l_fbf = true;
                                }

                                if(!(*i_reqlet)->m_url.m_where.empty())
                                {
                                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                                        sprintf(l_buf, "\"%swhere%s\": \"%s\"",
                                                        l_id_color.c_str(), l_id_color.c_str(),
                                                        (*i_reqlet)->m_url.m_where.c_str()
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
                                        if((*i_reqlet)->m_response_status == 200) l_status_val_color = ANSI_COLOR_FG_GREEN;
                                        else l_status_val_color = ANSI_COLOR_FG_RED;
                                }
                                sprintf(l_buf, "\"%sstatus-code%s\": %s%d%s",
                                                l_status_color.c_str(), l_off_color.c_str(),
                                                l_status_val_color, (*i_reqlet)->m_response_status, l_off_color.c_str());
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
                                //NDBG_PRINT("RESPONSE SIZE: %ld\n", (*i_reqlet)->m_response_body.length());
                                if(!(*i_reqlet)->m_response_body.empty())
                                {
                                        sprintf(l_buf, "\"%sbody%s\": %s",
                                                        l_body_color.c_str(), l_off_color.c_str(),
                                                        (*i_reqlet)->m_response_body.c_str());
                                }
                                else
                                {
                                        sprintf(l_buf, "\"%sbody%s\": \"NO_RESPONSE\"",
                                                        l_body_color.c_str(), l_off_color.c_str());
                                }
                                ARESP(l_buf);
                                l_fbf = true;
                        }
                        ARESP("\n");
                }
                ++i_client;
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
                l_js_allocator)\

std::string hlx_client::dump_all_responses_json(int a_part_map)
{
        rapidjson::Document l_js_doc;
        l_js_doc.SetObject();
        rapidjson::Value l_js_array(rapidjson::kArrayType);
        rapidjson::Document::AllocatorType& l_js_allocator = l_js_doc.GetAllocator();

        for(t_client_list_t::const_iterator i_client = m_t_client_list.begin();
           i_client != m_t_client_list.end();
           ++i_client)
        {
                const reqlet_vector_t &l_reqlet_vector = (*i_client)->get_reqlet_vector();
                for(reqlet_vector_t::const_iterator i_reqlet = l_reqlet_vector.begin();
                    i_reqlet != l_reqlet_vector.end();
                    ++i_reqlet)
                {
                        rapidjson::Value l_obj;
                        l_obj.SetObject();
                        bool l_content_type_json = false;

                        // Search for json
                        header_map_t::const_iterator i_h = (*i_reqlet)->m_response_headers.find("Content-type");
                        if(i_h != (*i_reqlet)->m_response_headers.end() && i_h->second == "application/json")
                        {
                                l_content_type_json = true;
                        }

                        // Host
                        if(a_part_map & PART_HOST)
                        {
                                JS_ADD_MEMBER("host", (*i_reqlet)->m_url.m_host.c_str());
                        }

                        // Server
                        if(a_part_map & PART_SERVER)
                        {
                                char l_server_buf[1024];
                                sprintf(l_server_buf, "%s:%d",
                                                (*i_reqlet)->m_url.m_host.c_str(),
                                                (*i_reqlet)->m_url.m_port);
                                JS_ADD_MEMBER("server", l_server_buf);

                                if(!(*i_reqlet)->m_url.m_id.empty())
                                {
                                        JS_ADD_MEMBER("id", (*i_reqlet)->m_url.m_id.c_str());
                                }

                                if(!(*i_reqlet)->m_url.m_where.empty())
                                {
                                        JS_ADD_MEMBER("where", (*i_reqlet)->m_url.m_where.c_str());
                                }
                        }

                        // Status Code
                        if(a_part_map & PART_STATUS_CODE)
                        {
                                l_obj.AddMember("status-code", (*i_reqlet)->m_response_status, l_js_allocator);
                        }

                        // Headers
                        if(a_part_map & PART_HEADERS)
                        {
                                for(header_map_t::iterator i_header = (*i_reqlet)->m_response_headers.begin();
                                                i_header != (*i_reqlet)->m_response_headers.end();
                                    ++i_header)
                                {
                                        l_obj.AddMember(rapidjson::Value(i_header->first.c_str(), l_js_allocator).Move(),
                                                        rapidjson::Value(i_header->second.c_str(), l_js_allocator).Move(),
                                                        l_js_allocator);
                                }
                        }

                        // Connection info
                        //if(a_part_map & PART_HEADERS)
                        for(header_map_t::iterator i_header = (*i_reqlet)->m_conn_info.begin();
                                        i_header != (*i_reqlet)->m_conn_info.end();
                            ++i_header)
                        {
                                l_obj.AddMember(rapidjson::Value(i_header->first.c_str(), l_js_allocator).Move(),
                                                rapidjson::Value(i_header->second.c_str(), l_js_allocator).Move(),
                                                l_js_allocator);
                        }

                        // Body
                        if(a_part_map & PART_BODY)
                        {

                                //NDBG_PRINT("RESPONSE SIZE: %ld\n", (*i_reqlet)->m_response_body.length());
                                if(!(*i_reqlet)->m_response_body.empty())
                                {
                                        // Append json
                                        if(l_content_type_json)
                                        {
                                                rapidjson::Document l_doc_body;
                                                l_doc_body.Parse((*i_reqlet)->m_response_body.c_str());
                                                l_obj.AddMember("body",
                                                                rapidjson::Value(l_doc_body, l_js_allocator).Move(),
                                                                l_js_allocator);

                                        }
                                        else
                                        {
                                                JS_ADD_MEMBER("body", (*i_reqlet)->m_response_body.c_str());
                                        }
                                }
                                else
                                {
                                        JS_ADD_MEMBER("body", "NO_RESPONSE");
                                }
                        }

                        l_js_array.PushBack(l_obj, l_js_allocator);

                }
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


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  client status indicating success or failure
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::init(void)
{

        if(true == m_is_initd)
                return HLX_CLIENT_STATUS_OK;
        // not initialized yet

        m_resolver = new resolver();

        // -------------------------------------------
        // Init resolver with cache
        // -------------------------------------------
        int32_t l_ldb_init_status;
        l_ldb_init_status = m_resolver->init(m_ai_cache, m_use_ai_cache);
        if(STATUS_OK != l_ldb_init_status)
        {
                return HLX_CLIENT_STATUS_ERROR;
        }

        // -------------------------------------------
        // SSL init...
        // -------------------------------------------
        m_ssl_ctx = ssl_init(m_ssl_cipher_list, // ctx cipher list str
                             m_ssl_options,     // ctx options
                             m_ssl_ca_file,     // ctx ca file
                             m_ssl_ca_path);    // ctx ca path
        if(NULL == m_ssl_ctx)
        {
                NDBG_PRINT("Error: performing ssl_init with cipher_list: %s\n", m_ssl_cipher_list.c_str());
                return HLX_CLIENT_STATUS_ERROR;
        }

        m_is_initd = true;
        return HLX_CLIENT_STATUS_OK;

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void show_total_agg_stat(std::string &a_tag,
        const total_stat_agg_t &a_stat,
        double a_time_elapsed_s,
        uint32_t a_max_parallel,
        bool a_color)
{
        if(a_color)
        printf("| %sRESULTS%s:             %s%s%s\n", ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF, ANSI_COLOR_FG_YELLOW, a_tag.c_str(), ANSI_COLOR_OFF);
        else
        printf("| RESULTS:             %s\n", a_tag.c_str());

        printf("| fetches:             %lu\n", a_stat.m_total_reqs);
        printf("| max parallel:        %u\n", a_max_parallel);
        printf("| bytes:               %e\n", (double)a_stat.m_total_bytes);
        printf("| seconds:             %f\n", a_time_elapsed_s);
        printf("| mean bytes/conn:     %f\n", ((double)a_stat.m_total_bytes)/((double)a_stat.m_total_reqs));
        printf("| fetches/sec:         %f\n", ((double)a_stat.m_total_reqs)/(a_time_elapsed_s));
        printf("| bytes/sec:           %e\n", ((double)a_stat.m_total_bytes)/a_time_elapsed_s);

        // TODO Fix stdev/var calcs
#if 0
#define SHOW_XSTAT_LINE(_tag, stat)\
        do {\
        printf("| %-16s %12.6f mean, %12.6f max, %12.6f min, %12.6f stdev, %12.6f var\n",\
               _tag,                                                    \
               stat.mean()/1000.0,                                      \
               stat.max()/1000.0,                                       \
               stat.min()/1000.0,                                       \
               stat.stdev()/1000.0,                                     \
               stat.var()/1000.0);                                      \
        } while(0)
#else
#define SHOW_XSTAT_LINE(_tag, stat)\
        do {\
        printf("| %-16s %12.6f mean, %12.6f max, %12.6f min\n",\
               _tag,\
               stat.mean()/1000.0,\
               stat.max()/1000.0,\
               stat.min()/1000.0);\
        } while(0)
#endif

        SHOW_XSTAT_LINE("ms/connect:", a_stat.m_stat_us_connect);
        SHOW_XSTAT_LINE("ms/1st-response:", a_stat.m_stat_us_first_response);
        //SHOW_XSTAT_LINE("ms/download:", a_stat.m_stat_us_download);
        SHOW_XSTAT_LINE("ms/end2end:", a_stat.m_stat_us_end_to_end);

        if(a_color)
                printf("| %sHTTP response codes%s: \n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF);
        else
                printf("| HTTP response codes: \n");

        for(status_code_count_map_t::const_iterator i_status_code = a_stat.m_status_code_count_map.begin();
                        i_status_code != a_stat.m_status_code_count_map.end();
                ++i_status_code)
        {
                if(a_color)
                printf("| %s%3d%s -- %u\n", ANSI_COLOR_FG_MAGENTA, i_status_code->first, ANSI_COLOR_OFF, i_status_code->second);
                else
                printf("| %3d -- %u\n", i_status_code->first, i_status_code->second);
        }

        // Done flush...
        printf("\n");

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::display_results(double a_elapsed_time,
                                 bool a_show_breakdown_flag)
{

        tag_stat_map_t l_tag_stat_map;
        total_stat_agg_t l_total;

        // Get stats
        get_stats(l_total, a_show_breakdown_flag, l_tag_stat_map);

        std::string l_tag;
        // TODO Fix elapse and max parallel
        l_tag = "ALL";
        show_total_agg_stat(l_tag, l_total, a_elapsed_time, m_num_parallel, m_color);

        // -------------------------------------------
        // Optional Breakdown
        // -------------------------------------------
        if(a_show_breakdown_flag)
        {
                for(tag_stat_map_t::iterator i_stat = l_tag_stat_map.begin();
                                i_stat != l_tag_stat_map.end();
                                ++i_stat)
                {
                        l_tag = i_stat->first;
                        show_total_agg_stat(l_tag, i_stat->second, a_elapsed_time, m_num_parallel, m_color);
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void show_total_agg_stat_legacy(std::string &a_tag,
                                       const total_stat_agg_t &a_stat,
                                       std::string &a_sep,
                                       double a_time_elapsed_s,
                                       uint32_t a_max_parallel)
{
        printf("%s: ", a_tag.c_str());
        printf("%lu fetches, ", a_stat.m_total_reqs);
        printf("%u max parallel, ", a_max_parallel);
        printf("%e bytes, ", (double)a_stat.m_total_bytes);
        printf("in %f seconds, ", a_time_elapsed_s);
        printf("%s", a_sep.c_str());

        printf("%f mean bytes/connection, ", ((double)a_stat.m_total_bytes)/((double)a_stat.m_total_reqs));
        printf("%s", a_sep.c_str());

        printf("%f fetches/sec, %e bytes/sec", ((double)a_stat.m_total_reqs)/(a_time_elapsed_s), ((double)a_stat.m_total_bytes)/a_time_elapsed_s);
        printf("%s", a_sep.c_str());

#define SHOW_XSTAT_LINE_LEGACY(_tag, stat)\
        printf("%s %.6f mean, %.6f max, %.6f min, %.6f stdev",\
               _tag,                                          \
               stat.mean()/1000.0,                            \
               stat.max()/1000.0,                             \
               stat.min()/1000.0,                             \
               stat.stdev()/1000.0);                          \
        printf("%s", a_sep.c_str())

        SHOW_XSTAT_LINE_LEGACY("msecs/connect:", a_stat.m_stat_us_connect);
        SHOW_XSTAT_LINE_LEGACY("msecs/first-response:", a_stat.m_stat_us_first_response);
        //SHOW_XSTAT_LINE_LEGACY("msecs/download:", a_stat.m_stat_us_download);
        SHOW_XSTAT_LINE_LEGACY("msecs/end2end:", a_stat.m_stat_us_end_to_end);

        printf("HTTP response codes: ");
        if(a_sep == "\n")
                printf("%s", a_sep.c_str());

        for(status_code_count_map_t::const_iterator i_status_code = a_stat.m_status_code_count_map.begin();
                        i_status_code != a_stat.m_status_code_count_map.end();
                ++i_status_code)
        {
                printf("code %d -- %u, ", i_status_code->first, i_status_code->second);
        }

        // Done flush...
        printf("\n");

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::get_stats(total_stat_agg_t &ao_all_stats,
                           bool a_get_breakdown,
                           tag_stat_map_t &ao_breakdown_stats)
{
        // -------------------------------------------
        // Aggregate
        // -------------------------------------------
        tag_stat_map_t l_copy;
        for(t_client_list_t::const_iterator i_client = m_t_client_list.begin();
           i_client != m_t_client_list.end();
           ++i_client)
        {
                const reqlet_vector_t &l_reqlet_vector = (*i_client)->get_reqlet_vector();
                for(reqlet_vector_t::const_iterator i_reqlet = l_reqlet_vector.begin(); i_reqlet != l_reqlet_vector.end(); ++i_reqlet)
                {
                        std::string l_label = (*i_reqlet)->get_label();
                        tag_stat_map_t::iterator i_copy = l_copy.find(l_label);
                        if(i_copy != l_copy.end())
                        {
                                add_to_total_stat_agg(i_copy->second, (*i_reqlet)->m_stat_agg);
                        }
                        else
                        {
                                l_copy[(*i_reqlet)->get_label()] = (*i_reqlet)->m_stat_agg;
                        }
                }
        }

        for(tag_stat_map_t::iterator i_reqlet = l_copy.begin(); i_reqlet != l_copy.end(); ++i_reqlet)
        {
                if(a_get_breakdown)
                {
                        std::string l_tag = i_reqlet->first;
                        tag_stat_map_t::iterator i_stat;
                        if((i_stat = ao_breakdown_stats.find(l_tag)) == ao_breakdown_stats.end())
                        {
                                ao_breakdown_stats[l_tag] = i_reqlet->second;
                        }
                        else
                        {
                                // Add to existing
                                add_to_total_stat_agg(i_stat->second, i_reqlet->second);
                        }
                }

                // Add to total
                add_to_total_stat_agg(ao_all_stats, i_reqlet->second);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::display_results_http_load_style(double a_elapsed_time,
                                                 bool a_show_breakdown_flag,
                                                 bool a_one_line_flag)
{

        tag_stat_map_t l_tag_stat_map;
        total_stat_agg_t l_total;

        // Get stats
        get_stats(l_total, a_show_breakdown_flag, l_tag_stat_map);

        std::string l_tag;
        // Separator
        std::string l_sep = "\n";
        if(a_one_line_flag) l_sep = "||";

        // TODO Fix elapse and max parallel
        l_tag = "State";
        show_total_agg_stat_legacy(l_tag, l_total, l_sep, a_elapsed_time, m_num_parallel);

        // -------------------------------------------
        // Optional Breakdown
        // -------------------------------------------
        if(a_show_breakdown_flag)
        {
                for(tag_stat_map_t::iterator i_stat = l_tag_stat_map.begin();
                                i_stat != l_tag_stat_map.end();
                                ++i_stat)
                {
                        l_tag = "[";
                        l_tag += i_stat->first;
                        l_tag += "]";
                        show_total_agg_stat_legacy(l_tag, i_stat->second, l_sep, a_elapsed_time, m_num_parallel);
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx_client::get_stats_json(char *l_json_buf, uint32_t l_json_buf_max_len)
{

        tag_stat_map_t l_tag_stat_map;
        total_stat_agg_t l_total;

        uint64_t l_time_ms = get_time_ms();
        // Get stats
        get_stats(l_total, true, l_tag_stat_map);

        int l_cur_offset = 0;
        l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,"{\"data\": [");
        bool l_first_stat = true;
        for(tag_stat_map_t::iterator i_agg_stat = l_tag_stat_map.begin();
                        i_agg_stat != l_tag_stat_map.end();
                        ++i_agg_stat)
        {

                if(l_first_stat) l_first_stat = false;
                else
                        l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,",");

                l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,
                                "{\"key\": \"%s\", \"value\": ",
                                i_agg_stat->first.c_str());

                l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,
                                "{\"%s\": %" PRIu64 ", \"%s\": %" PRIu64 "}",
                                "count", (uint64_t)(i_agg_stat->second.m_total_reqs),
                                "time", (uint64_t)(l_time_ms));

                l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,"}");
        }

        l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,"]}");


        return l_cur_offset;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::display_results_line_desc(void)
{
        printf("+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+\n");
        if(m_color)
        {
        printf("| %s%9s%s / %s%9s%s | %s%9s%s | %s%9s%s | %s%12s%s | %9s | %11s | %9s |\n",
                        ANSI_COLOR_FG_GREEN, "Cmpltd", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_BLUE, "Total", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_MAGENTA, "IdlKil", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_RED, "Errors", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_YELLOW, "kBytes Recvd", ANSI_COLOR_OFF,
                        "Elapsed",
                        "Req/s",
                        "MB/s");
        }
        else
        {
                printf("| %9s / %9s | %9s | %9s | %12s | %9s | %11s | %9s |\n",
                                "Cmpltd",
                                "Total",
                                "IdlKil",
                                "Errors",
                                "kBytes Recvd",
                                "Elapsed",
                                "Req/s",
                                "MB/s");
        }
        printf("+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+\n");
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::display_results_line(void)
{

        total_stat_agg_t l_total;
        tag_stat_map_t l_unused;
        uint64_t l_cur_time_ms = get_time_ms();

        // Get stats
        get_stats(l_total, false, l_unused);

        double l_reqs_per_s = ((double)(l_total.m_total_reqs - m_last_stat->m_total_reqs)*1000.0) /
                        ((double)(l_cur_time_ms - m_last_display_time_ms));
        double l_kb_per_s = ((double)(l_total.m_num_bytes_read - m_last_stat->m_num_bytes_read)*1000.0/1024) /
                        ((double)(l_cur_time_ms - m_last_display_time_ms));
        m_last_display_time_ms = get_time_ms();
        *m_last_stat = l_total;

        if(m_color)
        {
                        printf("| %s%9" PRIu64 "%s / %s%9" PRIi64 "%s | %s%9" PRIu64 "%s | %s%9" PRIu64 "%s | %s%12.2f%s | %8.2fs | %10.2fs | %8.2fs |\n",
                                        ANSI_COLOR_FG_GREEN, l_total.m_total_reqs, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_BLUE, l_total.m_total_reqs, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_MAGENTA, l_total.m_num_idle_killed, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_RED, l_total.m_num_errors, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_YELLOW, ((double)(l_total.m_num_bytes_read))/(1024.0), ANSI_COLOR_OFF,
                                        ((double)(get_delta_time_ms(m_start_time_ms))) / 1000.0,
                                        l_reqs_per_s,
                                        l_kb_per_s/1024.0
                                        );
        }
        else
        {
                printf("| %9" PRIu64 " / %9" PRIi64 " | %9" PRIu64 " | %9" PRIu64 " | %12.2f | %8.2fs | %10.2fs | %8.2fs |\n",
                                l_total.m_total_reqs,
                                l_total.m_total_reqs,
                                l_total.m_num_idle_killed,
                                l_total.m_num_errors,
                                ((double)(l_total.m_num_bytes_read))/(1024.0),
                                ((double)(get_delta_time_ms(m_start_time_ms)) / 1000.0),
                                l_reqs_per_s,
                                l_kb_per_s/1024.0
                                );

        }

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::display_responses_line_desc(bool a_show_per_interval)
{
        printf("+-----------+-------------+-----------+-----------+-----------+-----------+-----------+-----------+\n");
        if(a_show_per_interval)
        {
                if(m_color)
                {
                printf("| %s%9s%s / %s%11s%s / %s%9s%s / %s%9s%s / %s%9s%s | %s%9s%s | %s%9s%s | %s%9s%s | \n",
                                ANSI_COLOR_FG_WHITE, "Elapsed", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_WHITE, "Req/s", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_WHITE, "Cmpltd", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_WHITE, "Errors", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_GREEN, "200s %%", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_CYAN, "300s %%", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_MAGENTA, "400s %%", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_RED, "500s %%", ANSI_COLOR_OFF);
                }
                else
                {
                        printf("| %9s / %11s / %9s / %9s | %9s | %9s | %9s | %9s | \n",
                                        "Elapsed",
                                        "Req/s",
                                        "Cmpltd",
                                        "Errors",
                                        "200s %%",
                                        "300s %%",
                                        "400s %%",
                                        "500s %%");
                }
        }
        else
        {
                if(m_color)
                {
                printf("| %s%9s%s / %s%11s%s / %s%9s%s / %s%9s%s / %s%9s%s | %s%9s%s | %s%9s%s | %s%9s%s | \n",
                                ANSI_COLOR_FG_WHITE, "Elapsed", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_WHITE, "Req/s", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_WHITE, "Cmpltd", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_WHITE, "Errors", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_GREEN, "200s", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_CYAN, "300s", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_MAGENTA, "400s", ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_RED, "500s", ANSI_COLOR_OFF);
                }
                else
                {
                        printf("| %9s / %11s / %9s / %9s | %9s | %9s | %9s | %9s | \n",
                                        "Elapsed",
                                        "Req/s",
                                        "Cmpltd",
                                        "Errors",
                                        "200s",
                                        "300s",
                                        "400s",
                                        "500s");
                }
        }
        printf("+-----------+-------------+-----------+-----------+-----------+-----------+-----------+-----------+\n");
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::display_responses_line(bool a_show_per_interval)
{

        total_stat_agg_t l_total;
        tag_stat_map_t l_unused;
        uint64_t l_cur_time_ms = get_time_ms();

        // Get stats
        get_stats(l_total, false, l_unused);

        double l_reqs_per_s = ((double)(l_total.m_total_reqs - m_last_stat->m_total_reqs)*1000.0) /
                              ((double)(l_cur_time_ms - m_last_display_time_ms));
        m_last_display_time_ms = get_time_ms();
        *m_last_stat = l_total;

        // Aggregate over status code map
        status_code_count_map_t m_status_code_count_map;

        uint32_t l_responses[10] = {0};
        for(status_code_count_map_t::iterator i_code = l_total.m_status_code_count_map.begin();
            i_code != l_total.m_status_code_count_map.end();
            ++i_code)
        {
                if(i_code->first >= 200 && i_code->first <= 299)
                {
                        l_responses[2] += i_code->second;
                }
                else if(i_code->first >= 300 && i_code->first <= 399)
                {
                        l_responses[3] += i_code->second;
                }
                else if(i_code->first >= 400 && i_code->first <= 499)
                {
                        l_responses[4] += i_code->second;
                }
                else if(i_code->first >= 500 && i_code->first <= 599)
                {
                        l_responses[5] += i_code->second;
                }
        }

        if(a_show_per_interval)
        {

                // Calculate rates
                double l_rate[10] = {0.0};
                uint32_t l_totals = 0;

                for(uint32_t i = 2; i <= 5; ++i)
                {
                        l_totals += l_responses[i] - m_last_responses_count[i];
                }

                for(uint32_t i = 2; i <= 5; ++i)
                {
                        if(l_totals)
                        {
                                l_rate[i] = (100.0*((double)(l_responses[i] - m_last_responses_count[i]))) / ((double)(l_totals));
                        }
                        else
                        {
                                l_rate[i] = 0.0;
                        }
                }

                if(m_color)
                {
                                printf("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %s%9.2f%s | %s%9.2f%s | %s%9.2f%s | %s%9.2f%s |\n",
                                                ((double)(get_delta_time_ms(m_start_time_ms))) / 1000.0,
                                                l_reqs_per_s,
                                                l_total.m_total_reqs,
                                                l_total.m_num_errors,
                                                ANSI_COLOR_FG_GREEN, l_rate[2], ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_CYAN, l_rate[3], ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_MAGENTA, l_rate[4], ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_RED, l_rate[5], ANSI_COLOR_OFF);
                }
                else
                {
                        printf("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %9.2f | %9.2f | %9.2f | %9.2f |\n",
                                        ((double)(get_delta_time_ms(m_start_time_ms))) / 1000.0,
                                        l_reqs_per_s,
                                        l_total.m_total_reqs,
                                        l_total.m_num_errors,
                                        l_rate[2],
                                        l_rate[3],
                                        l_rate[4],
                                        l_rate[5]);
                }

                // Update last
                m_last_responses_count[2] = l_responses[2];
                m_last_responses_count[3] = l_responses[3];
                m_last_responses_count[4] = l_responses[4];
                m_last_responses_count[5] = l_responses[5];
        }
        else
        {
                if(m_color)
                {
                                printf("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %s%9u%s | %s%9u%s | %s%9u%s | %s%9u%s |\n",
                                                ((double)(get_delta_time_ms(m_start_time_ms))) / 1000.0,
                                                l_reqs_per_s,
                                                l_total.m_total_reqs,
                                                l_total.m_num_errors,
                                                ANSI_COLOR_FG_GREEN, l_responses[2], ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_CYAN, l_responses[3], ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_MAGENTA, l_responses[4], ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_RED, l_responses[5], ANSI_COLOR_OFF);
                }
                else
                {
                        printf("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %9u | %9u | %9u | %9u |\n",
                                        ((double)(get_delta_time_ms(m_start_time_ms))) / 1000.0,
                                        l_reqs_per_s,
                                        l_total.m_total_reqs,
                                        l_total.m_num_errors,
                                        l_responses[2],
                                        l_responses[3],
                                        l_responses[4],
                                        l_responses[5]);
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void append_to_map(summary_map_t &ao_sum, const summary_map_t &a_append)
{
        for(summary_map_t::const_iterator i_sum = a_append.begin();
            i_sum != a_append.end();
           ++i_sum)
        {
                summary_map_t::iterator i_obj = ao_sum.find(i_sum->first);
                if(i_obj != ao_sum.end())
                {
                        i_obj->second += i_sum->second;
                } else {
                        ao_sum[i_sum->first] = i_sum->second;
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::display_summary(bool a_color)
{
        std::string l_header_str = "";
        std::string l_protocol_str = "";
        std::string l_cipher_str = "";
        std::string l_off_color = "";

        if(a_color)
        {
                l_header_str = ANSI_COLOR_FG_CYAN;
                l_protocol_str = ANSI_COLOR_FG_GREEN;
                l_cipher_str = ANSI_COLOR_FG_YELLOW;
                l_off_color = ANSI_COLOR_OFF;
        }

        // -------------------------------------------------
        // Get results from clients
        // -------------------------------------------------
        uint32_t l_num_reqlets = 0;
        uint32_t l_summary_success = 0;
        uint32_t l_summary_error_addr = 0;
        uint32_t l_summary_error_conn = 0;
        uint32_t l_summary_error_unknown = 0;
        uint32_t l_summary_ssl_error_self_signed = 0;
        uint32_t l_summary_ssl_error_expired = 0;
        uint32_t l_summary_ssl_error_other = 0;
        summary_map_t l_summary_ssl_protocols;
        summary_map_t l_summary_ssl_ciphers;

        for(t_client_list_t::const_iterator i_client = m_t_client_list.begin();
           i_client != m_t_client_list.end();
           ++i_client)
        {
                l_num_reqlets += (*i_client)->m_num_reqlets;
                l_summary_success += (*i_client)->m_summary_success;
                l_summary_error_addr += (*i_client)->m_summary_error_addr;
                l_summary_error_conn += (*i_client)->m_summary_error_conn;
                l_summary_error_unknown += (*i_client)->m_summary_error_unknown;
                l_summary_ssl_error_self_signed += (*i_client)->m_summary_ssl_error_self_signed;
                l_summary_ssl_error_expired += (*i_client)->m_summary_ssl_error_expired;
                l_summary_ssl_error_other += (*i_client)->m_summary_ssl_error_other;
                append_to_map(l_summary_ssl_protocols, (*i_client)->m_summary_ssl_protocols);
                append_to_map(l_summary_ssl_ciphers, (*i_client)->m_summary_ssl_ciphers);
        }


        NDBG_OUTPUT("****************** %sSUMMARY%s ****************** \n", l_header_str.c_str(), l_off_color.c_str());
        NDBG_OUTPUT("| total hosts:                     %u\n",l_num_reqlets);
        NDBG_OUTPUT("| success:                         %u\n",l_summary_success);
        NDBG_OUTPUT("| error address lookup:            %u\n",l_summary_error_addr);
        NDBG_OUTPUT("| error connectivity:              %u\n",l_summary_error_conn);
        NDBG_OUTPUT("| error unknown:                   %u\n",l_summary_error_unknown);
        NDBG_OUTPUT("| ssl error cert expired           %u\n",l_summary_ssl_error_expired);
        NDBG_OUTPUT("| ssl error cert self-signed       %u\n",l_summary_ssl_error_self_signed);
        NDBG_OUTPUT("| ssl error other                  %u\n",l_summary_ssl_error_other);

        // Sort
        typedef std::map<uint32_t, std::string> _sorted_map_t;
        _sorted_map_t l_sorted_map;
        NDBG_OUTPUT("+--------------- %sSSL PROTOCOLS%s -------------- \n", l_protocol_str.c_str(), l_off_color.c_str());
        l_sorted_map.clear();
        for(summary_map_t::iterator i_s = l_summary_ssl_protocols.begin(); i_s != l_summary_ssl_protocols.end(); ++i_s)
        l_sorted_map[i_s->second] = i_s->first;
        for(_sorted_map_t::reverse_iterator i_s = l_sorted_map.rbegin(); i_s != l_sorted_map.rend(); ++i_s)
        NDBG_OUTPUT("| %-32s %u\n", i_s->second.c_str(), i_s->first);
        NDBG_OUTPUT("+--------------- %sSSL CIPHERS%s ---------------- \n", l_cipher_str.c_str(), l_off_color.c_str());
        l_sorted_map.clear();
        for(summary_map_t::iterator i_s = l_summary_ssl_ciphers.begin(); i_s != l_summary_ssl_ciphers.end(); ++i_s)
        l_sorted_map[i_s->second] = i_s->first;
        for(_sorted_map_t::reverse_iterator i_s = l_sorted_map.rbegin(); i_s != l_sorted_map.rend(); ++i_s)
        NDBG_OUTPUT("| %-32s %u\n", i_s->second.c_str(), i_s->first);

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::display_status_line(bool a_color)
{

        // -------------------------------------------------
        // Get results from clients
        // -------------------------------------------------
        uint32_t l_num_reqlets = 0;
        uint32_t l_num_get = 0;
        uint32_t l_num_done = 0;
        uint32_t l_num_resolved = 0;
        uint32_t l_num_error = 0;

        for(t_client_list_t::const_iterator i_client = m_t_client_list.begin();
           i_client != m_t_client_list.end();
           ++i_client)
        {
                l_num_reqlets += (*i_client)->m_num_reqlets;
                l_num_get += (*i_client)->m_num_get;
                l_num_done += (*i_client)->m_num_done;
                l_num_resolved += (*i_client)->m_num_resolved;
                l_num_error += (*i_client)->m_num_error;
        }

        if(a_color)
        {
                printf("Done/Resolved/Req'd/Total/Error %s%8u%s / %s%8u%s / %s%8u%s / %s%8u%s / %s%8u%s\n",
                                ANSI_COLOR_FG_GREEN, l_num_done, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_MAGENTA, l_num_resolved, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_YELLOW, l_num_get, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_BLUE, l_num_reqlets, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_RED, l_num_error, ANSI_COLOR_OFF);
        }
        else
        {
                printf("Done/Resolved/Req'd/Total/Error %8u / %8u / %8u / %8u / %8u\n",
                                l_num_done, l_num_resolved, l_num_get, l_num_reqlets, l_num_error);
        }
}

} //namespace ns_hlx {
