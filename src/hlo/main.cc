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
#include "hlx/hlx.h"
#include "tinymt64.h"
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
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

// Profiler
#define ENABLE_PROFILER 1
#ifdef ENABLE_PROFILER
#include <google/profiler.h>
#endif

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define UNUSED(x) ( (void)(x) )

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
// Version
#define HLO_VERSION_MAJOR 0
#define HLO_VERSION_MINOR 0
#define HLO_VERSION_MACRO 1
#define HLO_VERSION_PATCH "alpha"

//: ----------------------------------------------------------------------------
//: Status
//: ----------------------------------------------------------------------------
#ifndef STATUS_ERROR
#define STATUS_ERROR -1
#endif

#ifndef STATUS_OK
#define STATUS_OK 0
#endif

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
//: Handler
//: ----------------------------------------------------------------------------
class stats_getter: public ns_hlx::default_http_request_handler
{
public:
        // GET
        int32_t do_get(const ns_hlx::url_param_map_t &a_url_param_map,
                       ns_hlx::http_req &a_request,
                       ns_hlx::http_resp &ao_response)
        {
                // Process request
                if(!m_hlx)
                {
                        return -1;
                }
                char l_char_buf[2048];
                m_hlx->get_stats_json(l_char_buf, 2048);

                char l_len_str[64];
                uint32_t l_body_len = strlen(l_char_buf);
                sprintf(l_len_str, "%u", l_body_len);

                ao_response.write_status(ns_hlx::HTTP_STATUS_OK);
                ao_response.write_header("Content-Type", "application/json");
                ao_response.write_header("Access-Control-Allow-Origin", "*");
                ao_response.write_header("Access-Control-Allow-Credentials", "true");
                ao_response.write_header("Access-Control-Max-Age", "86400");
                ao_response.write_header("Content-Length", l_len_str);
                ao_response.write_body(l_char_buf, l_body_len);

                return STATUS_OK;
        }
        // hlx client
        ns_hlx::hlx *m_hlx;
        stats_getter(void):
                m_hlx(NULL)
        {};
private:
        // Disallow copy/assign
        stats_getter& operator=(const stats_getter &);
        stats_getter(const stats_getter &);
};

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
        uint32_t m_num_parallel;
        ns_hlx::hlx *m_hlx;
        uint64_t m_start_time_ms;
        uint64_t m_last_display_time_ms;
        ns_hlx::t_stat_t *m_last_stat;
        int32_t m_run_time_ms;
        uint32_t m_last_responses_count[10];

        // ---------------------------------
        // Defaults...
        // ---------------------------------
        settings_struct() :
                m_verbose(false),
                m_color(false),
                m_quiet(false),
                m_show_response_codes(false),
                m_show_per_interval(false),
                m_num_parallel(1),
                m_hlx(NULL),
                m_start_time_ms(),
                m_last_display_time_ms(),
                m_last_stat(NULL),
                m_run_time_ms(-1),
                m_last_responses_count()
        {
                m_last_stat = new ns_hlx::t_stat_struct();
                for(uint32_t i = 0; i < 10; ++i) {m_last_responses_count[i] = 0;}
        }

        ~settings_struct()
        {
                if(m_last_stat)
                {
                        delete m_last_stat;
                        m_last_stat = NULL;
                }
        }
private:
        // Disallow copy/assign
        settings_struct& operator=(const settings_struct &);
        settings_struct(const settings_struct &);
} settings_struct_t;

//: ----------------------------------------------------------------------------
//: Prototypes
//: ----------------------------------------------------------------------------
uint64_t hlo_get_time_us(void);
uint64_t hlo_get_time_ms(void);
uint64_t hlo_get_delta_time_ms(uint64_t a_start_time_ms);
void display_results_line_desc(settings_struct &a_settings);
void display_results_line(settings_struct &a_settings);
void display_responses_line_desc(settings_struct &a_settings);
void display_responses_line(settings_struct &a_settings);
void display_results(settings_struct &a_settings, double a_elapsed_time);
void display_results_http_load_style(settings_struct &a_settings,
                                     double a_elapsed_time,
                                     bool a_one_line_flag = false);
// Specifying urls instead of hosts
int32_t add_url(ns_hlx::hlx *a_hlx, ns_hlx::subreq *a_subreq, const std::string &a_url);
int32_t read_file(const char *a_file, char **a_buf, uint32_t *a_len);

//: ----------------------------------------------------------------------------
//: Globals
//: ----------------------------------------------------------------------------
static ns_hlx::hlx *g_hlx = NULL;
static bool g_test_finished = false;
static bool g_cancelled = false;
static settings_struct_t *g_settings = NULL;
static uint64_t g_rate_delta_us = 0;
static uint32_t g_num_threads = 4;
static int32_t g_num_to_request = -1;
static uint32_t g_num_requested = 0;
static uint32_t g_num_completed = 0;

