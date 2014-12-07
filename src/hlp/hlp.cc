//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlp.cc
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
#include "hlp.h"
#include "stats_collector.h"
#include "t_client.h"
#include "util.h"

#include "city.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>

#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlp::run(const std::string &a_playback_file,
                const std::string &a_pb_dest_addr,
                int32_t a_pb_dest_port)

{
        int32_t l_retval = STATUS_OK;

        stats_collector::get()->set_start_time_ms(get_time_ms());

        m_playback_file = a_playback_file;
        m_pb_dest_addr = a_pb_dest_addr;
        m_pb_dest_port = a_pb_dest_port;

        // -------------------------------------------
        // Create t_client list...
        // -------------------------------------------
        for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
        {
                // Create/Run playback client
                t_client *l_t_client = new t_client(m_verbose,
                        m_color,
                        m_sock_opt_recv_buf_size,
                        m_sock_opt_send_buf_size,
                        m_sock_opt_no_delay,
                        m_cipher_list,
                        m_ssl_ctx,
                        m_evr_type,
                        // TODO How to configure max connections???
                        2048,
                        m_timeout_s);

                l_t_client->set_scale(m_scale);

                m_t_client_list.push_back(l_t_client);

        }

        // Start feeder thread
        int32_t l_pthread_error = 0;

        l_pthread_error = pthread_create(&m_t_feed_playback_thread,
                        NULL,
                        t_feed_playback_static,
                        this);
        if (l_pthread_error != 0)
        {
                // failed to create thread

                NDBG_PRINT("Error: creating thread.  Reason: %s\n.", strerror(l_pthread_error));
                return STATUS_ERROR;

        }

        for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
        {
                m_t_client_list[i_client_idx]->run();
        }

        return l_retval;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define HLP_PLAYBACK_MAX_BYTES_PER_LINE 2048
void *hlp::t_feed_playback(void *a_nothing)
{

        // Init complete...
        m_playback_stat_percent_complete = 0.0;

        // Get size
        FILE *l_file_ptr;
    // Open file for reading
    l_file_ptr = fopen(m_playback_file.c_str(), "r");
        if (l_file_ptr == NULL)
        {
                //Error opening file
                NDBG_PRINT("Error: opening file = %s Reason: %s.\n",
                m_playback_file.c_str(), strerror(errno));
                return NULL;
        }


        // Stat file for size
    struct stat l_stat;
        if(fstat(fileno(l_file_ptr), &l_stat) == -1)
        {
                // failed to get the length
                NDBG_PRINT("Error: stat'ing file = %s Reason: %s.\n",
                        m_playback_file.c_str(), strerror(errno));
                fclose(l_file_ptr);
                return NULL;
    }


    // Write out contents of file
        NDBG_PRINT("Starting write for file: %s of size = %zd B.\n\n",m_playback_file.c_str(), l_stat.st_size);

    char l_connection_line_buf[HLP_PLAYBACK_MAX_BYTES_PER_LINE];
    char l_line_buf[HLP_PLAYBACK_MAX_BYTES_PER_LINE];
    uint32_t l_bytes_read = 0;
    bool l_is_in_command = false;
    bool l_got_verb = false;
    bool l_skip_ahead = false;
    bool l_first_time = true;

    pb_cmd_t l_cur_cmd;
    l_cur_cmd.init();

    // -----------------------------------------------------
    // TODO: Create playback parser object...
    // -----------------------------------------------------
    while(fgets(l_line_buf, HLP_PLAYBACK_MAX_BYTES_PER_LINE, l_file_ptr) != NULL)
        {
                size_t l_bytes_read_line = 0;
                l_bytes_read_line = strnlen(l_line_buf, HLP_PLAYBACK_MAX_BYTES_PER_LINE);
                if(l_bytes_read_line >= HLP_PLAYBACK_MAX_BYTES_PER_LINE)
                {
                        // failed to get the length
                        NDBG_PRINT("Error: line too long -greater > Max allowabled[%d].\n",
                                HLP_PLAYBACK_MAX_BYTES_PER_LINE);
                        fclose(l_file_ptr);
                        return NULL;
                }
                l_bytes_read += l_bytes_read_line;
                m_playback_stat_percent_complete = 100.0*((float)l_bytes_read / (float)l_stat.st_size);

                // Ignore comments

                // Comment check
                bool l_is_comment_line = false;
                int32_t i_idx = -1;
                while(l_line_buf[++i_idx] == ' ');
                if((l_line_buf[i_idx] == '#') || (l_line_buf[i_idx] == '@'))
                {
                        l_is_comment_line = true;
                }

                // Is blank
                bool l_is_blank_line = false;
                if(i_idx == (int32_t)(l_bytes_read_line - 1))
                {
                        l_is_blank_line = true;
                }

                if(l_is_in_command)
                {
                        // blank line is end of command
                        if(l_is_blank_line)
                        {
                                if(!l_skip_ahead)
                                {
                                        uint32_t l_hash;
                                        uint32_t l_val;
                                        l_hash = CityHash32(l_connection_line_buf, strnlen(l_connection_line_buf, HLP_PLAYBACK_MAX_BYTES_PER_LINE));
                                        l_val = l_hash % m_num_threads;
                                        // TODO Hash connection
                                        //NDBG_PRINT("%s <<BOOP>>: %u: %u\n", l_connection_line_buf, l_val, l_hash);
                                        m_t_client_list[l_val]->add_pb_cmd(l_cur_cmd);

                                }

                                l_is_in_command = false;
                                l_skip_ahead = false;
                                l_got_verb = false;
                            l_cur_cmd.init();

                        } else if(!l_is_comment_line && !l_skip_ahead)
                        {

                                if(!l_got_verb)
                                {
                                        // Get verb...

                                        // Is a command
                                        // FIN
                                        // or
                                        // GET /00C439/live_streaming_content3/0/C4.ts HTTP/1.1
                                        char l_verb[64];
                                        char l_uri[1024];
                                        char l_method[64];
                                        int32_t l_status;
                                        l_status = sscanf(l_line_buf, "%s ", l_verb);
                                        if(l_status != 1)
                                        {
                                                NDBG_PRINT("Error: Couldn't find verb: %s\n", l_line_buf);
                                                l_skip_ahead = true;
                                        }

                                        std::string l_verb_str(l_verb);
                                        if((l_verb_str == "GET") || (l_verb_str == "POST"))
                                        {

                                                l_status = sscanf(l_line_buf, "%s %s %s", l_verb, l_uri, l_method);
                                                if(l_status != 3)
                                                {
                                                        NDBG_PRINT("Error: Couldn't parse verb line: %s\n", l_line_buf);
                                                        l_skip_ahead = true;
                                                }

                                                // TODO REMOVE!!!
                                                //sprintf(l_uri, "/F0.ts");

                                                l_cur_cmd.m_verb = l_verb;
                                                l_cur_cmd.m_uri = l_uri;
                                                l_cur_cmd.m_method = l_method;

                                        }
                                        else if(l_verb_str == "FIN")
                                        {
                                                //printf("%s\n", l_verb_str.c_str());
                                                l_cur_cmd.m_verb = l_verb_str;

                                        }
                                        else if(l_verb_str == "RST")
                                        {
                                                // TODO NOT SUPPORTED YET
                                                NDBG_PRINT("Error: RST Not supported yet: %s\n", l_line_buf);
                                                l_skip_ahead = true;
                                        } else {
                                                // TODO NOT SUPPORTED YET
                                                NDBG_PRINT("Error: Unsupported Verb: %s\n", l_verb_str.c_str());
                                                l_skip_ahead = true;
                                        }

                                        l_got_verb = true;
                                } else {
                                        std::string l_line_str(l_line_buf);
                                        std::string l_key;
                                        std::string l_val;
                                        break_header_string(l_line_str, l_key, l_val);

                                        // Remove endlines and leading spaces
                                        l_val.erase(std::remove(l_val.begin(),l_val.end(), '\n'), l_val.end());
                                        l_val.erase(std::remove(l_val.begin(),l_val.end(), '\r'), l_val.end());
                                        l_val.erase(l_val.begin(), std::find_if(l_val.begin(), l_val.end(), std::bind1st(std::not_equal_to<char>(), ' ')));

                                        // Assume is header...
                                        //printf("HEDR: %s: %s", l_key.c_str(), l_val.c_str());
                                        l_cur_cmd.m_header_map[l_key] = l_val;

                                }
                        }

                } else if(!l_is_comment_line && !l_is_blank_line)
                {
                        // Try to start command
                        // 2.67847016468 192.168.0.51 127.0.0.1 25767 80

                        l_is_in_command = true;
                        double l_timestamp;
                        char l_src_ip[64];
                        char l_dest_ip[64];
                        int32_t l_src_port;
                        int32_t l_dest_port;
                        int32_t l_status;
                        l_status = sscanf(l_line_buf, "%lf %s %s %d %d", &l_timestamp, l_src_ip, l_dest_ip, &l_src_port, &l_dest_port);
                        if(l_status != 5)
                        {
                                NDBG_PRINT("Error: Couldn't parse line: %s\n", l_line_buf);
                                l_skip_ahead = true;
                        }
                        else
                        {

                                // Copy line for hashing
                                snprintf(l_connection_line_buf, HLP_PLAYBACK_MAX_BYTES_PER_LINE,
                                                "%s %s %d %d",
                                                l_src_ip, l_dest_ip, l_src_port, l_dest_port);

                                //printf("HEAD: %s", l_line_buf);
                                l_cur_cmd.m_timestamp_ms= (uint64_t)(l_timestamp*1000.0);

                                // Set first start time
                                if(l_first_time)
                                {
                                        for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
                                        {
                                                m_t_client_list[i_client_idx]->set_first_timestamp_ms(l_cur_cmd.m_timestamp_ms);
                                        }
                                        l_first_time = false;
                                }

                                l_cur_cmd.m_src_ip = l_src_ip;
                                l_cur_cmd.m_src_port = (uint16_t)l_src_port;

                                if(!m_pb_dest_addr.empty())
                                {
                                        l_cur_cmd.m_dest_ip = m_pb_dest_addr;
                                }
                                else
                                {
                                        l_cur_cmd.m_dest_ip = l_dest_ip;
                                }

                                if(m_pb_dest_port > 1)
                                {
                                        l_cur_cmd.m_dest_port = (uint16_t)m_pb_dest_port;
                                }
                                else
                                {
                                        l_cur_cmd.m_dest_port = (uint16_t)l_dest_port;
                                }
                        }
                }
        }

        fclose(l_file_ptr);

        for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
        {
                m_t_client_list[i_client_idx]->set_feeder_done(true);
        }


        return NULL;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlp::set_header(const std::string &a_header_key, const std::string &a_header_val)
{
        int32_t l_retval = STATUS_OK;
        m_header_map[a_header_key] = a_header_val;
        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlp::display_results(double a_elapsed_time,
                uint32_t a_max_parallel,
                bool a_show_breakdown_flag)
{
        stats_collector::get()->display_results(a_elapsed_time, a_max_parallel, a_show_breakdown_flag, m_color);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlp::display_results_line_desc(bool a_color_flag)
{
        stats_collector::get()->display_results_line_desc(a_color_flag);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlp::display_results_line(bool a_color_flag)
{
        stats_collector::get()->display_results_line(a_color_flag);

        if(m_extra_info > 0)
        {
                for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
                {
                        NDBG_OUTPUT("# THREAD[%d]\n", i_client_idx);
                        m_t_client_list[i_client_idx]->show_stats();
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlp::get_stats_json(char *l_json_buf, uint32_t l_json_buf_max_len)
{return stats_collector::get()->get_stats_json(l_json_buf, l_json_buf_max_len);}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlp::stop(void)
{
        int32_t l_retval = STATUS_OK;

        for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
        {
                m_t_client_list[i_client_idx]->stop();
        }

        return l_retval;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlp::wait_till_stopped(void)
{
        int32_t l_retval = STATUS_OK;
        for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
        {
                pthread_join(m_t_client_list[i_client_idx]->m_t_run_thread, NULL);
        }
        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool hlp::is_running(void)
{
        for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
        {
                if(m_t_client_list[i_client_idx]->is_running())
                        return true;
        }
        return false;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t hlp::get_num_cmds(void)
{
        uint64_t l_num = 0;
        for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
        {
                l_num += m_t_client_list[i_client_idx]->m_num_cmds;
        }
        return l_num;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t hlp::get_num_cmds_serviced(void)
{
        uint64_t l_num = 0;
        for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
        {
                l_num += m_t_client_list[i_client_idx]->m_num_cmds_serviced;
        }
        return l_num;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint32_t hlp::get_num_conns(void)
{
        uint64_t l_num = 0;
        for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
        {
                l_num += m_t_client_list[i_client_idx]->get_num_conns();
        }
        return l_num;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t hlp::get_num_reqs_sent(void)
{
        uint64_t l_num = 0;
        for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
        {
                l_num += m_t_client_list[i_client_idx]->m_num_reqs_sent;
        }
        return l_num;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlp::init(void)
{
        // Check if already is initd
        if(m_is_initd)
                return STATUS_OK;

        // SSL init...
        m_ssl_ctx = nconn_ssl_init(m_cipher_list);
        if(NULL == m_ssl_ctx) {
                NDBG_PRINT("Error: performing ssl_init with cipher_list: %s\n", m_cipher_list.c_str());
                return STATUS_ERROR;
        }

        // -------------------------------------------
        // Start async resolver
        // -------------------------------------------
        //t_async_resolver::get()->run();

        m_is_initd = true;
        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlp::hlp(void):
        m_t_feed_playback_thread(),
        m_t_client_list(),
        m_ssl_ctx(NULL),
        m_is_initd(false),
        m_cipher_list(""),
        m_verbose(false),
        m_color(false),
        m_quiet(false),
        m_sock_opt_recv_buf_size(0),
        m_sock_opt_send_buf_size(0),
        m_sock_opt_no_delay(false),
        m_evr_type(EV_EPOLL),
        m_playback_file(),
        m_header_map(),
        m_timeout_s(HLP_DEFAULT_CONN_TIMEOUT_S),
        m_playback_stat_percent_complete(0.0),
        m_pb_dest_addr(),
        m_pb_dest_port(-1),
        m_scale(1.0),
        m_extra_info(0),
        m_num_threads(1)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlp::~hlp()
{
        // SSL Cleanup
        nconn_kill_locks();
        // TODO Deprecated???
        //EVP_cleanup();
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlp *hlp::get(void)
{
        if (m_singleton_ptr == NULL) {
                //If not yet created, create the singleton instance
                m_singleton_ptr = new hlp();
                // Initialize
        }
        return m_singleton_ptr;
}

//: ----------------------------------------------------------------------------
//: Class variables
//: ----------------------------------------------------------------------------
// the pointer to the singleton for the instance 
hlp *hlp::m_singleton_ptr;
