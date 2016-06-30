//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    main.cc
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
#include "hlx/srvr.h"
#include "hlx/stat.h"
#include "hlx/stat_h.h"
#include "hlx/api_resp.h"
#include "hlx/subr.h"
#include "hlx/lsnr.h"
#include "hlx/time_util.h"
#include "hlx/trace.h"
#include "hlx/status.h"
#include "tinymt64.h"
#include "http/t_srvr.h"

#include <string.h>

// getrlimit
#include <sys/time.h>
#include <sys/resource.h>

// Mach time support clock_get_time
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

// signal
#include <signal.h>

#include <list>
#include <algorithm>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h> // For getopt_long
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <stdint.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>

// Profiler
#ifdef ENABLE_PROFILER
#include <gperftools/profiler.h>
//#include <google/profiler.h>
#endif

// For inet_pton
#include <arpa/inet.h>

// Bind
#include <sys/types.h>
#include <sys/socket.h>

// Get resource limits
#include <sys/resource.h>

// Json output
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define UNUSED(x) ( (void)(x) )

//: ----------------------------------------------------------------------------
//: ANSI Color Code Strings
//:
//: Taken from:
//: http://pueblo.sourceforge.net/doc/manual/ansi_color_codes.html
//: ----------------------------------------------------------------------------
#define ANSI_COLOR_OFF          "\033[0m"
#define ANSI_COLOR_FG_BLACK     "\033[01;30m"
#define ANSI_COLOR_FG_RED       "\033[01;31m"
#define ANSI_COLOR_FG_GREEN     "\033[01;32m"
#define ANSI_COLOR_FG_YELLOW    "\033[01;33m"
#define ANSI_COLOR_FG_BLUE      "\033[01;34m"
#define ANSI_COLOR_FG_MAGENTA   "\033[01;35m"
#define ANSI_COLOR_FG_CYAN      "\033[01;36m"
#define ANSI_COLOR_FG_WHITE     "\033[01;37m"
#define ANSI_COLOR_FG_DEFAULT   "\033[01;39m"
#define ANSI_COLOR_BG_BLACK     "\033[01;40m"
#define ANSI_COLOR_BG_RED       "\033[01;41m"
#define ANSI_COLOR_BG_GREEN     "\033[01;42m"
#define ANSI_COLOR_BG_YELLOW    "\033[01;43m"
#define ANSI_COLOR_BG_BLUE      "\033[01;44m"
#define ANSI_COLOR_BG_MAGENTA   "\033[01;45m"
#define ANSI_COLOR_BG_CYAN      "\033[01;46m"
#define ANSI_COLOR_BG_WHITE     "\033[01;47m"
#define ANSI_COLOR_BG_DEFAULT   "\033[01;49m"

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------
// ---------------------------------------
// Wildcard support
// ---------------------------------------
typedef enum path_order_enum {

        EXPLODED_PATH_ORDER_SEQUENTIAL = 0,
        EXPLODED_PATH_ORDER_RANDOM

} path_order_t;

// ---------------------------------------
// Results scheme
// ---------------------------------------
typedef enum results_scheme_enum {

        RESULTS_SCHEME_STD = 0,
        RESULTS_SCHEME_HTTP_LOAD,
        RESULTS_SCHEME_HTTP_LOAD_LINE,
        RESULTS_SCHEME_JSON

} results_scheme_t;

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef struct range_struct {
        uint32_t m_start;
        uint32_t m_end;
} range_t;

typedef std::list <std::string> header_str_list_t;
typedef std::vector <std::string> path_substr_vector_t;
typedef std::vector <std::string> path_vector_t;
typedef std::map<std::string, std::string> header_map_t;
typedef std::vector <range_t> range_vector_t;

//: ----------------------------------------------------------------------------
//: Settings
//: ----------------------------------------------------------------------------
typedef struct settings_struct
{
        bool m_verbose;
        bool m_color;
        bool m_quiet;
        bool m_show_response_codes;
        bool m_show_per_interval;
        uint32_t m_interval_ms;
        uint32_t m_num_parallel;
        ns_hlx::srvr *m_srvr;
        uint64_t m_start_time_ms;
        int32_t m_run_time_ms;

        // ---------------------------------
        // Defaults...
        // ---------------------------------
        settings_struct() :
                m_verbose(false),
                m_color(false),
                m_quiet(false),
                m_show_response_codes(false),
                m_show_per_interval(false),
                m_interval_ms(500),
                m_num_parallel(1),
                m_srvr(NULL),
                m_start_time_ms(),
                m_run_time_ms(-1)
        {
        }

        ~settings_struct()
        {
        }
private:
        // Disallow copy/assign
        settings_struct& operator=(const settings_struct &);
        settings_struct(const settings_struct &);
} settings_struct_t;

//: ----------------------------------------------------------------------------
//: Prototypes
//: ----------------------------------------------------------------------------
void display_results_line_desc(settings_struct &a_settings);
void display_results_line(settings_struct &a_settings);
void display_responses_line_desc(settings_struct &a_settings);
void display_responses_line(settings_struct &a_settings);

void get_results(settings_struct &a_settings, double a_elapsed_time,
                 std::string &ao_results);

void get_results_http_load(settings_struct &a_settings, double a_elapsed_time,
                           std::string &ao_results, bool a_one_line_flag = false);

void get_results_json(settings_struct &a_settings, double a_elapsed_time,
                      std::string &ao_results);

int32_t read_file(const char *a_file, char **a_buf, uint32_t *a_len);

//: ----------------------------------------------------------------------------
//: Globals
//: ----------------------------------------------------------------------------
static ns_hlx::srvr *g_hlx = NULL;
static bool g_test_finished = false;
static bool g_cancelled = false;
static settings_struct_t *g_settings = NULL;
static uint64_t g_rate_delta_us = 0;
static uint32_t g_num_threads = 1;
static int64_t g_num_to_request = -1;
static uint64_t g_num_requested = 0;
static uint64_t g_num_completed = 0;

// -----------------------------------------------
// Path vector support
// -----------------------------------------------
static tinymt64_t *g_rand_ptr = NULL;
static path_vector_t g_path_vector;
static std::string g_path;
static uint32_t g_path_vector_last_idx = 0;
static path_order_t g_path_order = EXPLODED_PATH_ORDER_RANDOM;
static pthread_mutex_t g_path_vector_mutex;
static pthread_mutex_t g_completion_mutex;

// -----------------------------------------------
// Address rotation
// -----------------------------------------------
#define LOCAL_ADDR_V4_MIN 0x7f000001
#define LOCAL_ADDR_V4_MAX 0x7ffffffe
static uint32_t g_addrx_addr_ipv4 = LOCAL_ADDR_V4_MIN;
static uint32_t g_addrx_addr_ipv4_max = LOCAL_ADDR_V4_MAX;
static pthread_mutex_t g_addrx_mutex;

//: ----------------------------------------------------------------------------
//: \details: sighandler
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void sig_handler(int signo)
{
        if (signo == SIGINT)
        {
                // Kill program
                g_test_finished = true;
                g_cancelled = true;
        }
}