// Path vector support
static tinymt64_t *g_rand_ptr = NULL;
static path_vector_t g_path_vector;
static std::string g_path;
static uint32_t g_path_vector_last_idx = 0;
static path_order_t g_path_order = EXPLODED_PATH_ORDER_RANDOM;
static pthread_mutex_t g_path_vector_mutex;

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
                g_settings->m_hlx->stop();
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
        struct timeval tv;
        fd_set fds;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        //STDIN_FILENO is 0
        select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        return FD_ISSET(STDIN_FILENO, &fds);
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
        bool l_sent_stop = false;
        ns_hlx::hlx *l_hlx = a_settings.m_hlx;
        bool l_first_time = true;

        nonblock(NB_ENABLE);

        //: ------------------------------------
        //:   Loop forever until user quits
        //: ------------------------------------
        while (!g_test_finished)
        {
                //NDBG_PRINT("BOOP.\n");

                i = kbhit();
                if (i != 0)
                {

                        l_cmd = fgetc(stdin);

                        switch (l_cmd)
                        {

                        // Display
                        case 'p':
                                //a_scanner->display_collection_stats();
                                break;

                                //Quit
                        case 'q':
                                g_test_finished = true;
                                l_hlx->stop();
                                l_sent_stop = true;
                                break;

                                //Toggle pause
                        case 'P':
                                //test_pause = 1 - test_pause;
                                break;

                                // Default
                        default:
                                break;
                        }
                }

                // TODO add define...
                usleep(500000);

                // Check for done
                if(a_settings.m_run_time_ms != -1)
                {
                        int32_t l_time_delta_ms = (int32_t)(hlo_get_delta_time_ms(a_settings.m_start_time_ms));
                        if(l_time_delta_ms >= a_settings.m_run_time_ms)
                        {
                                g_test_finished = true;
                                l_hlx->stop();
                                l_sent_stop = true;
                        }
                }

                if(!a_settings.m_quiet && !a_settings.m_verbose)
                {
                        if(a_settings.m_show_response_codes)
                        {
                                if(l_first_time)
                                {
                                        display_responses_line_desc(a_settings);
                                        l_first_time = false;
                                }
                                display_responses_line(a_settings);
                        }
                        else
                        {
                                if(l_first_time)
                                {
                                        display_results_line_desc(a_settings);
                                        l_first_time = false;
                                }
                                display_results_line(a_settings);
                        }
                }

                if (!l_hlx->is_running())
                {
                        g_test_finished = true;
                }

        }

        // Send stop -if unsent
        if(!l_sent_stop)
        {
                l_hlx->stop();
                l_sent_stop = true;
        }
        nonblock(NB_DISABLE);
}

//: ----------------------------------------------------------------------------
//: \details: Completion callback
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t s_completion_cb(void *a_ptr)
{
        ++g_num_completed;
        if((g_num_to_request != -1) && (g_num_completed >= (uint32_t)g_num_to_request))
        {
                g_test_finished = true;
                g_settings->m_hlx->stop();
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
                printf("STATUS_ERROR: Bad range start[%u] > end[%u]\n", l_start, l_end);
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
                        printf("STATUS_ERROR: Bad range for path: %s at pos: %lu\n", a_path, l_range_start_pos);
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
                        printf("STATUS_ERROR: performing convert_exp_to_range(%s)\n", l_range_exp.c_str());
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
                printf("SUBSTR: %s\n", i_substr->c_str());
        }

        for(range_vector_t::iterator i_range = ao_range_vector.begin();
                        i_range != ao_range_vector.end();
                        ++i_range)
        {
                printf("RANGE: %u -- %u\n", i_range->m_start, i_range->m_end);
        }
#endif
        return STATUS_OK;
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
                return STATUS_OK;
        }

        a_path_part.append(a_substr_vector[a_substr_idx]);
        ++a_substr_idx;

        if(a_range_idx >= a_range_vector.size())
        {
                g_path_vector.push_back(a_path_part);
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
                        printf("STATUS_ERROR: Bad value[%s] for key[%s]\n", l_val.c_str(), l_key.c_str());
                        return STATUS_ERROR;
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
                        NDBG_PRINT("STATUS_ERROR: Bad value[%s] for key[%s]\n", l_val.c_str(), l_key.c_str());
                        return STATUS_ERROR;
                }
        }
