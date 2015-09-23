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
#include "hlx_client.h"
#include "util.h"
#include "ndebug.h"
#include "hlx_server.h"

#include <string.h>

// getrlimit
#include <sys/time.h>
#include <sys/resource.h>

// signal
#include <signal.h>

// Shared pointer
//#include <tr1/memory>

#include <list>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h> // For getopt_long
#include <termios.h>

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
//: Constants
//: ----------------------------------------------------------------------------
// Version
#define HLO_VERSION_MAJOR 0
#define HLO_VERSION_MINOR 0
#define HLO_VERSION_MACRO 1
#define HLO_VERSION_PATCH "alpha"

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------

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
                if(!m_hlx_client)
                {
                        return -1;
                }
                char l_char_buf[2048];
                m_hlx_client->get_stats_json(l_char_buf, 2048);

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
        ns_hlx::hlx_client *m_hlx_client;
        stats_getter(void):
                m_hlx_client(NULL)
        {};
private:
        DISALLOW_COPY_AND_ASSIGN(stats_getter);
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

        ns_hlx::hlx_client *m_hlx_client;

        // Stats
        uint64_t m_start_time_ms;
        uint64_t m_last_display_time_ms;

        ns_hlx::t_stat_t *m_last_stat;
        int32_t m_run_time_ms;

        // Used for displaying interval stats
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
                m_num_parallel(128),
                m_hlx_client(NULL),
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
        HLX_CLIENT_DISALLOW_COPY_AND_ASSIGN(settings_struct);
} settings_struct_t;

//: ----------------------------------------------------------------------------
//: Prototypes
//: ----------------------------------------------------------------------------
uint64_t hlo_get_time_ms(void);
uint64_t hlo_get_delta_time_ms(uint64_t a_start_time_ms);
void display_results_line_desc(settings_struct &a_settings);
void display_results_line(settings_struct &a_settings);
void display_responses_line_desc(settings_struct &a_settings);
void display_responses_line(settings_struct &a_settings);

void display_results(settings_struct &a_settings,
                     double a_elapsed_time,
                     bool a_show_breakdown_flag = false);
void display_results_http_load_style(settings_struct &a_settings,
                                     double a_elapsed_time,
                                     bool a_show_breakdown_flag = false,
                                     bool a_one_line_flag = false);
//: ----------------------------------------------------------------------------
//: Globals
//: ----------------------------------------------------------------------------
static ns_hlx::hlx_client *g_hlx_client = NULL;