//: ----------------------------------------------------------------------------
//: Command
//: TODO Refactor
//: ----------------------------------------------------------------------------
#define NB_ENABLE  1
#define NB_DISABLE 0

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int kbhit()
{
        struct timeval l_tv;
        fd_set l_fds;
        l_tv.tv_sec = 0;
        l_tv.tv_usec = 0;
        FD_ZERO(&l_fds);
        FD_SET(STDIN_FILENO, &l_fds);
        //STDIN_FILENO is 0
        select(STDIN_FILENO + 1, &l_fds, NULL, NULL, &l_tv);
        return FD_ISSET(STDIN_FILENO, &l_fds);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nonblock(int state)
{
        struct termios ttystate;

        //get the terminal state
        tcgetattr(STDIN_FILENO, &ttystate);

        if (state == NB_ENABLE)
        {
                //turn off canonical mode
                ttystate.c_lflag &= ~ICANON;
                //minimum of number input read.
                ttystate.c_cc[VMIN] = 1;
        } else if (state == NB_DISABLE)
        {
                //turn on canonical mode
                ttystate.c_lflag |= ICANON;
        }
        //set the terminal attributes.
        tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void command_exec(settings_struct_t &a_settings)
{
        int i = 0;
        char l_cmd = ' ';
        ns_hlx::srvr *l_srvr = a_settings.m_srvr;
        bool l_first_time = true;
        nonblock(NB_ENABLE);
        // ---------------------------------------
        // Loop forever until user quits
        // ---------------------------------------
        while (!g_test_finished)
        {
                //NDBG_PRINT("BOOP.\n");

                i = kbhit();
                if (i != 0)
                {

                        l_cmd = fgetc(stdin);

                        switch (l_cmd)
                        {
                        // -------------------------------------------
                        // Display
                        // -------------------------------------------
                        case 'd':
                        {
                                a_settings.m_srvr->display_stat();
                                break;
                        }
                        // -------------------------------------------
                        // Quit
                        // -------------------------------------------
                        case 'q':
                        {
                                g_test_finished = true;
                                break;
                        }
                        // -------------------------------------------
                        // default
                        // -------------------------------------------
                        default:
                        {
                                break;
                        }
                        }
                }

                // TODO add define...
                usleep(a_settings.m_interval_ms*1000);

                // Check for done
                if(a_settings.m_run_time_ms != -1)
                {
                        int32_t l_time_delta_ms = (int32_t)(ns_hlx::get_delta_time_ms(a_settings.m_start_time_ms));
                        if(l_time_delta_ms >= a_settings.m_run_time_ms)
                        {
                                g_test_finished = true;
                        }
                }
                if (!l_srvr->is_running())
                {
                        g_test_finished = true;
                        break;
                }
                if(!a_settings.m_quiet && !a_settings.m_verbose)
                {
                        if(a_settings.m_show_response_codes)
                        {
                                if(l_first_time)
                                {
                                        display_responses_line_desc(a_settings);
                                        ns_hlx::t_stat_cntr_t l_total;
                                        ns_hlx::t_stat_calc_t l_total_calc;
                                        ns_hlx::t_stat_cntr_list_t l_thread;
                                        a_settings.m_srvr->get_stat(l_total, l_total_calc, l_thread, true);
                                        if(!g_test_finished)
                                        {
                                                usleep(a_settings.m_interval_ms*1000+100);
                                        }
                                        l_first_time = false;
                                }
                                display_responses_line(a_settings);
                        }
                        else
                        {
                                if(l_first_time)
                                {
                                        display_results_line_desc(a_settings);
                                        ns_hlx::t_stat_cntr_t l_total;
                                        ns_hlx::t_stat_calc_t l_total_calc;
                                        ns_hlx::t_stat_cntr_list_t l_thread;
                                        a_settings.m_srvr->get_stat(l_total, l_total_calc, l_thread, true);
                                        if(!g_test_finished)
                                        {
                                                usleep(a_settings.m_interval_ms*1000+100);
                                        }
                                        l_first_time = false;
                                }
                                display_results_line(a_settings);
                        }
                }
        }
        nonblock(NB_DISABLE);
}

//: ----------------------------------------------------------------------------
//: \details: Completion callback
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t s_completion_cb(ns_hlx::subr &a_subr,
                               ns_hlx::nconn &a_nconn,
                               ns_hlx::resp &a_resp)
{
        pthread_mutex_lock(&g_completion_mutex);
        ++g_num_completed;
        pthread_mutex_unlock(&g_completion_mutex);
        if((g_num_to_request != -1) && (g_num_completed >= (uint64_t)g_num_to_request))
        {
                g_test_finished = true;
                return 0;
        }
        if(g_rate_delta_us && !g_test_finished)
        {
                usleep(g_rate_delta_us*g_num_threads);
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t s_pre_connect_cb(int a_fd)
{
        pthread_mutex_lock(&g_addrx_mutex);
        struct sockaddr_in l_c_addr;
        bzero((char *) &l_c_addr, sizeof(l_c_addr));
        l_c_addr.sin_family = AF_INET;
        l_c_addr.sin_addr.s_addr = htonl(g_addrx_addr_ipv4);
        l_c_addr.sin_port = htons(0);
        int32_t l_s;
        errno = 0;
        l_s = bind(a_fd, (struct sockaddr *) &l_c_addr, sizeof(l_c_addr));
        if(l_s != 0)
        {
                printf("%s.%s.%d: Error performing bind. Reason: %s.\n",
                       __FILE__,__FUNCTION__,__LINE__,strerror(errno));
                pthread_mutex_unlock(&g_addrx_mutex);
                return HLX_STATUS_ERROR;
        }
        ++g_addrx_addr_ipv4;
        if(g_addrx_addr_ipv4 >= g_addrx_addr_ipv4_max)
        {
                g_addrx_addr_ipv4 = LOCAL_ADDR_V4_MIN;
        }
        pthread_mutex_unlock(&g_addrx_mutex);
        return HLX_STATUS_OK;
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

        return HLX_STATUS_ERROR;

success:
        // Check range...
        if(l_start > l_end)
        {
                printf("HLX_STATUS_ERROR: Bad range start[%u] > end[%u]\n", l_start, l_end);
                return HLX_STATUS_ERROR;
        }

        // Successfully matched we outie
        ao_range.m_start = l_start;
        ao_range.m_end = l_end;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t parse_path(const char *a_path,
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
                        printf("HLX_STATUS_ERROR: Bad range for path: %s at pos: %lu\n", a_path, l_range_start_pos);
                        return HLX_STATUS_ERROR;
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
                int l_status = HLX_STATUS_OK;
                l_status = convert_exp_to_range(l_range_exp, l_range);
                if(HLX_STATUS_OK != l_status)
                {
                        printf("HLX_STATUS_ERROR: performing convert_exp_to_range(%s)\n", l_range_exp.c_str());
                        return HLX_STATUS_ERROR;
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
                printf("SUBSTR: %s\n", i_substr->c_str());
        }

        for(range_vector_t::iterator i_range = ao_range_vector.begin();
                        i_range != ao_range_vector.end();
                        ++i_range)
        {
                printf("RANGE: %u -- %u\n", i_range->m_start, i_range->m_end);
        }
#endif
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t path_exploder(std::string a_path_part,
                      const path_substr_vector_t &a_substr_vector,
                      uint32_t a_substr_idx,
                      const range_vector_t &a_range_vector,
                      uint32_t a_range_idx)
{
        //a_path_part
        if(a_substr_idx >= a_substr_vector.size())
        {
                g_path_vector.push_back(a_path_part);
                return HLX_STATUS_OK;
        }

        a_path_part.append(a_substr_vector[a_substr_idx]);
        ++a_substr_idx;

        if(a_range_idx >= a_range_vector.size())
        {
                g_path_vector.push_back(a_path_part);
                return HLX_STATUS_OK;
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
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t add_option(const char *a_key, const char *a_val)
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
                {
                        g_path_order = EXPLODED_PATH_ORDER_SEQUENTIAL;
                }
                else if (l_val == "random")
                {
                        g_path_order = EXPLODED_PATH_ORDER_RANDOM;
                }
                else
                {
                        printf("HLX_STATUS_ERROR: Bad value[%s] for key[%s]\n", l_val.c_str(), l_key.c_str());
                        return HLX_STATUS_ERROR;
                }
        }
        //: ------------------------------------------------
        //:
        //: ------------------------------------------------
        //else if (l_key == "tag")
        //{
        //        //NDBG_PRINT("HEADER: %s: %s\n", l_val.substr(0,l_pos).c_str(), l_val.substr(l_pos+1,l_val.length()).c_str());
        //        m_label = l_val;
        //}
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
                        NDBG_PRINT("HLX_STATUS_ERROR: Bad value[%s] for key[%s]\n", l_val.c_str(), l_key.c_str());
                        return HLX_STATUS_ERROR;
                }
        }
#endif
        else
        {
                printf("HLX_STATUS_ERROR: Unrecognized key[%s]\n", l_key.c_str());
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define SPECIAL_EFX_OPT_SEPARATOR ";"
#define SPECIAL_EFX_KV_SEPARATOR "="
int32_t special_effects_parse(std::string &a_path)
{
        //printf("SPECIAL_FX_PARSE: path: %s\n", a_path.c_str());
        // -------------------------------------------
        // 1. Break by separator ";"
        // 2. Check for exploded path
        // 3. For each bit after path
        //        Split by Key "=" Value
        // -------------------------------------------
        // Bail out if no path
        if(a_path.empty())
        {
                return HLX_STATUS_OK;
        }

        // -------------------------------------------
        // TODO This seems hacky...
        // -------------------------------------------
        // strtok is NOT thread-safe but not sure it matters here...
        char l_path[2048];
        char *l_save_ptr;
        strncpy(l_path, a_path.c_str(), 2048);
        char *l_p = strtok_r(l_path, SPECIAL_EFX_OPT_SEPARATOR, &l_save_ptr);

        if( a_path.front() != *SPECIAL_EFX_OPT_SEPARATOR )
        {
                // Rule out special cases that m_path only contains options
                //
                a_path = l_p;

                int32_t l_status;
                path_substr_vector_t l_path_substr_vector;
                range_vector_t l_range_vector;
                if(l_p)
                {
                        l_status = parse_path(l_p, l_path_substr_vector, l_range_vector);
                        if(l_status != HLX_STATUS_OK)
                        {
                                printf("HLX_STATUS_ERROR: Performing parse_path(%s)\n", l_p);
                                return HLX_STATUS_ERROR;
                        }
                }

                // If empty path explode
                if(l_range_vector.size())
                {
                        l_status = path_exploder(std::string(""), l_path_substr_vector, 0, l_range_vector, 0);
                        if(l_status != HLX_STATUS_OK)
                        {
                                printf("HLX_STATUS_ERROR: Performing explode_path(%s)\n", l_p);
                                return HLX_STATUS_ERROR;
                        }
                        // DEBUG show paths
                        //printf(" -- Displaying Paths --\n");
                        //uint32_t i_path_cnt = 0;
                        //for(path_vector_t::iterator i_path = g_path_vector.begin();
                        //              i_path != g_path_vector.end();
                        //              ++i_path, ++i_path_cnt)
                        //{
                        //      printf(": [%6d]: %s\n", i_path_cnt, i_path->c_str());
                        //}
                }
                else
                {
                        g_path_vector.push_back(a_path);
                }

                l_p = strtok_r(NULL, SPECIAL_EFX_OPT_SEPARATOR, &l_save_ptr);
        }
        else
        {
                g_path.clear();
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
                        if(l_add_option_status != HLX_STATUS_OK)
                        {
                                printf("HLX_STATUS_ERROR: Performing add_option(%s,%s)\n", l_k,l_v);
                                // Nuttin doing???

                        }
                        //printf("Key: %s\n", l_k);
                        //printf("Val: %s\n", l_v);
                }
                l_p = strtok_r(NULL, SPECIAL_EFX_OPT_SEPARATOR, &l_save_ptr);
        }
        //printf("a_path: %s\n", a_path.c_str());
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &get_path(void *a_rand)
{
        // TODO -make this threadsafe -mutex per???
        if(g_path_vector.size())
        {
                // Rollover..
                if(g_path_order == EXPLODED_PATH_ORDER_SEQUENTIAL)
                {
                        pthread_mutex_lock(&g_path_vector_mutex);
                        if(g_path_vector_last_idx >= g_path_vector.size())
                        {
                                g_path_vector_last_idx = 0;
                        }
                        uint32_t l_last_idx = g_path_vector_last_idx;
                        ++g_path_vector_last_idx;
                        pthread_mutex_unlock(&g_path_vector_mutex);
                        return g_path_vector[l_last_idx];
                }
                else
                {
                        uint32_t l_rand_idx = 0;
                        if(a_rand)
                        {
                                tinymt64_t *l_rand_ptr = (tinymt64_t*)a_rand;
                                l_rand_idx = (uint32_t)(tinymt64_generate_uint64(l_rand_ptr) % g_path_vector.size());
                        }
                        else
                        {
                                l_rand_idx = (random() * g_path_vector.size()) / RAND_MAX;
                        }
                        return g_path_vector[l_rand_idx];
                }
        }
        else
        {
                return g_path;
        }
}

//: ----------------------------------------------------------------------------
//: \details: create request callback
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t s_create_request_cb(ns_hlx::subr &a_subr, ns_hlx::nbq &a_nbq)
{
        pthread_mutex_lock(&g_completion_mutex);
        if(g_test_finished ||
           ((g_num_to_request != -1) &&
            (g_num_requested >= (uint32_t)g_num_to_request)))
        {
                pthread_mutex_unlock(&g_completion_mutex);
                return HLX_STATUS_ERROR;
        }
        ++g_num_requested;
        pthread_mutex_unlock(&g_completion_mutex);

        // TODO grab from path...
        std::string l_path_ref = get_path(g_rand_ptr);

        char l_buf[2048];
        int32_t l_len = 0;
        if(l_path_ref.empty())
        {
                l_path_ref = "/";
        }
        if(!(a_subr.get_query().empty()))
        {
                l_path_ref += "?";
                l_path_ref += a_subr.get_query();
        }
        //NDBG_PRINT("HOST: %s PATH: %s\n", a_reqlet.m_url.m_host.c_str(), l_path_ref.c_str());
        l_len = snprintf(l_buf, sizeof(l_buf),
                        "%s %.500s HTTP/1.1", a_subr.get_verb().c_str(), l_path_ref.c_str());

        ns_hlx::nbq_write_request_line(a_nbq, l_buf, l_len);

        // -------------------------------------------
        // Add repo headers
        // -------------------------------------------
        bool l_specd_host = false;

        // Loop over reqlet map
        for(ns_hlx::kv_map_list_t::const_iterator i_hl = a_subr.get_headers().begin();
            i_hl != a_subr.get_headers().end();
            ++i_hl)
        {
                if(i_hl->first.empty() || i_hl->second.empty())
                {
                        continue;
                }
                for(ns_hlx::str_list_t::const_iterator i_v = i_hl->second.begin();
                    i_v != i_hl->second.end();
                    ++i_v)
                {
                        ns_hlx::nbq_write_header(a_nbq, i_hl->first.c_str(), i_hl->first.length(), i_v->c_str(), i_v->length());
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
                ns_hlx::nbq_write_header(a_nbq, "Host", strlen("Host"),
                                         a_subr.get_host().c_str(), a_subr.get_host().length());
        }

        // -------------------------------------------
        // body
        // -------------------------------------------
        if(a_subr.get_body_data() && a_subr.get_body_len())
        {
                //NDBG_PRINT("Write: buf: %p len: %d\n", l_buf, l_len);
                ns_hlx::nbq_write_body(a_nbq, a_subr.get_body_data(), a_subr.get_body_len());
        }
        else
        {
                ns_hlx::nbq_write_body(a_nbq, NULL, 0);
        }

        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: Print the version.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_version(FILE* a_stream, int a_exit_code)
{

        // print out the version information
        fprintf(a_stream, "hurl HTTP Load Tester.\n");
        fprintf(a_stream, "Copyright (C) 2015 Verizon Digital Media.\n");
        fprintf(a_stream, "               Version: %s\n", HLX_VERSION);
        exit(a_exit_code);
}

//: ----------------------------------------------------------------------------
//: \details: Print the command line help.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_usage(FILE* a_stream, int a_exit_code)
{
        fprintf(a_stream, "Usage: hurl [http[s]://]hostname[:port]/path [options]\n");
        fprintf(a_stream, "Options are:\n");
        fprintf(a_stream, "  -h, --help           Display this help and exit.\n");
        fprintf(a_stream, "  -V, --version        Display the version number and exit.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Input Options:\n");
        fprintf(a_stream, "  -w, --no_wildcards   Don't wildcard the url.\n");
        fprintf(a_stream, "  -M, --mode           Request mode -if multipath [random(default) | sequential].\n");
        fprintf(a_stream, "  -d, --data           HTTP body data -supports curl style @ file specifier\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Settings:\n");
        fprintf(a_stream, "  -y, --cipher         Cipher --see \"openssl ciphers\" for list.\n");
        fprintf(a_stream, "  -p, --parallel       Num parallel. Default: 100.\n");
        fprintf(a_stream, "  -f, --fetches        Num fetches.\n");
        fprintf(a_stream, "  -N, --num_calls      Number of requests per connection\n");
        fprintf(a_stream, "  -t, --threads        Number of parallel threads. Default: 1\n");
        fprintf(a_stream, "  -H, --header         Request headers -can add multiple ie -H<> -H<>...\n");
        fprintf(a_stream, "  -X, --verb           Request command -HTTP verb to use -GET/PUT/etc. Default GET\n");
        fprintf(a_stream, "  -l, --seconds        Run for <N> seconds .\n");
        fprintf(a_stream, "  -A, --rate           Max Request Rate.\n");
        fprintf(a_stream, "  -R, --recv_buffer    Socket receive buffer size.\n");
        fprintf(a_stream, "  -S, --send_buffer    Socket send buffer size.\n");
        fprintf(a_stream, "  -D, --no_delay       Disable socket TCP no-delay (on by default).\n");
        fprintf(a_stream, "  -T, --timeout        Timeout (seconds).\n");
        fprintf(a_stream, "  -x, --no_stats       Don't collect stats -faster.\n");
        fprintf(a_stream, "  -r, --addr_seq       Rotate through local addresses (number).\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Print Options:\n");
        fprintf(a_stream, "  -v, --verbose        Verbose logging\n");
        fprintf(a_stream, "  -c, --no_color       Turn off colors\n");
        fprintf(a_stream, "  -q, --quiet          Suppress progress output\n");
        fprintf(a_stream, "  -C, --responses      Display http(s) response codes instead of request statistics\n");
        fprintf(a_stream, "  -L, --responses_per  Display http(s) response codes per interval instead of request statistics\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Stat Options:\n");
        fprintf(a_stream, "  -U, --update         Update output every N ms. Default 500ms.\n");
        fprintf(a_stream, "  -P, --data_port      Start HTTP Stats Daemon on port.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Results Options:\n");
        fprintf(a_stream, "  -j, --json           Display results in json\n");
        fprintf(a_stream, "  -Y, --http_load      Display results in http load mode -Legacy support\n");
        fprintf(a_stream, "  -Z, --http_load_line Display results in http load mode on a single line -Legacy support\n");
        fprintf(a_stream, "  -o, --output         Output results to file <FILE> -default to stdout\n");
        fprintf(a_stream, "  \n");
#ifdef ENABLE_PROFILER
        fprintf(a_stream, "Profile Options:\n");
        fprintf(a_stream, "  -G, --gprofile       Google profiler output file\n");
        fprintf(a_stream, "  \n");
#endif
        fprintf(a_stream, "Note: If running long jobs consider enabling tcp_tw_reuse -eg:\n");
        fprintf(a_stream, "echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse\n");
        fprintf(a_stream, "\n");
        exit(a_exit_code);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int main(int argc, char** argv)
{
        results_scheme_t l_results_scheme = RESULTS_SCHEME_STD;
        int32_t l_http_data_port = -1;

        int l_max_threads = 1;
        // TODO Make default definitions
        int l_num_parallel = 100;
        int l_max_reqs_per_conn = 1;
        bool l_input_flag = false;
        bool l_wildcarding = true;
        std::string l_output_file = "";

        // Get srvr instance
        ns_hlx::srvr *l_srvr = new ns_hlx::srvr();
        g_hlx = l_srvr;

        // Suppress errors
        ns_hlx::trc_log_level_set(ns_hlx::TRC_LOG_LEVEL_NONE);

        // For sighandler
        settings_struct_t l_settings;
        l_settings.m_srvr = l_srvr;
        g_settings = &l_settings;

        // hlx settings
        l_srvr->set_collect_stats(true);
        l_srvr->set_count_response_status(true);
        l_srvr->set_dns_use_ai_cache(true);
        l_srvr->set_dns_use_sync(true);
        l_srvr->set_num_threads(l_max_threads);
        l_srvr->set_num_parallel(l_num_parallel);
        l_srvr->set_num_reqs_per_conn(-1);
        l_srvr->set_update_stats_ms(l_settings.m_interval_ms);
        l_srvr->set_sock_opt_no_delay(true);

        if(isatty(fileno(stdout)))
        {
                l_settings.m_color = true;
                l_srvr->set_rqst_resp_logging_color(true);
        }

        // -------------------------------------------------
        // Subrequest settings
        // -------------------------------------------------

        ns_hlx::subr *l_subr = new ns_hlx::subr();
        l_subr->set_uid(l_srvr->get_next_subr_uuid());
        l_subr->set_save(false);

        // Default headers
        l_subr->set_header("User-Agent","hurl Server Load Tester");
        l_subr->set_header("Accept","*/*");
        l_subr->set_keepalive(true);
        //l_subr->set_header("User-Agent","ONGA_BONGA (╯°□°）╯︵ ┻━┻)");
        //l_subr->set_header("User-Agent","Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/33.0.1750.117 Safari/537.36");
        //l_subr->set_header("Accept","text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8");
        //l_subr->set_header("Accept","gzip,deflate");
        //l_subr->set_header("Connection","keep-alive");

        l_subr->set_num_to_request(-1);
        l_subr->set_kind(ns_hlx::subr::SUBR_KIND_DUPE);
        l_subr->set_create_req_cb(s_create_request_cb);
        //l_subr->set_num_reqs_per_conn(1);

        // Initialize rand...
        g_rand_ptr = (tinymt64_t*)calloc(1, sizeof(tinymt64_t));
        tinymt64_init(g_rand_ptr, ns_hlx::get_time_us());

        // Initialize mutex for sequential path requesting
        pthread_mutex_init(&g_path_vector_mutex, NULL);

        // Completion
        pthread_mutex_init(&g_completion_mutex, NULL);

        // -------------------------------------------
        // Get args...
        // -------------------------------------------
        char l_opt;
        std::string l_argument;
        int l_option_index = 0;
        struct option l_long_options[] =
                {
                { "help",           0, 0, 'h' },
                { "version",        0, 0, 'V' },
                { "no_wildcards",   0, 0, 'w' },
                { "data",           1, 0, 'd' },
                { "cipher",         1, 0, 'y' },
                { "parallel",       1, 0, 'p' },
                { "fetches",        1, 0, 'f' },
                { "num_calls",      1, 0, 'N' },
                { "threads",        1, 0, 't' },
                { "header",         1, 0, 'H' },
                { "verb",           1, 0, 'X' },
                { "rate",           1, 0, 'A' },
                { "mode",           1, 0, 'M' },
                { "seconds",        1, 0, 'l' },
                { "recv_buffer",    1, 0, 'R' },
                { "send_buffer",    1, 0, 'S' },
                { "no_delay",       0, 0, 'D' },
                { "timeout",        1, 0, 'T' },
                { "no_stats",       0, 0, 'x' },
                { "addr_seq",       1, 0, 'r' },
                { "verbose",        0, 0, 'v' },
                { "no_color",       0, 0, 'c' },
                { "quiet",          0, 0, 'q' },
                { "responses",      0, 0, 'C' },
                { "responses_per",  0, 0, 'L' },
                { "json",           0, 0, 'j' },
                { "http_load",      0, 0, 'Y' },
                { "http_load_line", 0, 0, 'Z' },
                { "output",         1, 0, 'o' },
                { "update",         1, 0, 'U' },
                { "data_port",      1, 0, 'P' },
#ifdef ENABLE_PROFILER
                { "gprofile",       1, 0, 'G' },
#endif
                // list sentinel
                { 0, 0, 0, 0 }
        };

        // -------------------------------------------
        // Assume unspecified arg url...
        // TODO Unsure if good way to allow unspecified
        // arg...
        // -------------------------------------------
        std::string l_url;
#ifdef ENABLE_PROFILER
        std::string l_gprof_file;
#endif
        bool is_opt = false;

        for(int i_arg = 1; i_arg < argc; ++i_arg) {

                if(argv[i_arg][0] == '-') {
                        // next arg is for the option
                        is_opt = true;
                }
                else if(argv[i_arg][0] != '-' && is_opt == false) {
                        // Stuff in url field...
                        l_url = std::string(argv[i_arg]);
                        //if(l_settings.m_verbose)
                        //{
                        //      NDBG_PRINT("Found unspecified argument: %s --assuming url...\n", l_url.c_str());
                        //}
                        l_input_flag = true;
                        break;
                } else {
                        // reset option flag
                        is_opt = false;
                }

        }

#ifdef ENABLE_PROFILER
        char l_short_arg_list[] = "hVwd:y:p:f:N:t:H:X:A:M:l:R:S:DT:xr:vcqCLjYZo:P:U:G:";
#else
        char l_short_arg_list[] = "hVwd:y:p:f:N:t:H:X:A:M:l:R:S:DT:xr:vcqCLjYZo:P:U:";
#endif

        while ((l_opt = getopt_long_only(argc, argv, l_short_arg_list, l_long_options, &l_option_index)) != -1)
        {

                if (optarg)
                        l_argument = std::string(optarg);
                else
                        l_argument.clear();
                //NDBG_PRINT("arg[%c=%d]: %s\n", l_opt, l_option_index, l_argument.c_str());

                switch (l_opt)
                {
                // ---------------------------------------
                // Help
                // ---------------------------------------
                case 'h':
                {
                        print_usage(stdout, 0);
                        break;
                }
                // ---------------------------------------
                // Version
                // ---------------------------------------
                case 'V':
                {
                        print_version(stdout, 0);
                        break;
                }
                // ---------------------------------------
                // Wildcarding
                // ---------------------------------------
                case 'w':
                {
                        l_wildcarding = false;
                        break;
                }
                // ---------------------------------------
                // Data
                // ---------------------------------------
                case 'd':
                {
                        // TODO Size limits???
                        int32_t l_status;
                        // If a_data starts with @ assume file
                        if(l_argument[0] == '@')
                        {
                                char *l_buf;
                                uint32_t l_len;
                                l_status = read_file(l_argument.data() + 1, &(l_buf), &(l_len));
                                if(l_status != 0)
                                {
                                        printf("Error reading body data from file: %s\n", l_argument.c_str() + 1);
                                        return -1;
                                }
                                l_subr->set_body_data(l_buf, l_len);
                        }
                        else
                        {
                                char *l_buf;
                                uint32_t l_len;
                                l_len = l_argument.length() + 1;
                                l_buf = (char *)malloc(sizeof(char)*l_len);
                                l_subr->set_body_data(l_buf, l_len);
                        }

                        // Add content length
                        char l_len_str[64];
                        sprintf(l_len_str, "%u", l_subr->get_body_len());
                        l_subr->set_header("Content-Length", l_len_str);
                        break;
                }
#ifdef ENABLE_PROFILER
                // ---------------------------------------
                // Google Profiler Output File
                // ---------------------------------------
                case 'G':
                {
                        l_gprof_file = l_argument;
                        break;
                }
#endif
                // ---------------------------------------
                // cipher
                // ---------------------------------------
                case 'y':
                {
                        l_srvr->set_tls_client_ctx_cipher_list(l_argument);
                        break;
                }
                // ---------------------------------------
                // parallel
                // ---------------------------------------
                case 'p':
                {
                        //NDBG_PRINT("arg: --parallel: %s\n", optarg);
                        //l_settings.m_start_type = START_PARALLEL;
                        l_num_parallel = atoi(optarg);
                        if (l_num_parallel < 1)
                        {
                                printf("Error parallel must be at least 1\n");
                                return -1;
                        }
                        l_srvr->set_num_parallel(l_num_parallel);
                        l_settings.m_num_parallel = l_num_parallel;
                        break;
                }
                // ---------------------------------------
                // fetches
                // ---------------------------------------
                case 'f':
                {
                        int32_t l_end_fetches = atoi(optarg);
                        if (l_end_fetches < 1)
                        {
                                printf("Error fetches must be at least 1\n");
                                return -1;
                        }
                        l_subr->set_num_to_request(l_end_fetches);
                        g_num_to_request = l_end_fetches;
                        break;
                }
                // ---------------------------------------
                // number of calls per connection
                // ---------------------------------------
                case 'N':
                {
                        l_max_reqs_per_conn = atoi(optarg);
                        if(l_max_reqs_per_conn < 1)
                        {
                                printf("Error num-calls must be at least 1");
                                return -1;
                        }
                        if (l_max_reqs_per_conn == 1)
                        {
                                l_subr->set_keepalive(false);
                        }
                        break;
                }
                // ---------------------------------------
                // num threads
                // ---------------------------------------
                case 't':
                {
                        //NDBG_PRINT("arg: --threads: %s\n", l_argument.c_str());
                        l_max_threads = atoi(optarg);
                        if (l_max_threads < 0)
                        {
                                printf("Error num-threads must be 0 or greater\n");
                                return -1;
                        }
                        l_srvr->set_num_threads(l_max_threads);
                        g_num_threads = l_max_threads;
                        break;
                }
                // ---------------------------------------
                // Header
                // ---------------------------------------
                case 'H':
                {
                        int32_t l_status;
                        l_status = l_subr->set_header(l_argument);
                        if (l_status != 0)
                        {
                                printf("Error performing set_header: %s\n", l_argument.c_str());
                                return -1;
                        }
                        break;
                }
                // ---------------------------------------
                // Verb
                // ---------------------------------------
                case 'X':
                {
                        if(l_argument.length() > 64)
                        {
                                printf("Error verb string: %s too large try < 64 chars\n", l_argument.c_str());
                                return -1;
                        }
                        l_subr->set_verb(l_argument);
                        break;
                }
                // ---------------------------------------
                // rate
                // ---------------------------------------
                case 'A':
                {
                        int l_rate = atoi(optarg);
                        if (l_rate < 1)
                        {
                                printf("Error: rate must be at least 1\n");
                                //print_usage(stdout, -1);
                                return -1;
                        }
                        g_rate_delta_us = 1000000 / l_rate;
                        break;
                }
                // ---------------------------------------
                // Mode
                // ---------------------------------------
                case 'M':
                {
                        std::string l_order = optarg;
                        if(l_order == "sequential")
                        {
                                g_path_order = EXPLODED_PATH_ORDER_SEQUENTIAL;
                        }
                        else if(l_order == "random")
                        {
                                g_path_order = EXPLODED_PATH_ORDER_RANDOM;
                        }
                        else
                        {
                                printf("Error: Mode must be [roundrobin|sequential|random]\n");
                                //print_usage(stdout, -1);
                                return -1;
                        }
                        break;
                }
                // ---------------------------------------
                // seconds
                // ---------------------------------------
                case 'l':
                {
                        int l_run_time_s = atoi(optarg);
                        if (l_run_time_s < 1)
                        {
                                printf("Error: seconds must be at least 1\n");
                                //print_usage(stdout, -1);
                                return -1;
                        }
                        l_settings.m_run_time_ms = l_run_time_s*1000;

                        break;
                }
                // ---------------------------------------
                // sock_opt_recv_buf_size
                // ---------------------------------------
                case 'R':
                {
                        int l_sock_opt_recv_buf_size = atoi(optarg);
                        // TODO Check value...
                        l_srvr->set_sock_opt_recv_buf_size(l_sock_opt_recv_buf_size);

                        break;
                }
                // ---------------------------------------
                // sock_opt_send_buf_size
                // ---------------------------------------
                case 'S':
                {
                        int l_sock_opt_send_buf_size = atoi(optarg);
                        // TODO Check value...
                        l_srvr->set_sock_opt_send_buf_size(l_sock_opt_send_buf_size);
                        break;
                }
                // ---------------------------------------
                // No delay
                // ---------------------------------------
                case 'D':
                {
                        l_srvr->set_sock_opt_no_delay(false);
                        break;
                }
                // ---------------------------------------
                // timeout
                // ---------------------------------------
                case 'T':
                {
                        //NDBG_PRINT("arg: --fetches: %s\n", optarg);
                        //l_settings.m_end_type = END_FETCHES;
                        int l_subreq_timeout_s = atoi(optarg);
                        if (l_subreq_timeout_s < 1)
                        {
                                printf("timeout must be > 0\n");
                                //print_usage(stdout, -1);
                                return -1;
                        }
                        l_subr->set_timeout_ms(l_subreq_timeout_s*1000);
                        break;
                }
                // ---------------------------------------
                // No stats
                // ---------------------------------------
                case 'x':
                {
                        l_srvr->set_collect_stats(false);
                        break;
                }
                // ---------------------------------------
                // Rotate local address
                // ---------------------------------------
                case 'r':
                {
                        pthread_mutex_init(&g_addrx_mutex, NULL);

                        // Get value
                        int l_num = atoi(optarg);
                        if ((l_num < 1) || (l_num > (LOCAL_ADDR_V4_MAX - LOCAL_ADDR_V4_MIN)))
                        {
                                printf("timeout must be > 0 and less than local addr range: 127.0.0.0/8\n");
                                //print_usage(stdout, -1);
                                return -1;
                        }
                        g_addrx_addr_ipv4_max = LOCAL_ADDR_V4_MIN + l_num;
                        l_subr->set_pre_connect_cb(s_pre_connect_cb);
                        break;
                }
                // ---------------------------------------
                // verbose
                // ---------------------------------------
                case 'v':
                {
                        l_settings.m_verbose = true;
                        l_subr->set_save(true);
                        l_srvr->set_rqst_resp_logging(true);
                        break;
                }
                // ---------------------------------------
                // color
                // ---------------------------------------
                case 'c':
                {
                        l_settings.m_color = false;
                        l_srvr->set_rqst_resp_logging_color(false);
                        break;
                }
                // ---------------------------------------
                // quiet
                // ---------------------------------------
                case 'q':
                {
                        l_settings.m_quiet = true;
                        break;
                }
                // ---------------------------------------
                // responses
                // ---------------------------------------
                case 'C':
                {
                        l_settings.m_show_response_codes = true;
                        break;
                }
                // ---------------------------------------
                // per_interval
                // ---------------------------------------
                case 'L':
                {
                        l_settings.m_show_response_codes = true;
                        l_settings.m_show_per_interval = true;
                        break;
                }
                // ---------------------------------------
                // http_load
                // ---------------------------------------
                case 'Y':
                {
                        l_results_scheme = RESULTS_SCHEME_HTTP_LOAD;
                        break;
                }
                // ---------------------------------------
                // http_load_line
                // ---------------------------------------
                case 'Z':
                {
                        l_results_scheme = RESULTS_SCHEME_HTTP_LOAD_LINE;
                        break;
                }
                // ---------------------------------------
                // json
                // ---------------------------------------
                case 'j':
                {
                        l_results_scheme = RESULTS_SCHEME_JSON;
                        break;
                }
                // ---------------------------------------
                // output
                // ---------------------------------------
                case 'o':
                {
                        l_output_file = optarg;
                        break;
                }
                // ---------------------------------------
                // Data Port
                // ---------------------------------------
                case 'P':
                {
                        l_http_data_port = (uint16_t)atoi(optarg);
                        if (l_http_data_port < 1)
                        {
                                printf("Error: HTTP Data port must be > 0\n");
                                //print_usage(stdout, -1);
                                return -1;
                        }
                        break;
                }
                // ---------------------------------------
                // Update interval
                // ---------------------------------------
                case 'U':
                {
                        int l_interval_ms = atoi(optarg);
                        if (l_interval_ms < 1)
                        {
                                printf("Error: Update interval must be > 0 ms\n");
                                //print_usage(stdout, -1);
                                return -1;
                        }
                        l_settings.m_interval_ms = l_interval_ms;
                        break;
                }
                // ---------------------------------------
                // What???
                // ---------------------------------------
                case '?':
                {
                        // Required argument was missing
                        // '?' is provided when the 3rd arg to getopt_long does not begin with a ':', and is preceeded
                        // by an automatic error message.
                        fprintf(stdout, "  Exiting.\n");
                        print_usage(stdout, -1);
                        break;
                }
                // ---------------------------------------
                // Huh???
                // ---------------------------------------
                default:
                {
                        fprintf(stdout, "Unrecognized option.\n");
                        print_usage(stdout, -1);
                        break;
                }
                }
        }

        // Verify input
        if(!l_input_flag)
        {
                fprintf(stdout, "Error: Input url/url file/playback file required.");
                print_usage(stdout, -1);
        }

        // -------------------------------------------
        // Add url from command line
        // -------------------------------------------
        if(!l_url.length())
        {
                fprintf(stdout, "Error: No specified URL on cmd line.\n");
                print_usage(stdout, -1);
        }

        // -------------------------------------------------
        // Get resource limits
        // -------------------------------------------------
        int32_t l_s;
        struct rlimit l_rlim;
        errno = 0;
        l_s = getrlimit(RLIMIT_NOFILE, &l_rlim);
        if(l_s != 0)
        {
                fprintf(stdout, "Error performing getrlimit. Reason: %s\n", strerror(errno));
                return HLX_STATUS_ERROR;
        }
        if(l_rlim.rlim_cur < (uint64_t)(l_max_threads*l_num_parallel))
        {
                fprintf(stdout, "Error threads[%d]*parallelism[%d] > process fd resource limit[%u]\n",
                                l_max_threads, l_num_parallel, (uint32_t)l_rlim.rlim_cur);
                return HLX_STATUS_ERROR;
        }

        // -------------------------------------------
        // Sigint handler3
        // -------------------------------------------
        if (signal(SIGINT, sig_handler) == SIG_ERR)
        {
                printf("Error: can't catch SIGINT\n");
                return -1;
        }

        // -------------------------------------------
        // Add url from command line
        // -------------------------------------------
        //printf("Adding url: %s\n", l_url.c_str());
        // Set url
        int32_t l_status = 0;
        l_status = l_subr->init_with_url(l_url);
        if(l_status != 0)
        {
                printf("Error: performing init_with_url: %s\n", l_url.c_str());
                return -1;
        }
        l_subr->set_completion_cb(s_completion_cb);

        // -------------------------------------------
        // Paths...
        // -------------------------------------------
        std::string l_raw_path = l_subr->get_path();
        //printf("l_raw_path: %s\n",l_raw_path.c_str());
        if(l_wildcarding)
        {
                int32_t l_status;
                l_status = special_effects_parse(l_raw_path);
                if(l_status != HLX_STATUS_OK)
                {
                        printf("Error performing special_effects_parse with path: %s\n", l_raw_path.c_str());
                        return -1;
                }
                if(g_path_vector.size() > 1)
                {
                        l_subr->set_is_multipath(true);
                }
                else
                {
                        l_subr->set_is_multipath(false);
                }
        }
        else
        {
                g_path = l_raw_path;
        }

        // -------------------------------------------
        // Start api server
        // -------------------------------------------
        if(l_http_data_port > 0)
        {
                ns_hlx::lsnr *l_lsnr = new ns_hlx::lsnr(l_http_data_port, ns_hlx::SCHEME_TCP);
                ns_hlx::stat_h *l_stat_h = new ns_hlx::stat_h();
                int32_t l_status;
                l_status = l_lsnr->add_route("/", l_stat_h);
                if(l_status != 0)
                {
                        printf("Error: adding endpoint: %s\n", "/");
                        return -1;
                }
                l_srvr->register_lsnr(l_lsnr);
        }

#ifdef ENABLE_PROFILER
        // Start Profiler
        if (!l_gprof_file.empty())
                ProfilerStart(l_gprof_file.c_str());
#endif

        // Message
        if(!l_settings.m_quiet && !l_settings.m_verbose)
        {
                if(l_max_reqs_per_conn < 0)
                {
                        fprintf(stdout, "Running %d threads %d parallel connections per thread with infinite reqests per connection\n",
                                l_max_threads, l_num_parallel);
                }
                else
                {
                        fprintf(stdout, "Running %d threads %d parallel connections per thread with %d reqests per connection\n",
                                l_max_threads, l_num_parallel, l_max_reqs_per_conn);
                }
        }

        // -------------------------------------------
        //
        // -------------------------------------------
        l_srvr->set_update_stats_ms(l_settings.m_interval_ms);

        // -------------------------------------------
        // Setup to run -but don't start
        // -------------------------------------------
        l_status = l_srvr->init_run();
        if(HLX_STATUS_OK != l_status)
        {
                printf("Error: performing hlx::init_run\n");
                return -1;
        }

        // -------------------------------------------
        // Add subrequests
        // -------------------------------------------
        ns_hlx::srvr::t_srvr_list_t &l_t_srvr_list = l_srvr->get_t_srvr_list();
        uint32_t l_num_hlx = (uint32_t)l_t_srvr_list.size();
        uint32_t i_hlx_idx = 0;
        for(ns_hlx::srvr::t_srvr_list_t::iterator i_t = l_t_srvr_list.begin();
            i_t != l_t_srvr_list.end();
            ++i_t, ++i_hlx_idx)
        {
                if(!(*i_t))
                {
                        continue;
                }
                ns_hlx::subr *l_duped_subr = new ns_hlx::subr(*l_subr);
                l_duped_subr->set_uid(l_srvr->get_next_subr_uuid());

                // Recalculate num fetches per thread
                if(l_duped_subr->get_num_to_request() > 0)
                {
                        uint32_t l_num_fetches_per_thread =
                                        l_duped_subr->get_num_to_request() / l_num_hlx;
                        uint32_t l_remainder_fetches =
                                        l_duped_subr->get_num_to_request() % l_num_hlx;
                        if (i_hlx_idx == (l_num_hlx - 1))
                        {
                                l_num_fetches_per_thread += l_remainder_fetches;
                        }
                        //NDBG_PRINT("Num to fetch: %d\n", (int)l_num_fetches_per_thread);
                        l_duped_subr->set_num_to_request(l_num_fetches_per_thread);
                }
                if(l_duped_subr->get_num_to_request() != 0)
                {
                        //a_http_req.m_sr_child_list.push_back(&l_rqst);
                        l_status = add_subr_t_srvr(*i_t, *l_duped_subr);
                        if(l_status != HLX_STATUS_OK)
                        {
                                printf("Error: performing add_subr_t_srvr\n");
                                return -1;
                        }
                }
        }
        delete l_subr;

        // -------------------------------------------
        // Start
        // -------------------------------------------
        uint64_t l_start_time_ms = ns_hlx::get_time_ms();
        l_settings.m_start_time_ms = l_start_time_ms;
        l_status = l_srvr->run();
        if(HLX_STATUS_OK != l_status)
        {
                printf("Error: performing hlx::run");
                return -1;
        }

        // -------------------------------------------
        // Run command exec
        // -------------------------------------------
        command_exec(l_settings);
        uint64_t l_end_time_ms = ns_hlx::get_time_ms() - l_start_time_ms;

        // -------------------------------------------
        // Stop
        // -------------------------------------------
        usleep(l_settings.m_interval_ms*1000+1000);
        l_srvr->stop();
        l_srvr->wait_till_stopped();

#ifdef ENABLE_PROFILER
        if (!l_gprof_file.empty())
                ProfilerStop();
#endif

        std::string l_out_str;
        switch(l_results_scheme)
        {
        case RESULTS_SCHEME_STD:
        {
                get_results(l_settings, ((double)l_end_time_ms)/1000.0, l_out_str);
                break;
        }
        case RESULTS_SCHEME_JSON:
        {
                get_results_json(l_settings, ((double)l_end_time_ms)/1000.0, l_out_str);
                break;
        }
        case RESULTS_SCHEME_HTTP_LOAD:
        {
                get_results_http_load(l_settings, ((double)l_end_time_ms)/1000.0, l_out_str, false);
                break;
        }
        case RESULTS_SCHEME_HTTP_LOAD_LINE:
        {
                get_results_http_load(l_settings, ((double)l_end_time_ms)/1000.0, l_out_str, true);
                break;
        }
        default:
        {
                get_results(l_settings, ((double)l_end_time_ms)/1000.0, l_out_str);
                break;
        }
        }

        // -------------------------------------------
        // Write results...
        // -------------------------------------------
        if(l_output_file.empty())
        {
                if(!l_settings.m_verbose)
                {
                        printf("%s\n", l_out_str.c_str());
                }
        }
        else
        {
                int32_t l_num_bytes_written = 0;
                int32_t l_status = 0;
                // Open
                FILE *l_file_ptr = fopen(l_output_file.c_str(), "w+");
                if(l_file_ptr == NULL)
                {
                        printf("Error performing fopen. Reason: %s\n", strerror(errno));
                        return HLX_STATUS_ERROR;
                }

                // Write
                l_num_bytes_written = fwrite(l_out_str.c_str(), 1, l_out_str.length(), l_file_ptr);
                if(l_num_bytes_written != (int32_t)l_out_str.length())
                {
                        printf("Error performing fwrite. Reason: %s\n", strerror(errno));
                        fclose(l_file_ptr);
                        return HLX_STATUS_ERROR;
                }

                // Close
                l_status = fclose(l_file_ptr);
                if(l_status != 0)
                {
                        printf("Error performing fclose. Reason: %s\n", strerror(errno));
                        return HLX_STATUS_ERROR;
                }
        }

        // -------------------------------------------
        // Cleanup...
        // -------------------------------------------
        if(l_srvr)
        {
                delete l_srvr;
                l_srvr = NULL;
        }
        //if(l_settings.m_verbose)
        //{mak
        //      NDBG_PRINT("Cleanup\n");
        //}
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: Portable gettime function
//: \return:  NA
//: \param:   ao_timespec: struct timespec -with gettime result
//: ----------------------------------------------------------------------------
static void _rt_gettime(struct timespec &ao_timespec)
{
#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
        clock_serv_t l_cclock;
        mach_timespec_t l_mts;
        host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &l_cclock);
        clock_get_time(l_cclock, &l_mts);
        mach_port_deallocate(mach_task_self(), l_cclock);
        ao_timespec.tv_sec = l_mts.tv_sec;
        ao_timespec.tv_nsec = l_mts.tv_nsec;
// TODO -if __linux__
#else
        clock_gettime(CLOCK_REALTIME, &ao_timespec);
#endif
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t hurl_get_time_ms(void)
{
        uint64_t l_retval;
        struct timespec l_timespec;
        _rt_gettime(l_timespec);    
        l_retval = (((uint64_t)l_timespec.tv_sec) * 1000) + (((uint64_t)l_timespec.tv_nsec) / 1000000);
        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t hurl_get_delta_time_ms(uint64_t a_start_time_ms)
{
        uint64_t l_retval;
        struct timespec l_timespec;
        _rt_gettime(l_timespec);    
        l_retval = (((uint64_t)l_timespec.tv_sec) * 1000) + (((uint64_t)l_timespec.tv_nsec) / 1000000);
        return l_retval - a_start_time_ms;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t hurl_get_time_us(void)
{
        uint64_t l_retval;
        struct timespec l_timespec;
        _rt_gettime(l_timespec);
        l_retval = (((uint64_t)l_timespec.tv_sec) * 1000000) + (((uint64_t)l_timespec.tv_nsec) / 1000);
        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void display_responses_line_desc(settings_struct &a_settings)
{
        printf("+-----------+-------------+-----------+-----------+-----------+-----------+-----------+-----------+\n");
        if(a_settings.m_show_per_interval)
        {
                if(a_settings.m_color)
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
                if(a_settings.m_color)
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
static void get_status_codes(settings_struct &a_settings,
                             ns_hlx::status_code_count_map_t &ao_map)
{
        ns_hlx::srvr::t_srvr_list_t &l_t_srvr_list = a_settings.m_srvr->get_t_srvr_list();
        for(ns_hlx::srvr::t_srvr_list_t::iterator i_t = l_t_srvr_list.begin(); i_t != l_t_srvr_list.end(); ++i_t)
        {
                ns_hlx::status_code_count_map_t i_m;
                (*i_t)->get_ups_status_code_count_map(i_m);
                for(ns_hlx::status_code_count_map_t::iterator i_c = i_m.begin(); i_c != i_m.end(); ++i_c)
                {
                        ao_map[i_c->first] += i_c->second;
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void display_responses_line(settings_struct &a_settings)
{
        // Get stats
        ns_hlx::t_stat_cntr_t l_total;
        ns_hlx::t_stat_calc_t l_total_calc;
        ns_hlx::t_stat_cntr_list_t l_thread;
        a_settings.m_srvr->get_stat(l_total, l_total_calc, l_thread, true);
        if(a_settings.m_show_per_interval)
        {
                if(a_settings.m_color)
                {
                                printf("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %s%9.2f%s | %s%9.2f%s | %s%9.2f%s | %s%9.2f%s |\n",
                                                ((double)(hurl_get_delta_time_ms(a_settings.m_start_time_ms))) / 1000.0,
                                                l_total_calc.m_upsv_req_s,
                                                l_total.m_upsv_resp,
                                                l_total.m_upsv_errors,
                                                ANSI_COLOR_FG_GREEN, l_total_calc.m_upsv_resp_status_2xx_pcnt, ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_CYAN, l_total_calc.m_upsv_resp_status_3xx_pcnt, ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_MAGENTA, l_total_calc.m_upsv_resp_status_4xx_pcnt, ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_RED, l_total_calc.m_upsv_resp_status_5xx_pcnt, ANSI_COLOR_OFF);
                }
                else
                {
                        printf("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %9.2f | %9.2f | %9.2f | %9.2f |\n",
                                        ((double)(hurl_get_delta_time_ms(a_settings.m_start_time_ms))) / 1000.0,
                                        l_total_calc.m_upsv_req_s,
                                        l_total.m_upsv_resp,
                                        l_total.m_upsv_errors,
                                        l_total_calc.m_upsv_resp_status_2xx_pcnt,
                                        l_total_calc.m_upsv_resp_status_3xx_pcnt,
                                        l_total_calc.m_upsv_resp_status_4xx_pcnt,
                                        l_total_calc.m_upsv_resp_status_5xx_pcnt);
                }
        }
        else
        {
                // Aggregate over status code map
                uint32_t l_responses[10] = {0};
                ns_hlx::status_code_count_map_t l_status_code_count_map;
                get_status_codes(a_settings, l_status_code_count_map);
                for(ns_hlx::status_code_count_map_t::iterator i_code = l_status_code_count_map.begin();
                    i_code != l_status_code_count_map.end();
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
                if(a_settings.m_color)
                {
                                printf("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %s%9u%s | %s%9u%s | %s%9u%s | %s%9u%s |\n",
                                                ((double)(hurl_get_delta_time_ms(a_settings.m_start_time_ms))) / 1000.0,
                                                l_total_calc.m_upsv_req_s,
                                                l_total.m_upsv_resp,
                                                l_total.m_upsv_errors,
                                                ANSI_COLOR_FG_GREEN, l_responses[2], ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_CYAN, l_responses[3], ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_MAGENTA, l_responses[4], ANSI_COLOR_OFF,
                                                ANSI_COLOR_FG_RED, l_responses[5], ANSI_COLOR_OFF);
                }
                else
                {
                        printf("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %9u | %9u | %9u | %9u |\n",
                                        ((double)(hurl_get_delta_time_ms(a_settings.m_start_time_ms))) / 1000.0,
                                        l_total_calc.m_upsv_req_s,
                                        l_total.m_upsv_resp,
                                        l_total.m_upsv_errors,
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
void display_results_line_desc(settings_struct &a_settings)
{
        printf("+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+\n");
        if(a_settings.m_color)
        {
        printf("| %s%9s%s / %s%9s%s | %s%9s%s | %s%9s%s | %s%12s%s | %9s | %11s | %9s |\n",
                        ANSI_COLOR_FG_GREEN, "Completed", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_BLUE, "Requested", ANSI_COLOR_OFF,
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
                                "Completed",
                                "Requested",
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
void display_results_line(settings_struct &a_settings)
{
        // Get stats
        ns_hlx::t_stat_cntr_t l_total;
        ns_hlx::t_stat_calc_t l_total_calc;
        ns_hlx::t_stat_cntr_list_t l_thread;
        a_settings.m_srvr->get_stat(l_total, l_total_calc, l_thread, true);
        if(a_settings.m_color)
        {
                        printf("| %s%9" PRIu64 "%s / %s%9" PRIi64 "%s | %s%9" PRIu64 "%s | %s%9" PRIu64 "%s | %s%12.2f%s | %8.2fs | %10.2fs | %8.2fs |\n",
                                        ANSI_COLOR_FG_GREEN, l_total.m_upsv_resp, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_BLUE, l_total.m_upsv_reqs, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_MAGENTA, l_total.m_upsv_idle_killed, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_RED, l_total.m_upsv_errors, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_YELLOW, l_total_calc.m_upsv_bytes_read_s/1024.0, ANSI_COLOR_OFF,
                                        ((double)(hurl_get_delta_time_ms(a_settings.m_start_time_ms))) / 1000.0,
                                        l_total_calc.m_upsv_req_s,
                                        l_total_calc.m_upsv_bytes_read_s/(1024.0*1024.0)
                                        );
        }
        else
        {
                printf("| %9" PRIu64 " / %9" PRIi64 " | %9" PRIu64 " | %9" PRIu64 " | %12.2f | %8.2fs | %10.2fs | %8.2fs |\n",
                                l_total.m_upsv_resp,
                                l_total.m_upsv_reqs,
                                l_total.m_upsv_idle_killed,
                                l_total.m_upsv_errors,
                                ((double)(l_total.m_upsv_bytes_read))/(1024.0),
                                ((double)(hurl_get_delta_time_ms(a_settings.m_start_time_ms)) / 1000.0),
                                l_total_calc.m_upsv_req_s,
                                l_total_calc.m_upsv_bytes_read_s/(1024.0*1024.0)
                                );
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define STR_PRINT(...) \
        do { \
                snprintf(l_buf,1024,__VA_ARGS__);\
                ao_results+=l_buf;\
        } while(0)

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void get_results(settings_struct &a_settings,
                 double a_elapsed_time,
                 std::string &ao_results)
{
        // Get stats
        ns_hlx::t_stat_cntr_t l_total;
        ns_hlx::t_stat_calc_t l_total_calc;
        ns_hlx::t_stat_cntr_list_t l_thread;
        a_settings.m_srvr->get_stat(l_total, l_total_calc, l_thread, true);

        std::string l_tag;
        char l_buf[1024];
        // TODO Fix elapse and max parallel
        l_tag = "ALL";
        uint64_t l_total_bytes = l_total.m_upsv_bytes_read + l_total.m_upsv_bytes_written;
        if(a_settings.m_color)
        {
        STR_PRINT("| %sRESULTS%s:             %s%s%s\n", ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF, ANSI_COLOR_FG_YELLOW, l_tag.c_str(), ANSI_COLOR_OFF);
        }
        else
        {
        STR_PRINT("| RESULTS:             %s\n", l_tag.c_str());
        }
        STR_PRINT("| fetches:             %" PRIu64 "\n", l_total.m_upsv_resp);
        STR_PRINT("| max parallel:        %u\n", a_settings.m_num_parallel);
        STR_PRINT("| bytes:               %e\n", (double)(l_total_bytes));
        STR_PRINT("| seconds:             %f\n", a_elapsed_time);
        STR_PRINT("| mean bytes/conn:     %f\n", ((double)l_total_bytes)/((double)l_total.m_upsv_resp));
        STR_PRINT("| fetches/sec:         %f\n", ((double)l_total.m_upsv_resp)/(a_elapsed_time));
        STR_PRINT("| bytes/sec:           %e\n", ((double)l_total_bytes)/a_elapsed_time);
        // TODO Fix stdev/var calcs
#if 0
#define SHOW_XSTAT_LINE(_tag, stat)\
        do {\
        STR_PRINT("| %-16s %12.6f mean, %12.6f max, %12.6f min, %12.6f stdev, %12.6f var\n",\
               _tag,\
               stat.mean()/1000.0,\
               stat.max()/1000.0,\
               stat.min()/1000.0,\
               stat.stdev()/1000.0,\
               stat.var()/1000.0);\
        } while(0)
#else
#define SHOW_XSTAT_LINE(_tag, stat)\
        do {\
        STR_PRINT("| %-16s %12.6f mean, %12.6f max, %12.6f min\n",\
               _tag,\
               stat.mean()/1000.0,\
               stat.max()/1000.0,\
               stat.min()/1000.0);\
        } while(0)
#endif
        SHOW_XSTAT_LINE("ms/connect:", l_total.m_upsv_stat_us_connect);
        SHOW_XSTAT_LINE("ms/1st-response:", l_total.m_upsv_stat_us_first_response);
        SHOW_XSTAT_LINE("ms/end2end:", l_total.m_upsv_stat_us_end_to_end);

        if(a_settings.m_color)
        {
        STR_PRINT("| %sHTTP response codes%s: \n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF);
        }
        else
        {
        STR_PRINT("| HTTP response codes: \n");
        }
        ns_hlx::status_code_count_map_t l_status_code_count_map;
        get_status_codes(a_settings, l_status_code_count_map);
        for(ns_hlx::status_code_count_map_t::const_iterator i_status_code = l_status_code_count_map.begin();
            i_status_code != l_status_code_count_map.end();
            ++i_status_code)
        {
                if(a_settings.m_color)
                {
                STR_PRINT("| %s%3d%s -- %u\n", ANSI_COLOR_FG_MAGENTA, i_status_code->first, ANSI_COLOR_OFF, i_status_code->second);
                }
                else
                {
                STR_PRINT("| %3d -- %u\n", i_status_code->first, i_status_code->second);
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void get_results_http_load(settings_struct &a_settings,
                           double a_elapsed_time,
                           std::string &ao_results,
                           bool a_one_line_flag)
{
        // Get stats
        ns_hlx::t_stat_cntr_t l_total;
        ns_hlx::t_stat_calc_t l_total_calc;
        ns_hlx::t_stat_cntr_list_t l_thread;
        a_settings.m_srvr->get_stat(l_total, l_total_calc, l_thread, true);

        std::string l_tag;
        // Separator
        std::string l_sep = "\n";
        if(a_one_line_flag) l_sep = "||";

        // TODO Fix elapse and max parallel
        l_tag = "State";
        char l_buf[1024];
        uint64_t l_total_bytes = l_total.m_upsv_bytes_read + l_total.m_upsv_bytes_written;
        STR_PRINT("%s: ", l_tag.c_str());
        STR_PRINT("%" PRIu64 " fetches, ", l_total.m_upsv_resp);
        STR_PRINT("%u max parallel, ", a_settings.m_num_parallel);
        STR_PRINT("%e bytes, ", (double)(l_total_bytes));
        STR_PRINT("in %f seconds, ", a_elapsed_time);
        STR_PRINT("%s", l_sep.c_str());
        STR_PRINT("%f mean bytes/connection, ", ((double)l_total_bytes)/((double)l_total.m_upsv_resp));
        STR_PRINT("%s", l_sep.c_str());
        STR_PRINT("%f fetches/sec, %e bytes/sec", ((double)l_total.m_upsv_resp)/(a_elapsed_time), ((double)l_total_bytes)/a_elapsed_time);
        STR_PRINT("%s", l_sep.c_str());
#define SHOW_XSTAT_LINE_LEGACY(_tag, stat)\
        STR_PRINT("%s %.6f mean, %.6f max, %.6f min, %.6f stdev",\
               _tag,                                          \
               stat.mean()/1000.0,                            \
               stat.max()/1000.0,                             \
               stat.min()/1000.0,                             \
               stat.stdev()/1000.0);                          \
        printf("%s", l_sep.c_str())
        SHOW_XSTAT_LINE_LEGACY("msecs/connect:", l_total.m_upsv_stat_us_connect);
        SHOW_XSTAT_LINE_LEGACY("msecs/first-response:", l_total.m_upsv_stat_us_first_response);
        SHOW_XSTAT_LINE_LEGACY("msecs/end2end:", l_total.m_upsv_stat_us_end_to_end);
        STR_PRINT("HTTP response codes: ");
        if(l_sep == "\n")
        {
        STR_PRINT("%s", l_sep.c_str());
        }
        ns_hlx::status_code_count_map_t l_status_code_count_map;
        get_status_codes(a_settings, l_status_code_count_map);
        for(ns_hlx::status_code_count_map_t::const_iterator i_status_code = l_status_code_count_map.begin();
            i_status_code != l_status_code_count_map.end();
            ++i_status_code)
        {
        STR_PRINT("code %d -- %u, ", i_status_code->first, i_status_code->second);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void get_results_json(settings_struct &a_settings,
                      double a_elapsed_time,
                      std::string &ao_results)
{
        // Get stats
        ns_hlx::t_stat_cntr_t l_total;
        ns_hlx::t_stat_calc_t l_total_calc;
        ns_hlx::t_stat_cntr_list_t l_thread;
        a_settings.m_srvr->get_stat(l_total, l_total_calc, l_thread, true);

        uint64_t l_total_bytes = l_total.m_upsv_bytes_read + l_total.m_upsv_bytes_written;
        rapidjson::Document l_body;
        l_body.SetObject();
        rapidjson::Document::AllocatorType& l_alloc = l_body.GetAllocator();
#define ADD_MEMBER(_l, _v) \
        l_body.AddMember(_l, _v, l_alloc)

        ADD_MEMBER("fetches", l_total.m_upsv_resp);
        ADD_MEMBER("max-parallel", a_settings.m_num_parallel);
        ADD_MEMBER("bytes", (double)(l_total_bytes));
        ADD_MEMBER("seconds", a_elapsed_time);
        ADD_MEMBER("mean-bytes-per-conn", ((double)l_total_bytes)/((double)l_total.m_upsv_resp));
        ADD_MEMBER("fetches-per-sec", ((double)l_total.m_upsv_resp)/(a_elapsed_time));
        ADD_MEMBER("bytes-per-sec", ((double)l_total_bytes)/a_elapsed_time);

        // TODO Fix stdev/var calcs
        // Stats
        ADD_MEMBER("connect-ms-mean", l_total.m_upsv_stat_us_connect.mean()/1000.0);
        ADD_MEMBER("connect-ms-max", l_total.m_upsv_stat_us_connect.max()/1000.0);
        ADD_MEMBER("connect-ms-min", l_total.m_upsv_stat_us_connect.min()/1000.0);

        ADD_MEMBER("1st-resp-ms-mean", l_total.m_upsv_stat_us_first_response.mean()/1000.0);
        ADD_MEMBER("1st-resp-ms-max", l_total.m_upsv_stat_us_first_response.max()/1000.0);
        ADD_MEMBER("1st-resp-ms-min", l_total.m_upsv_stat_us_first_response.min()/1000.0);

        ADD_MEMBER("end2end-ms-mean", l_total.m_upsv_stat_us_end_to_end.mean()/1000.0);
        ADD_MEMBER("end2end-ms-max", l_total.m_upsv_stat_us_end_to_end.max()/1000.0);
        ADD_MEMBER("end2end-ms-min", l_total.m_upsv_stat_us_end_to_end.min()/1000.0);

        ns_hlx::status_code_count_map_t l_status_code_count_map;
        get_status_codes(a_settings, l_status_code_count_map);
        if(l_status_code_count_map.size())
        {
        rapidjson::Value l_obj;
        l_obj.SetObject();
        for(ns_hlx::status_code_count_map_t::const_iterator i_status_code = l_status_code_count_map.begin();
            i_status_code != l_status_code_count_map.end();
            ++i_status_code)
        {
                char l_buf[16]; snprintf(l_buf,16,"%3d",i_status_code->first);
                l_obj.AddMember(rapidjson::Value(l_buf, l_alloc).Move(), i_status_code->second, l_alloc);
        }
        l_body.AddMember("response-codes", l_obj, l_alloc);
        }
        rapidjson::StringBuffer l_strbuf;
        rapidjson::Writer<rapidjson::StringBuffer> l_writer(l_strbuf);
        l_body.Accept(l_writer);
        ao_results.assign(l_strbuf.GetString(), l_strbuf.GetSize());
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t read_file(const char *a_file, char **a_buf, uint32_t *a_len)
{
        // Check is a file
        struct stat l_stat;
        int32_t l_status = HLX_STATUS_OK;
        l_status = stat(a_file, &l_stat);
        if(l_status != 0)
        {
                printf("Error performing stat on file: %s.  Reason: %s\n", a_file, strerror(errno));
                return -1;
        }

        // Check if is regular file
        if(!(l_stat.st_mode & S_IFREG))
        {
                printf("Error opening file: %s.  Reason: is NOT a regular file\n", a_file);
                return -1;
        }

        // Open file...
        FILE * l_file;
        l_file = fopen(a_file,"r");
        if (NULL == l_file)
        {
                printf("Error opening file: %s.  Reason: %s\n", a_file, strerror(errno));
                return -1;
        }

        // Read in file...
        int32_t l_size = l_stat.st_size;
        *a_buf = (char *)malloc(sizeof(char)*l_size);
        *a_len = l_size;
        int32_t l_read_size;
        l_read_size = fread(*a_buf, 1, l_size, l_file);
        if(l_read_size != l_size)
        {
                printf("Error performing fread.  Reason: %s [%d:%d]\n",
                                strerror(errno), l_read_size, l_size);
                return -1;
        }

        // Close file...
        l_status = fclose(l_file);
        if (HLX_STATUS_OK != l_status)
        {
                printf("Error performing fclose.  Reason: %s\n", strerror(errno));
                return -1;
        }
        return 0;
}