#endif
        else
        {
                printf("STATUS_ERROR: Unrecognized key[%s]\n", l_key.c_str());
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
                return STATUS_OK;
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
                        if(l_status != STATUS_OK)
                        {
                                printf("STATUS_ERROR: Performing parse_path(%s)\n", l_p);
                                return STATUS_ERROR;
                        }
                }

                // If empty path explode
                if(l_range_vector.size())
                {
                        l_status = path_exploder(std::string(""), l_path_substr_vector, 0, l_range_vector, 0);
                        if(l_status != STATUS_OK)
                        {
                                printf("STATUS_ERROR: Performing explode_path(%s)\n", l_p);
                                return STATUS_ERROR;
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
                        if(l_add_option_status != STATUS_OK)
                        {
                                printf("STATUS_ERROR: Performing add_option(%s,%s)\n", l_k,l_v);
                                // Nuttin doing???

                        }
                        //printf("Key: %s\n", l_k);
                        //printf("Val: %s\n", l_v);
                }
                l_p = strtok_r(NULL, SPECIAL_EFX_OPT_SEPARATOR, &l_save_ptr);
        }
        //printf("a_path: %s\n", a_path.c_str());
        return STATUS_OK;
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
static int32_t s_create_request_cb(ns_hlx::subreq *a_subreq, ns_hlx::http_req *a_req)
{
        if((g_num_to_request != -1) && (g_num_requested >= (uint32_t)g_num_to_request))
        {
                return STATUS_ERROR;
        }
        ++g_num_requested;

        // TODO grab from path...
        std::string l_path_ref = get_path(g_rand_ptr);

        char l_buf[2048];
        int32_t l_len = 0;
        if(l_path_ref.empty())
        {
                l_path_ref = "/";
        }
        if(!(a_subreq->m_query.empty()))
        {
                l_path_ref += "?";
                l_path_ref += a_subreq->m_query;
        }
        //NDBG_PRINT("HOST: %s PATH: %s\n", a_reqlet.m_url.m_host.c_str(), l_path_ref.c_str());
        l_len = snprintf(l_buf, sizeof(l_buf),
                        "%s %.500s HTTP/1.1", a_subreq->m_verb.c_str(), l_path_ref.c_str());

        a_req->write_request_line(l_buf, l_len);

        // -------------------------------------------
        // Add repo headers
        // -------------------------------------------
        bool l_specd_host = false;

        // Loop over reqlet map
        for(ns_hlx::kv_map_list_t::const_iterator i_hl = a_subreq->m_headers.begin();
            i_hl != a_subreq->m_headers.end();
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
                        a_req->write_header(i_hl->first.c_str(), i_v->c_str());
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
                a_req->write_header("Host", a_subreq->m_host.c_str());
        }

        // -------------------------------------------
        // body
        // -------------------------------------------
        if(a_subreq->m_body_data && a_subreq->m_body_data_len)
        {
                //NDBG_PRINT("Write: buf: %p len: %d\n", l_buf, l_len);
                a_req->write_body(a_subreq->m_body_data, a_subreq->m_body_data_len);
        }
        else
        {
                a_req->write_body(NULL, 0);
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: Print the version.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_version(FILE* a_stream, int a_exit_code)
{

        // print out the version information
        fprintf(a_stream, "hlo HTTP Load Tester.\n");
        fprintf(a_stream, "Copyright (C) 2014 Edgecast Networks.\n");
        fprintf(a_stream, "               Version: %d.%d.%d.%s\n",
                        HLO_VERSION_MAJOR,
                        HLO_VERSION_MINOR,
                        HLO_VERSION_MACRO,
                        HLO_VERSION_PATCH);
        exit(a_exit_code);
}

//: ----------------------------------------------------------------------------
//: \details: Print the command line help.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_usage(FILE* a_stream, int a_exit_code)
{
        fprintf(a_stream, "Usage: hlo [http[s]://]hostname[:port]/path [options]\n");
        fprintf(a_stream, "Options are:\n");
        fprintf(a_stream, "  -h, --help          Display this help and exit.\n");
        fprintf(a_stream, "  -r, --version       Display the version number and exit.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Input Options:\n");
        fprintf(a_stream, "  -w, --no_wildcards  Don't wildcard the url.\n");
        fprintf(a_stream, "  -M, --mode          Request mode -if multipath [random(default) | sequential].\n");
        fprintf(a_stream, "  -d, --data          HTTP body data -supports bodies up to 8k.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Settings:\n");
        fprintf(a_stream, "  -y, --cipher        Cipher --see \"openssl ciphers\" for list.\n");
        fprintf(a_stream, "  -p, --parallel      Num parallel. Default: 1.\n");
        fprintf(a_stream, "  -f, --fetches       Num fetches.\n");
        fprintf(a_stream, "  -N, --num_calls     Number of requests per connection\n");
        fprintf(a_stream, "  -k, --keep_alive    Re-use connections for all requests\n");
        fprintf(a_stream, "  -t, --threads       Number of parallel threads. Default: 1\n");
        fprintf(a_stream, "  -H, --header        Request headers -can add multiple ie -H<> -H<>...\n");
        fprintf(a_stream, "  -X, --verb          Request command -HTTP verb to use -GET/PUT/etc. Default GET\n");
        fprintf(a_stream, "  -l, --seconds       Run for <N> seconds .\n");
        fprintf(a_stream, "  -A, --rate          Max Request Rate.\n");
        fprintf(a_stream, "  -R, --recv_buffer   Socket receive buffer size.\n");
        fprintf(a_stream, "  -S, --send_buffer   Socket send buffer size.\n");
        fprintf(a_stream, "  -D, --no_delay      Socket TCP no-delay.\n");
        fprintf(a_stream, "  -T, --timeout       Timeout (seconds).\n");
        fprintf(a_stream, "  -x, --no_stats      Don't collect stats -faster.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Print Options:\n");
        fprintf(a_stream, "  -v, --verbose       Verbose logging\n");
        fprintf(a_stream, "  -c, --color         Color\n");
        fprintf(a_stream, "  -q, --quiet         Suppress progress output\n");
        fprintf(a_stream, "  -C, --responses     Display http(s) response codes instead of request statistics\n");
        fprintf(a_stream, "  -L, --responses_per Display http(s) response codes per interval instead of request statistics\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Stat Options:\n");
        fprintf(a_stream, "  -P, --data_port     Start HTTP Stats Daemon on port.\n");
        fprintf(a_stream, "  -Y, --http_load     Display in http load mode [MODE] -Legacy support\n");
        fprintf(a_stream, "  -G, --gprofile      Google profiler output file\n");
        fprintf(a_stream, "  \n");
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
        // Get hlo instance
        settings_struct_t l_settings;
        ns_hlx::hlx *l_hlx = new ns_hlx::hlx();
        l_settings.m_hlx = l_hlx;
        g_hlx = l_hlx;
        // For sighandler
        g_settings = &l_settings;

        // hlx settings
        l_hlx->set_split_requests_by_thread(false);
        l_hlx->set_collect_stats(true);
        l_hlx->set_use_ai_cache(true);
        l_hlx->set_num_threads(1);
        l_hlx->set_num_parallel(1);

        int32_t l_http_load_display = -1;
        int32_t l_http_data_port = -1;

        int l_max_threads = 1;
        // TODO Make default definitions
        int l_start_parallel = 1;
        int l_max_reqs_per_conn = 1;
        bool l_input_flag = false;
        bool l_wildcarding = true;
        UNUSED(l_wildcarding);

        // -------------------------------------------------
        // Subrequest settings
        // -------------------------------------------------
        ns_hlx::subreq *l_subreq = new ns_hlx::subreq("MY_COOL_ID");
        l_subreq->set_save_response(false);

        // Default headers
        l_subreq->set_header("User-Agent","hlo Server Load Tester");
        l_subreq->set_header("Accept","*/*");
        //l_subreq->set_header("User-Agent","ONGA_BONGA (╯°□°）╯︵ ┻━┻)");
        //l_subreq->set_header("User-Agent","Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/33.0.1750.117 Safari/537.36");
        //l_subreq->set_header("Accept","text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8");
        //l_subreq->set_header("Accept","gzip,deflate");
        //l_subreq->set_header("Connection","keep-alive");

        l_subreq->set_num_to_request(-1);
        l_subreq->m_type = ns_hlx::subreq::SUBREQ_TYPE_DUPE;
        l_subreq->set_create_req_cb(s_create_request_cb);
        //l_subreq->set_num_reqs_per_conn(1);

        // Initialize rand...
        g_rand_ptr = (tinymt64_t*)malloc(sizeof(tinymt64_t));
        tinymt64_init(g_rand_ptr, hlo_get_time_us());

        // Initialize mutex for sequential path requesting
        pthread_mutex_init(&g_path_vector_mutex, NULL);

        // -------------------------------------------
        // Get args...
        // -------------------------------------------
        char l_opt;
        std::string l_argument;
        int l_option_index = 0;
        struct option l_long_options[] =
                {
                { "help",         0, 0, 'h' },
                { "version",      0, 0, 'r' },
                { "no_wildcards", 0, 0, 'w' },
                { "data",         1, 0, 'd' },
                { "cipher",       1, 0, 'y' },
                { "parallel",     1, 0, 'p' },
                { "fetches",      1, 0, 'f' },
                { "num_calls",    1, 0, 'N' },
                { "keep_alive",   0, 0, 'k' },
                { "threads",      1, 0, 't' },
                { "header",       1, 0, 'H' },
                { "verb",         1, 0, 'X' },
                { "rate",         1, 0, 'A' },
                { "mode",         1, 0, 'M' },
                { "seconds",      1, 0, 'l' },
                { "recv_buffer",  1, 0, 'R' },
                { "send_buffer",  1, 0, 'S' },
                { "no_delay",     0, 0, 'D' },
                { "timeout",      1, 0, 'T' },
                { "no_stats",     0, 0, 'x' },
                { "verbose",      0, 0, 'v' },
                { "color",        0, 0, 'c' },
                { "quiet",        0, 0, 'q' },
                { "responses",    0, 0, 'C' },
                { "responses_per",0, 0, 'L' },
                { "http_load",    1, 0, 'Y' },
                { "data_port",    1, 0, 'P' },
                { "gprofile",     1, 0, 'G' },

                // list sentinel
                { 0, 0, 0, 0 }
        };

        // -------------------------------------------
        // Assume unspecified arg url...
        // TODO Unsure if good way to allow unspecified
        // arg...
        // -------------------------------------------
        std::string l_url;
        std::string l_gprof_file;
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

        while ((l_opt = getopt_long_only(argc, argv, "hrkwd:y:p:f:N:t:H:X:A:M:l:R:S:DT:xvcqCLY:P:G:", l_long_options, &l_option_index)) != -1)
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
                case 'r':
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
                                l_status = read_file(l_argument.data() + 1, &(l_subreq->m_body_data), &(l_subreq->m_body_data_len));
                                if(l_status != 0)
                                {
                                        printf("Error reading body data from file: %s\n", l_argument.c_str() + 1);
                                        return -1;
                                }
                        }
                        else
                        {
                                l_subreq->m_body_data_len = l_argument.length() + 1;
                                l_subreq->m_body_data = (char *)malloc(sizeof(char)*l_subreq->m_body_data_len);
                                memcpy(l_subreq->m_body_data,l_argument.c_str(), l_subreq->m_body_data_len);
                        }

                        // Add content length
                        char l_len_str[64];
                        sprintf(l_len_str, "%u", l_subreq->m_body_data_len);
                        l_subreq->set_header("Content-Length", l_len_str);
                        break;
                }
                // ---------------------------------------
                // Google Profiler Output File
                // ---------------------------------------
                case 'G':
                {
                        l_gprof_file = l_argument;
                        break;
                }
                // ---------------------------------------
                // cipher
                // ---------------------------------------
                case 'y':
                {
                        std::string l_cipher_str = l_argument;
                        if (strcasecmp(l_cipher_str.c_str(), "fastsec") == 0)
                                l_cipher_str = "RC4-MD5";
                        else if (strcasecmp(l_cipher_str.c_str(), "highsec") == 0)
                                l_cipher_str = "DES-CBC3-SHA";
                        else if (strcasecmp(l_cipher_str.c_str(), "paranoid") == 0)
                                l_cipher_str = "AES256-SHA";
                        l_hlx->set_ssl_cipher_list(l_cipher_str);
                        break;
                }
                // ---------------------------------------
                // parallel
                // ---------------------------------------
                case 'p':
                {
                        //NDBG_PRINT("arg: --parallel: %s\n", optarg);
                        //l_settings.m_start_type = START_PARALLEL;
                        l_start_parallel = atoi(optarg);
                        if (l_start_parallel < 1)
                        {
                                printf("Error parallel must be at least 1\n");
                                return -1;
                        }
                        l_hlx->set_num_parallel(l_start_parallel);
                        l_settings.m_num_parallel = l_start_parallel;
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
                        l_subreq->set_num_to_request(l_end_fetches);
                        g_num_to_request = l_end_fetches;
                        break;
                }
                // ---------------------------------------
                // number of calls per connection
                // ---------------------------------------
                case 'N':
                {
                        l_max_reqs_per_conn = atoi(optarg);
                        if (l_max_reqs_per_conn < 1)
                        {
                                printf("Error num-calls must be at least 1");
                                return -1;
                        }
                        l_subreq->set_num_reqs_per_conn(l_max_reqs_per_conn);
                        break;
                }
                // ---------------------------------------
                // enable keep alive connections
                // ---------------------------------------
                case 'k':
                {
                        if(l_max_reqs_per_conn == 1)
                        {
                                l_subreq->set_num_reqs_per_conn(-1);
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
                        l_hlx->set_num_threads(l_max_threads);
                        g_num_threads = l_max_threads;
                        break;
                }
                // ---------------------------------------
                // Header
                // ---------------------------------------
                case 'H':
                {
                        int32_t l_status;
                        l_status = l_subreq->set_header(l_argument);
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
                        l_subreq->set_verb(l_argument);
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
                        l_hlx->set_sock_opt_recv_buf_size(l_sock_opt_recv_buf_size);

                        break;
                }
                // ---------------------------------------
                // sock_opt_send_buf_size
                // ---------------------------------------
                case 'S':
                {
                        int l_sock_opt_send_buf_size = atoi(optarg);
                        // TODO Check value...
                        l_hlx->set_sock_opt_send_buf_size(l_sock_opt_send_buf_size);
                        break;
                }
                // ---------------------------------------
                // No delay
                // ---------------------------------------
                case 'D':
                {
                        l_hlx->set_sock_opt_no_delay(true);
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
                        l_subreq->set_timeout_s(l_subreq_timeout_s);
                        break;
                }
                // ---------------------------------------
                // No stats
                // ---------------------------------------
                case 'x':
                {
                        l_hlx->set_collect_stats(false);
                        break;
                }
                // ---------------------------------------
                // verbose
                // ---------------------------------------
                case 'v':
                {
                        l_settings.m_verbose = true;
                        l_hlx->set_verbose(true);
                        l_subreq->set_save_response(true);
                        break;
                }
                // ---------------------------------------
                // color
                // ---------------------------------------
                case 'c':
                {
                        l_settings.m_color = true;
                        l_hlx->set_color(true);
                        break;
                }
                // ---------------------------------------
                // quiet
                // ---------------------------------------
                case 'q':
                {
                        l_settings.m_quiet = true;
                        l_hlx->set_quiet(true);
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
                        l_http_load_display = atoi(optarg);
                        if ((l_http_load_display < 0) || (l_http_load_display > 3))
                        {
                                printf("Error: http load display mode must be between 0--3\n");
                                //print_usage(stdout, -1);
                                return -1;
                        }
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
        // Sigint handler3
        // -------------------------------------------
        if (signal(SIGINT, sig_handler) == SIG_ERR)
        {
                printf("Error: can't catch SIGINT\n");
                return -1;
        }
        // TODO???
        //signal(SIGPIPE, SIG_IGN);

        // -------------------------------------------
        // Add url from command line
        // -------------------------------------------
        if(l_url.length())
        {
                //printf("Adding url: %s\n", l_url.c_str());
                int32_t l_status;
                // Set url
                l_status = l_subreq->init_with_url(l_url);
                if(l_status != 0)
                {
                        printf("Error: performing init_with_url: %s\n", l_url.c_str());
                        return -1;
                }
                // Set callback
                l_subreq->set_cb(s_completion_cb);
                l_status = l_hlx->add_subreq(l_subreq);
                if(l_status != 0)
                {
                        printf("Error: performing add_subreq with url: %s\n", l_url.c_str());
                        return -1;
                }
        }
        else
        {
                fprintf(stdout, "Error: No specified URL on cmd line.\n");
                print_usage(stdout, -1);
        }

        // -------------------------------------------
        // Paths...
        // -------------------------------------------
        std::string l_raw_path = l_subreq->m_path;
        //printf("l_raw_path: %s\n",l_raw_path.c_str());
        if(l_wildcarding)
        {
                int32_t l_status;
                l_status = special_effects_parse(l_raw_path);
                if(l_status != STATUS_OK)
                {
                        printf("Error performing special_effects_parse with path: %s\n", l_raw_path.c_str());
                        return -1;
                }
                if(g_path_vector.size() > 1)
                {
                        l_subreq->m_multipath = true;
                }
                else
                {
                        l_subreq->m_multipath = false;
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
                ns_hlx::listener *l_listener = new ns_hlx::listener(l_http_data_port, ns_hlx::SCHEME_TCP);
                stats_getter *l_stats_getter = new stats_getter();
                l_stats_getter->m_hlx = l_hlx;
                int32_t l_status;
                l_status = l_listener->add_endpoint("/", l_stats_getter);
                if(l_status != 0)
                {
                        printf("Error: adding endpoint: %s\n", "/");
                        return -1;
                }
                l_hlx->add_listener(l_listener);
        }


#ifdef ENABLE_PROFILER
        // Start Profiler
        if (!l_gprof_file.empty())
                ProfilerStart(l_gprof_file.c_str());
#endif

        // Run
        // Message
        fprintf(stdout, "Running %d threads %d parallel connections per thread with %d reqests per connection\n",
                l_max_threads, l_start_parallel, l_max_reqs_per_conn);

        int32_t l_run_status = 0;
        l_run_status = l_hlx->run();
        if(0 != l_run_status)
        {
                printf("Error: performing hlo::run");
                return -1;
        }

        uint64_t l_start_time_ms = hlo_get_time_ms();
        l_settings.m_start_time_ms = l_start_time_ms;
        l_settings.m_last_display_time_ms = l_start_time_ms;

        // -------------------------------------------
        // Run command exec
        // -------------------------------------------
        command_exec(l_settings);

        if(l_settings.m_verbose)
        {
                printf("Finished -joining all threads\n");
        }

        // Wait for completion
        l_hlx->wait_till_stopped();

#ifdef ENABLE_PROFILER
        if (!l_gprof_file.empty())
                ProfilerStop();
#endif

        uint64_t l_end_time_ms = hlo_get_time_ms() - l_start_time_ms;


        // -------------------------------------------
        // Stats summary
        // -------------------------------------------
        if(l_http_load_display != -1)
        {
                display_results_http_load_style(l_settings,
                                                ((double)l_end_time_ms)/1000.0,
                                                (bool)((l_http_load_display&(0x2)) >> 1));
        }
        else
        {
                display_results(l_settings, ((double)l_end_time_ms)/1000.0);
        }

        // -------------------------------------------
        // Cleanup...
        // -------------------------------------------
        if(l_hlx)
        {
                delete l_hlx;
                l_hlx = NULL;
        }

        //if(l_settings.m_verbose)
        //{
        //      NDBG_PRINT("Cleanup\n");
        //}

        return 0;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t hlo_get_time_ms(void)
{
        uint64_t l_retval;
        struct timespec l_timespec;
        clock_gettime(CLOCK_REALTIME, &l_timespec);
        l_retval = (((uint64_t)l_timespec.tv_sec) * 1000) + (((uint64_t)l_timespec.tv_nsec) / 1000000);
        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t hlo_get_delta_time_ms(uint64_t a_start_time_ms)
{
        uint64_t l_retval;
        struct timespec l_timespec;
        clock_gettime(CLOCK_REALTIME, &l_timespec);
        l_retval = (((uint64_t)l_timespec.tv_sec) * 1000) + (((uint64_t)l_timespec.tv_nsec) / 1000000);
        return l_retval - a_start_time_ms;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t hlo_get_time_us(void)
{
        uint64_t l_retval;
        struct timespec l_timespec;
        clock_gettime(CLOCK_REALTIME, &l_timespec);
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
void display_responses_line(settings_struct &a_settings)
{
        ns_hlx::t_stat_t l_total;
        uint64_t l_cur_time_ms = hlo_get_time_ms();

        // Get stats
        a_settings.m_hlx->get_stats(l_total);

        double l_reqs_per_s = ((double)(l_total.m_total_reqs - a_settings.m_last_stat->m_total_reqs)*1000.0) /
                              ((double)(l_cur_time_ms - a_settings.m_last_display_time_ms));
        a_settings.m_last_display_time_ms = hlo_get_time_ms();
        *(a_settings.m_last_stat) = l_total;

        // Aggregate over status code map
        ns_hlx::status_code_count_map_t m_status_code_count_map;

        uint32_t l_responses[10] = {0};
        for(ns_hlx::status_code_count_map_t::iterator i_code = l_total.m_status_code_count_map.begin();
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

        if(a_settings.m_show_per_interval)
        {

                // Calculate rates
                double l_rate[10] = {0.0};
                uint32_t l_totals = 0;

                for(uint32_t i = 2; i <= 5; ++i)
                {
                        l_totals += l_responses[i] - a_settings.m_last_responses_count[i];
                }

                for(uint32_t i = 2; i <= 5; ++i)
                {
                        if(l_totals)
                        {
                                l_rate[i] = (100.0*((double)(l_responses[i] - a_settings.m_last_responses_count[i]))) / ((double)(l_totals));
                        }
                        else
                        {
                                l_rate[i] = 0.0;
                        }
                }

                if(a_settings.m_color)
                {
                                printf("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %s%9.2f%s | %s%9.2f%s | %s%9.2f%s | %s%9.2f%s |\n",
                                                ((double)(hlo_get_delta_time_ms(a_settings.m_start_time_ms))) / 1000.0,
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
                                        ((double)(hlo_get_delta_time_ms(a_settings.m_start_time_ms))) / 1000.0,
                                        l_reqs_per_s,
                                        l_total.m_total_reqs,
                                        l_total.m_num_errors,
                                        l_rate[2],
                                        l_rate[3],
                                        l_rate[4],
                                        l_rate[5]);
                }

                // Update last
                a_settings.m_last_responses_count[2] = l_responses[2];
                a_settings.m_last_responses_count[3] = l_responses[3];
                a_settings.m_last_responses_count[4] = l_responses[4];
                a_settings.m_last_responses_count[5] = l_responses[5];
        }
        else
        {
                if(a_settings.m_color)
                {
                                printf("| %8.2fs / %10.2fs / %9" PRIu64 " / %9" PRIu64 " / %s%9u%s | %s%9u%s | %s%9u%s | %s%9u%s |\n",
                                                ((double)(hlo_get_delta_time_ms(a_settings.m_start_time_ms))) / 1000.0,
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
                                        ((double)(hlo_get_delta_time_ms(a_settings.m_start_time_ms))) / 1000.0,
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
void display_results_line_desc(settings_struct &a_settings)
{
        printf("+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+\n");
        if(a_settings.m_color)
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
void display_results_line(settings_struct &a_settings)
{
        ns_hlx::t_stat_t l_total;
        uint64_t l_cur_time_ms = hlo_get_time_ms();

        // Get stats
        a_settings.m_hlx->get_stats(l_total);
        double l_reqs_per_s = ((double)(l_total.m_total_reqs - a_settings.m_last_stat->m_total_reqs)*1000.0) /
                        ((double)(l_cur_time_ms - a_settings.m_last_display_time_ms));
        double l_kb_per_s = ((double)(l_total.m_num_bytes_read - a_settings.m_last_stat->m_num_bytes_read)*1000.0/1024) /
                        ((double)(l_cur_time_ms - a_settings.m_last_display_time_ms));
        a_settings.m_last_display_time_ms = hlo_get_time_ms();
        *a_settings.m_last_stat = l_total;
        if(a_settings.m_color)
        {
                        printf("| %s%9" PRIu64 "%s / %s%9" PRIi64 "%s | %s%9" PRIu64 "%s | %s%9" PRIu64 "%s | %s%12.2f%s | %8.2fs | %10.2fs | %8.2fs |\n",
                                        ANSI_COLOR_FG_GREEN, l_total.m_total_reqs, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_BLUE, l_total.m_total_reqs, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_MAGENTA, l_total.m_num_idle_killed, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_RED, l_total.m_num_errors, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_YELLOW, ((double)(l_total.m_num_bytes_read))/(1024.0), ANSI_COLOR_OFF,
                                        ((double)(hlo_get_delta_time_ms(a_settings.m_start_time_ms))) / 1000.0,
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
                                ((double)(hlo_get_delta_time_ms(a_settings.m_start_time_ms)) / 1000.0),
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
static void show_total_agg_stat(std::string &a_tag,
        const ns_hlx::t_stat_t &a_stat,
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
        SHOW_XSTAT_LINE("ms/end2end:", a_stat.m_stat_us_end_to_end);

        if(a_color)
                printf("| %sHTTP response codes%s: \n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF);
        else
                printf("| HTTP response codes: \n");

        for(ns_hlx::status_code_count_map_t::const_iterator i_status_code = a_stat.m_status_code_count_map.begin();
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
void display_results(settings_struct &a_settings,
                     double a_elapsed_time)
{
        ns_hlx::t_stat_t l_total;

        // Get stats
        a_settings.m_hlx->get_stats(l_total);

        std::string l_tag;
        // TODO Fix elapse and max parallel
        l_tag = "ALL";
        show_total_agg_stat(l_tag, l_total, a_elapsed_time, a_settings.m_num_parallel, a_settings.m_color);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void show_total_agg_stat_legacy(std::string &a_tag,
                                       const ns_hlx::t_stat_t &a_stat,
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
        SHOW_XSTAT_LINE_LEGACY("msecs/end2end:", a_stat.m_stat_us_end_to_end);

        printf("HTTP response codes: ");
        if(a_sep == "\n")
                printf("%s", a_sep.c_str());

        for(ns_hlx::status_code_count_map_t::const_iterator i_status_code = a_stat.m_status_code_count_map.begin();
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
void display_results_http_load_style(settings_struct &a_settings,
                                     double a_elapsed_time,
                                     bool a_one_line_flag)
{
        ns_hlx::t_stat_t l_total;

        // Get stats
        a_settings.m_hlx->get_stats(l_total);

        std::string l_tag;
        // Separator
        std::string l_sep = "\n";
        if(a_one_line_flag) l_sep = "||";

        // TODO Fix elapse and max parallel
        l_tag = "State";
        show_total_agg_stat_legacy(l_tag, l_total, l_sep, a_elapsed_time, a_settings.m_num_parallel);
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t add_url(ns_hlx::hlx *a_hlx, ns_hlx::subreq *a_subreq, const std::string &a_url)
{
        ns_hlx::subreq *l_subreq = new ns_hlx::subreq(*a_subreq);
        l_subreq->init_with_url(a_url);
        //printf("Adding url: %s\n", a_url.c_str());
        a_hlx->add_subreq(l_subreq);
        return 0;
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
        int32_t l_status = STATUS_OK;
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
        if (STATUS_OK != l_status)
        {
                printf("Error performing fclose.  Reason: %s\n", strerror(errno));
                return -1;
        }
        return 0;
}

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