//: ----------------------------------------------------------------------------
//: \details: sighandler
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool g_test_finished = false;
bool g_cancelled = false;
settings_struct_t *g_settings = NULL;
void sig_handler(int signo)
{
        if (signo == SIGINT)
        {
                // Kill program
                g_test_finished = true;
                g_cancelled = true;
                g_settings->m_hlx_client->stop();
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
        ns_hlx::hlx_client *l_hlx_client = a_settings.m_hlx_client;
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
                                l_hlx_client->stop();
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
                                l_hlx_client->stop();
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

                if (!l_hlx_client->is_running())
                {
                        g_test_finished = true;
                }

        }

        // Send stop -if unsent
        if(!l_sent_stop)
        {
                l_hlx_client->stop();
                l_sent_stop = true;
        }

        nonblock(NB_DISABLE);

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
        fprintf(a_stream, "  -u, --url_file      URL file.\n");
        fprintf(a_stream, "  -w, --no_wildcards  Don't wildcard the url.\n");
        fprintf(a_stream, "  -d, --data          HTTP body data -supports bodies up to 8k.\n");

        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Settings:\n");
        fprintf(a_stream, "  -y, --cipher        Cipher --see \"openssl ciphers\" for list.\n");
        fprintf(a_stream, "  -p, --parallel      Num parallel Default: 64.\n");
        fprintf(a_stream, "  -f, --fetches       Num fetches.\n");
        fprintf(a_stream, "  -N, --num_calls     Number of requests per connection\n");
        fprintf(a_stream, "  -k, --keep_alive    Re-use connections for all requests\n");
        fprintf(a_stream, "  -t, --threads       Number of parallel threads.\n");
        fprintf(a_stream, "  -H, --header        Request headers -can add multiple ie -H<> -H<>...\n");
        fprintf(a_stream, "  -X, --verb          Request command -HTTP verb to use -GET/PUT/etc\n");
        fprintf(a_stream, "  -l, --seconds       Run for <N> seconds .\n");
        fprintf(a_stream, "  -A, --rate          Max Request Rate.\n");
        fprintf(a_stream, "  -M, --mode          Requests mode [roundrobin|sequential|random].\n");
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
        fprintf(a_stream, "  -B, --breakdown     Show breakdown\n");
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
        ns_hlx::hlx_client *l_hlx_client = new ns_hlx::hlx_client();
        l_settings.m_hlx_client = l_hlx_client;
        g_hlx_client = l_hlx_client;

        // For sighandler
        g_settings = &l_settings;

        // Turn on wildcarding by default
        l_hlx_client->set_wildcarding(true);
        l_hlx_client->set_split_requests_by_thread(false);
        l_hlx_client->set_collect_stats(true);
        l_hlx_client->set_save_response(false);
        l_hlx_client->set_use_ai_cache(true);

        // -------------------------------------------
        // Setup default headers before the user
        // -------------------------------------------
        l_hlx_client->set_header("User-Agent", "hlo Server Load Tester");
        //l_hlo->set_header("User-Agent", "ONGA_BONGA (╯°□°）╯︵ ┻━┻)");
        //l_hlo->set_header("User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/33.0.1750.117 Safari/537.36");
        //l_hlo->set_header("x-select-backend", "self");
        l_hlx_client->set_header("Accept", "*/*");
        //l_hlo->set_header("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8");
        //l_hlo->set_header("Accept-Encoding", "gzip,deflate");
        //l_hlo->set_header("Connection", "keep-alive");

        int32_t l_http_load_display = -1;
        int32_t l_http_data_port = -1;

        bool l_show_breakdown = false;

        int l_max_threads = 4;
        // TODO Make default definitions
        int l_start_parallel = 128;
        int l_max_reqs_per_conn = 1;
        int l_end_fetches = -1;
        bool l_input_flag = false;


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
                { "url_file",     1, 0, 'u' },
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
                { "breakdown",    0, 0, 'B' },
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
        std::string l_url_file_str;
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

        while ((l_opt = getopt_long_only(argc, argv, "hrku:wd:y:p:f:N:t:H:X:A:M:l:R:S:DT:xvcqCLY:BP:G:", l_long_options, &l_option_index)) != -1)
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
                // URL File
                // ---------------------------------------
                case 'u':
                {
                        l_url_file_str = l_argument;
                        l_input_flag = true;
                        break;
                }
                // ---------------------------------------
                // Wildcarding
                // ---------------------------------------
                case 'w':
                {
                        l_hlx_client->set_wildcarding(false);
                        break;
                }
                // ---------------------------------------
                // Data
                // ---------------------------------------
                case 'd':
                {
                        int32_t l_status;
                        l_status = l_hlx_client->set_data(l_argument.c_str(), l_argument.length());
                        if(l_status != HLX_CLIENT_STATUS_OK)
                        {
                                printf("Error setting HTTP body data with: %s\n", l_argument.c_str());
                                return -1;
                        }
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
                        l_hlx_client->set_ssl_cipher_list(l_cipher_str);
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
                        l_hlx_client->set_num_parallel(l_start_parallel);
                        l_settings.m_num_parallel = l_start_parallel;
                        break;
                }
                // ---------------------------------------
                // fetches
                // ---------------------------------------
                case 'f':
                {
                        //NDBG_PRINT("arg: --fetches: %s\n", optarg);
                        //l_settings.m_end_type = END_FETCHES;
                        l_end_fetches = atoi(optarg);
                        if (l_end_fetches < 1)
                        {
                                printf("Error fetches must be at least 1\n");
                                return -1;
                        }
                        l_hlx_client->set_end_fetches(l_end_fetches);

                        break;
                }
                // ---------------------------------------
                // number of calls per connection
                // ---------------------------------------
                case 'N':
                {
                        //NDBG_PRINT("arg: --max_reqs_per_conn: %s\n", optarg);
                        l_max_reqs_per_conn = atoi(optarg);
                        if (l_max_reqs_per_conn < 1)
                        {
                                printf("Error num-calls must be at least 1");
                                return -1;
                        }

                        l_hlx_client->set_num_reqs_per_conn(l_max_reqs_per_conn);
                        break;
                }
                // ---------------------------------------
                // enable keep alive connections
                // ---------------------------------------
                case 'k':
                {
                        l_hlx_client->set_num_reqs_per_conn(-1);
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
                        l_hlx_client->set_num_threads(l_max_threads);
                        break;
                }
                // ---------------------------------------
                // Header
                // ---------------------------------------
                case 'H':
                {
                        int32_t l_status;
                        l_status = l_hlx_client->set_header(l_argument);
                        if(l_status != HLX_CLIENT_STATUS_OK)
                        {
                                printf("Error header string[%s] is malformed\n", l_argument.c_str());
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
                        l_hlx_client->set_verb(l_argument);
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
                        l_hlx_client->set_rate(l_rate);

                        break;
                }
                // ---------------------------------------
                // Mode
                // ---------------------------------------
                case 'M':
                {
                        ns_hlx::request_mode_t l_mode;
                        std::string l_mode_arg = optarg;
                        if(l_mode_arg == "roundrobin")
                        {
                                l_mode = ns_hlx::REQUEST_MODE_ROUND_ROBIN;
                        }
                        else if(l_mode_arg == "sequential")
                        {
                                l_mode = ns_hlx::REQUEST_MODE_SEQUENTIAL;
                        }
                        else if(l_mode_arg == "random")
                        {
                                l_mode = ns_hlx::REQUEST_MODE_RANDOM;
                        }
                        else
                        {
                                printf("Error: Mode must be [roundrobin|sequential|random]\n");
                                //print_usage(stdout, -1);
                                return -1;
                        }
                        l_hlx_client->set_request_mode(l_mode);

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
                        l_hlx_client->set_sock_opt_recv_buf_size(l_sock_opt_recv_buf_size);

                        break;
                }
                // ---------------------------------------
                // sock_opt_send_buf_size
                // ---------------------------------------
                case 'S':
                {
                        int l_sock_opt_send_buf_size = atoi(optarg);
                        // TODO Check value...
                        l_hlx_client->set_sock_opt_send_buf_size(l_sock_opt_send_buf_size);
                        break;
                }
                // ---------------------------------------
                // No delay
                // ---------------------------------------
                case 'D':
                {
                        l_hlx_client->set_sock_opt_no_delay(true);
                        break;
                }
                // ---------------------------------------
                // timeout
                // ---------------------------------------
                case 'T':
                {
                        //NDBG_PRINT("arg: --fetches: %s\n", optarg);
                        //l_settings.m_end_type = END_FETCHES;
                        int l_timeout;
                        l_timeout = atoi(optarg);
                        if (l_timeout < 1)
                        {
                                printf("timeout must be > 0\n");
                                //print_usage(stdout, -1);
                                return -1;
                        }
                        l_hlx_client->set_timeout_s(l_timeout);

                        break;
                }
                // ---------------------------------------
                // No stats
                // ---------------------------------------
                case 'x':
                {
                        l_hlx_client->set_collect_stats(false);
                        break;
                }
                // ---------------------------------------
                // verbose
                // ---------------------------------------
                case 'v':
                {
                        l_settings.m_verbose = true;
                        l_hlx_client->set_verbose(true);
                        l_hlx_client->set_save_response(true);
                        break;
                }
                // ---------------------------------------
                // color
                // ---------------------------------------
                case 'c':
                {
                        l_settings.m_color = true;
                        l_hlx_client->set_color(true);
                        break;
                }
                // ---------------------------------------
                // quiet
                // ---------------------------------------
                case 'q':
                {
                        l_settings.m_quiet = true;
                        l_hlx_client->set_quiet(true);
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
                // show breakdown
                // ---------------------------------------
                case 'B':
                {
                        l_show_breakdown = true;
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

        if (l_end_fetches > 0 && l_start_parallel > l_end_fetches)
        {
                l_start_parallel = l_end_fetches;
                l_hlx_client->set_num_parallel(l_start_parallel);
        }

        // -------------------------------------------
        // Sigint handler
        // -------------------------------------------
        if (signal(SIGINT, sig_handler) == SIG_ERR)
        {
                printf("Error: can't catch SIGINT\n");
                return -1;
        }
        // TODO???
        //signal(SIGPIPE, SIG_IGN);

        // -------------------------------------------
        // Ingest URL File or URL from command line
        // if one...
        // -------------------------------------------
        if(l_url_file_str.length())
        {
                int32_t l_status;
                l_status = l_hlx_client->add_url_file(l_url_file_str);
                if(HLX_CLIENT_STATUS_OK != l_status)
                {
                        printf("Error: adding url file: %s\n", l_url_file_str.c_str());
                        return -1;
                }
                // TODO Check status
        }
        else if(l_url.length())
        {
                l_hlx_client->add_url(l_url);
        } else
        {
                fprintf(stdout, "Error: No specified URLs on cmd line or in file with -u.\n");
                print_usage(stdout, -1);
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
        l_run_status = l_hlx_client->run();
        if(0 != l_run_status)
        {
                printf("Error: performing hlo::run");
                return -1;
        }

        uint64_t l_start_time_ms = hlo_get_time_ms();
        l_settings.m_start_time_ms = l_start_time_ms;

        // -------------------------------------------
        // Start api server
        // -------------------------------------------
        ns_hlx::hlx_server *l_hlx_server = NULL;
        if(l_http_data_port > 0)
        {
                l_hlx_server = new ns_hlx::hlx_server();
                l_hlx_server->set_port(l_http_data_port);

                stats_getter *l_stats_getter = new stats_getter();
                l_stats_getter->m_hlx_client = l_hlx_client;
                int32_t l_status;
                l_status = l_hlx_server->add_endpoint("/", l_stats_getter);
                if(l_status != 0)
                {
                        printf("Error: adding endpoint: %s\n", "/");
                        return -1;
                }

                int32_t l_run_status = 0;
                l_run_status = l_hlx_server->run();
                if(l_run_status != 0)
                {
                        printf("Error: performing hlx_server::run");
                        return -1;
                }
        }

        // -------------------------------------------
        // Run command exec
        // -------------------------------------------
        command_exec(l_settings);

        if(l_settings.m_verbose)
        {
                printf("Finished -joining all threads\n");
        }

        // Wait for completion
        l_hlx_client->wait_till_stopped();

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
                                                (bool)(l_http_load_display&(0x1)),
                                                (bool)((l_http_load_display&(0x2)) >> 1));
        }
        else
        {
                display_results(l_settings, ((double)l_end_time_ms)/1000.0, l_show_breakdown);
        }

        // -------------------------------------------
        // Cleanup...
        // -------------------------------------------
        if(l_hlx_server)
        {
                delete l_hlx_server;
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
        ns_hlx::tag_stat_map_t l_unused;
        uint64_t l_cur_time_ms = hlo_get_time_ms();

        // Get stats
        a_settings.m_hlx_client->get_stats(l_total, false, l_unused);

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
        ns_hlx::tag_stat_map_t l_unused;
        uint64_t l_cur_time_ms = hlo_get_time_ms();

        // Get stats
        a_settings.m_hlx_client->get_stats(l_total, false, l_unused);

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
                     double a_elapsed_time,
                     bool a_show_breakdown_flag)
{
        ns_hlx::tag_stat_map_t l_tag_stat_map;
        ns_hlx::t_stat_t l_total;

        // Get stats
        a_settings.m_hlx_client->get_stats(l_total, a_show_breakdown_flag, l_tag_stat_map);

        std::string l_tag;
        // TODO Fix elapse and max parallel
        l_tag = "ALL";
        show_total_agg_stat(l_tag, l_total, a_elapsed_time, a_settings.m_num_parallel, a_settings.m_color);

        // -------------------------------------------
        // Optional Breakdown
        // -------------------------------------------
        if(a_show_breakdown_flag)
        {
                for(ns_hlx::tag_stat_map_t::iterator i_stat = l_tag_stat_map.begin();
                                i_stat != l_tag_stat_map.end();
                                ++i_stat)
                {
                        l_tag = i_stat->first;
                        show_total_agg_stat(l_tag, i_stat->second, a_elapsed_time, a_settings.m_num_parallel, a_settings.m_color);
                }
        }
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
                                     bool a_show_breakdown_flag,
                                     bool a_one_line_flag)
{
        ns_hlx::tag_stat_map_t l_tag_stat_map;
        ns_hlx::t_stat_t l_total;

        // Get stats
        a_settings.m_hlx_client->get_stats(l_total, a_show_breakdown_flag, l_tag_stat_map);

        std::string l_tag;
        // Separator
        std::string l_sep = "\n";
        if(a_one_line_flag) l_sep = "||";

        // TODO Fix elapse and max parallel
        l_tag = "State";
        show_total_agg_stat_legacy(l_tag, l_total, l_sep, a_elapsed_time, a_settings.m_num_parallel);

        // -------------------------------------------
        // Optional Breakdown
        // -------------------------------------------
        if(a_show_breakdown_flag)
        {
                for(ns_hlx::tag_stat_map_t::iterator i_stat = l_tag_stat_map.begin();
                                i_stat != l_tag_stat_map.end();
                                ++i_stat)
                {
                        l_tag = "[";
                        l_tag += i_stat->first;
                        l_tag += "]";
                        show_total_agg_stat_legacy(l_tag, i_stat->second, l_sep, a_elapsed_time, a_settings.m_num_parallel);
                }
        }
}

