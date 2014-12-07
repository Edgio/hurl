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
#include "hlo.h"
#include "t_microhttpd.h"
#include "util.h"

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
#define NB_ENABLE  1
#define NB_DISABLE 0

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::list <t_client *> t_client_list_t;

//: ----------------------------------------------------------------------------
//: Settings
//: ----------------------------------------------------------------------------
typedef struct settings_struct
{
        bool m_verbose;
        bool m_color;
        bool m_quiet;

        // ---------------------------------
        // Defaults...
        // ---------------------------------
        settings_struct() :
                m_verbose(false),
                m_color(false),
                m_quiet(false)
        {}

} settings_struct_t;

// ---------------------------------------------------------
// Structure of arguments to pass to client thread
// ---------------------------------------------------------
typedef struct thread_args_struct
{
                t_client_list_t m_t_client_list;
                settings_struct m_settings;

        thread_args_struct() :
                m_t_client_list(),
                m_settings()
        {};

} thread_args_struct_t;

//: ----------------------------------------------------------------------------
//: Forward Decls
//: ----------------------------------------------------------------------------
void command_exec(thread_args_struct_t &a_thread_args);

//: ----------------------------------------------------------------------------
//: \details: Print the version.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool g_test_finished = false;
void sig_handler(int signo)
{
  if (signo == SIGINT)
  {
          // Kill program
          //NDBG_PRINT("SIGINT\n");
          g_test_finished = true;
  }
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
        fprintf(a_stream, "  -h, --help         Display this help and exit.\n");
        fprintf(a_stream, "  -v, --version      Display the version number and exit.\n");

        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Input Options:\n");
        fprintf(a_stream, "  -u, --url_file     URL file.\n");
        fprintf(a_stream, "  -w, --no_wildcards Don't wildcard the url.\n");

        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Settings:\n");
        fprintf(a_stream, "  -y, --cipher       Cipher --see \"openssl ciphers\" for list.\n");
        fprintf(a_stream, "  -p, --parallel     Num parallel.\n");
        fprintf(a_stream, "  -f, --fetches      Num fetches.\n");
        fprintf(a_stream, "  -N, --num_calls    Number of requests per connection\n");
        fprintf(a_stream, "  -k, --keep_alive   Re-use connections for all requests\n");
        fprintf(a_stream, "  -t, --threads      Number of parallel threads.\n");
        fprintf(a_stream, "  -H, --header       Request headers -can add multiple ie -H<> -H<>...\n");
        fprintf(a_stream, "  -l, --seconds      Run for <N> seconds .\n");
        fprintf(a_stream, "  -r, --rate         Max Request Rate.\n");
        fprintf(a_stream, "  -M, --mode         Requests mode [roundrobin|sequential|random].\n");
        //fprintf(a_stream, "  -c, --checksum   Checksum.\n");
        fprintf(a_stream, "  -R, --recv_buffer  Socket receive buffer size.\n");
        fprintf(a_stream, "  -S, --send_buffer  Socket send buffer size.\n");
        fprintf(a_stream, "  -D, --no_delay     Socket TCP no-delay.\n");
        fprintf(a_stream, "  -T, --timeout      Timeout (seconds).\n");

        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Print Options:\n");
        fprintf(a_stream, "  -x, --verbose      Verbose logging\n");
        fprintf(a_stream, "  -c, --color        Color\n");
        fprintf(a_stream, "  -q, --quiet        Suppress progress output\n");

        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Stat Options:\n");
        fprintf(a_stream, "  -P, --data_port    Start HTTP Stats Daemon on port.\n");
        fprintf(a_stream, "  -B, --breakdown    Show breakdown\n");
        fprintf(a_stream, "  -X, --http_load    Display in http load mode [MODE] -Legacy support\n");
        fprintf(a_stream, "  -G, --gprofile     Google profiler output file\n");

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
        hlo *l_hlo = hlo::get();

        // -------------------------------------------
        // Setup default headers before the user
        // -------------------------------------------
        l_hlo->set_header("User-Agent", "hlo Server Load Tester");
        //l_hlo->set_header("User-Agent", "ONGA_BONGA (╯°□°）╯︵ ┻━┻)");
        //l_hlo->set_header("User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/33.0.1750.117 Safari/537.36");
        //l_hlo->set_header("x-select-backend", "self");
        l_hlo->set_header("Accept", "*/*");
        //l_hlo->set_header("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8");
        //l_hlo->set_header("Accept-Encoding", "gzip,deflate");
        //l_hlo->set_header("Connection", "keep-alive");

        settings_struct_t l_settings;
        thread_args_struct_t l_thread_args;
        int32_t l_http_load_display = -1;
        int32_t l_http_data_port = -1;
        bool l_show_breakdown = false;
        int l_max_threads = 1;
        int l_start_parallel = 1;
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
                { "version",      0, 0, 'v' },
                { "url_file",     1, 0, 'u' },
                { "no_wildcards", 0, 0, 'w' },
                { "cipher",       1, 0, 'y' },
                { "parallel",     1, 0, 'p' },
                { "fetches",      1, 0, 'f' },
                { "num_calls",    1, 0, 'N' },
                { "keep_alive",   0, 0, 'k' },
                { "threads",      1, 0, 't' },
                { "header",       1, 0, 'H' },
                { "rate",         1, 0, 'r' },
                { "mode",         1, 0, 'M' },
                { "seconds",      1, 0, 'l' },
                { "recv_buffer",  1, 0, 'R' },
                { "send_buffer",  1, 0, 'S' },
                { "no_delay",     0, 0, 'D' },
                { "timeout",      1, 0, 'T' },
                { "verbose",      0, 0, 'x' },
                { "color",        0, 0, 'c' },
                { "quiet",        0, 0, 'q' },
                { "http_load",    1, 0, 'X' },
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

        while ((l_opt = getopt_long_only(argc, argv, "hvku:wy:p:f:N:t:H:r:M:l:R:S:DT:xcqX:BP:G:", l_long_options, &l_option_index)) != -1)
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
                case 'v':
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
                        l_hlo->set_wildcarding(false);
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
                        l_hlo->set_cipher_list(l_cipher_str);
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
                                printf("parallel must be at least 1\n");
                                print_usage(stdout, -1);
                        }
                        l_hlo->set_start_parallel(l_start_parallel);
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
                                printf("fetches must be at least 1\n");
                                print_usage(stdout, -1);
                        }
                        l_hlo->set_end_fetches(l_end_fetches);

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
                                printf("num-calls must be at least 1");
                                print_usage(stdout, -1);
                        }

                        l_hlo->set_max_reqs_per_conn(l_max_reqs_per_conn);
                        break;
                }

                // ---------------------------------------
                // enable keep alive connections
                // ---------------------------------------
                case 'k':
                {
                        l_hlo->set_max_reqs_per_conn(-1);
                        break;
                }

                // ---------------------------------------
                // num threads
                // ---------------------------------------
                case 't':
                {
                        //NDBG_PRINT("arg: --threads: %s\n", l_argument.c_str());
                        l_max_threads = atoi(optarg);
                        if (l_max_threads < 1)
                        {
                                printf("num-threads must be at least 1\n");
                                print_usage(stdout, -1);
                        }

                        l_hlo->set_num_threads(l_max_threads);
                        break;
                }
                // ---------------------------------------
                // Header
                // ---------------------------------------
                case 'H':
                {
                        int32_t l_status;
                        std::string l_header_key;
                        std::string l_header_val;
                        l_status = break_header_string(l_argument, l_header_key, l_header_val);
                        if(l_status != 0)
                        {
                                printf("Error header string[%s] is malformed\n", l_argument.c_str());
                                print_usage(stdout, -1);
                        }

                        // Add to hlo header map
                        l_hlo->set_header(l_header_key, l_header_val);
                        // TODO Check status???

                        break;
                }
                // ---------------------------------------
                // rate
                // ---------------------------------------
                case 'r':
                {
                        int l_rate = atoi(optarg);
                        if (l_rate < 1)
                        {
                                printf("Error: rate must be at least 1\n");
                                print_usage(stdout, -1);
                        }
                        l_hlo->set_rate(l_rate);

                        break;
                }
                // ---------------------------------------
                // Mode
                // ---------------------------------------
                case 'M':
                {
                        reqlet_mode_t l_mode;
                        std::string l_mode_arg = optarg;
                        if(l_mode_arg == "roundrobin")
                        {
                                l_mode = REQLET_MODE_ROUND_ROBIN;
                        }
                        else if(l_mode_arg == "sequential")
                        {
                                l_mode = REQLET_MODE_SEQUENTIAL;
                        }
                        else if(l_mode_arg == "random")
                        {
                                l_mode = REQLET_MODE_RANDOM;
                        }
                        else
                        {
                                printf("Error: Mode must be [roundrobin|sequential|random]\n");
                                print_usage(stdout, -1);
                        }
                        l_hlo->set_reqlet_mode(l_mode);

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
                                print_usage(stdout, -1);
                        }
                        l_hlo->set_run_time_s(l_run_time_s);

                        break;
                }

                // ---------------------------------------
                // sock_opt_recv_buf_size
                // ---------------------------------------
                case 'R':
                {
                        int l_sock_opt_recv_buf_size = atoi(optarg);
                        // TODO Check value...
                        l_hlo->set_sock_opt_recv_buf_size(l_sock_opt_recv_buf_size);

                        break;
                }

                // ---------------------------------------
                // sock_opt_send_buf_size
                // ---------------------------------------
                case 'S':
                {
                        int l_sock_opt_send_buf_size = atoi(optarg);
                        // TODO Check value...
                        l_hlo->set_sock_opt_send_buf_size(l_sock_opt_send_buf_size);
                        break;
                }

                // ---------------------------------------
                // No delay
                // ---------------------------------------
                case 'D':
                {
                        l_hlo->set_sock_opt_no_delay(true);
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
                                printf("timeout must be at > 0\n");
                                print_usage(stdout, -1);
                        }
                        l_hlo->set_timeout_s(l_timeout);

                        break;
                }

                // ---------------------------------------
                // verbose
                // ---------------------------------------
                case 'x':
                        l_settings.m_verbose = true;
                        l_hlo->set_verbose(true);
                        break;

                // ---------------------------------------
                // color
                // ---------------------------------------
                case 'c':
                        l_settings.m_color = true;
                        l_hlo->set_color(true);
                        break;

                // ---------------------------------------
                // quiet
                // ---------------------------------------
                case 'q':
                        l_settings.m_quiet = true;
                        l_hlo->set_quiet(true);
                        break;


                // ---------------------------------------
                // http_load
                // ---------------------------------------
                case 'X':
                        l_http_load_display = atoi(optarg);
                        if ((l_http_load_display < 0) || (l_http_load_display > 3))
                        {
                                printf("Error: http load display mode must be between 0--3\n");
                                print_usage(stdout, -1);
                        }
                        break;

                // ---------------------------------------
                // show breakdown
                // ---------------------------------------
                case 'B':
                        l_show_breakdown = true;
                        break;

                // ---------------------------------------
                // Data Port
                // ---------------------------------------
                case 'P':
                        l_http_data_port = (uint16_t)atoi(optarg);
                        if (l_http_data_port < 1)
                        {
                                printf("Error: HTTP Data port must be > 0\n");
                                print_usage(stdout, -1);
                        }
                        break;

                // What???
                case '?':
                        // Required argument was missing
                        // '?' is provided when the 3rd arg to getopt_long does not begin with a ':', and is preceeded
                        // by an automatic error message.
                        fprintf(stdout, "  Exiting.\n");
                        print_usage(stdout, -1);
                        break;

                // Huh???
                default:
                        fprintf(stdout, "Unrecognized option.\n");
                        print_usage(stdout, -1);
                        break;
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
                fprintf(stdout, "Error, number of parallel requests must be less than the number of fetches.");
                print_usage(stdout, -1);
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
        if(l_url_file_str.length()) {
                l_hlo->set_url_file(l_url_file_str);
                // TODO Check status
        } else if(l_url.length()) {
                l_hlo->set_url(l_url);
        } else {
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
        fprintf(stdout, "Running %d parallel connections with: %d reqs/conn, %d threads\n", l_start_parallel, l_max_reqs_per_conn, l_max_threads);

        int32_t l_run_status = 0;
        l_run_status = l_hlo->run();
        if(0 != l_run_status)
        {
                printf("Error: performing hlo::run");
                return -1;
        }

        uint64_t l_start_time_ms = get_time_ms();

        // Start microhttpd
        if(l_http_data_port > 0)
        {
                start_microhttpd(l_http_data_port);
        }

        // -------------------------------------------
        // Run command exec
        // -------------------------------------------
        // Copy in settings
        l_thread_args.m_settings = l_settings;
        command_exec(l_thread_args);

        if(l_settings.m_verbose)
        {
                printf("Finished -joining all threads\n");
        }

        // Wait for completion
        l_hlo->wait_till_stopped();

#ifdef ENABLE_PROFILER
        if (!l_gprof_file.empty())
                ProfilerStop();
#endif

        uint64_t l_end_time_ms = get_time_ms() - l_start_time_ms;


        // -------------------------------------------
        // Stats summary
        // -------------------------------------------
        if(l_http_load_display != -1)
        {
                l_hlo->display_results_http_load_style(((double)l_end_time_ms)/1000.0, 100,
                                (bool)(l_http_load_display&(0x1)),
                                (bool)((l_http_load_display&(0x2)) >> 1));
        }
        else
        {
                l_hlo->display_results(((double)l_end_time_ms)/1000.0, 100, l_show_breakdown);
        }

        // -------------------------------------------
        // Cleanup...
        // -------------------------------------------
        if(l_http_data_port > 0)
        {
                stop_microhttpd();
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
void command_exec(thread_args_struct_t &a_thread_args)
{
        int i = 0;
        char l_cmd = ' ';
        bool l_sent_stop = false;
        hlo *l_hlo = hlo::get();
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
                                l_hlo->stop();
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
                usleep(200000);
                if(!a_thread_args.m_settings.m_quiet && !a_thread_args.m_settings.m_verbose)
                {
                        if(l_first_time)
                        {
                                l_hlo->display_results_line_desc(a_thread_args.m_settings.m_color);
                                l_first_time = false;
                        }
                        l_hlo->display_results_line(a_thread_args.m_settings.m_color);
                }

                if (!l_hlo->is_running())
                {
                        g_test_finished = true;
                }

        }

        // Send stop -if unsent
        if(!l_sent_stop)
        {
                l_hlo->stop();
                l_sent_stop = true;
        }

        nonblock(NB_DISABLE);

}
