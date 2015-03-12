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
#include "ndebug.h"

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
#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

// Json parser
#include <jsoncpp/json/json.h>

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

#define MAX_READLINE_SIZE 1024

// Version
#define HLE_VERSION_MAJOR 0
#define HLE_VERSION_MINOR 0
#define HLE_VERSION_MACRO 1
#define HLE_VERSION_PATCH "alpha"

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Settings
//: ----------------------------------------------------------------------------
typedef struct settings_struct
{
        bool m_verbose;
        bool m_color;
        bool m_quiet;
        bool m_show_stats;
        bool m_show_summary;

        ns_hlx::hlx_client *m_hlx_client;

        // TODO ???
#if 0
        // request options
        std::string m_url;
        header_map_t m_header_map;

        // run options
        t_client_list_t m_t_client_list;
        evr_loop_type_t m_evr_loop_type;
        int32_t m_start_parallel;
        uint32_t m_num_threads;
        uint32_t m_timeout_s;
        bool m_connect_only;

        // tcp options
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;

        // SSL options
        SSL_CTX* m_ssl_ctx;
        std::string m_cipher_list_str;
        std::string m_ssl_options_str;
        long m_ssl_options;
        bool m_ssl_verify;
        bool m_ssl_sni;
        std::string m_ssl_ca_file;
        std::string m_ssl_ca_path;
#endif

        // ---------------------------------
        // Defaults...
        // ---------------------------------
        settings_struct(void) :
                m_verbose(false),
                m_color(false),
                m_quiet(false),
                m_show_stats(false),
                m_show_summary(false),
                m_hlx_client(NULL)

                // TODO ???
#if 0
                m_url(),
                m_header_map(),
                m_t_client_list(),
                m_evr_loop_type(EVR_LOOP_EPOLL),
                m_start_parallel(128),
                m_num_threads(8),
                m_timeout_s(HLE_DEFAULT_CONN_TIMEOUT_S),
                m_connect_only(false),
                m_sock_opt_recv_buf_size(0),
                m_sock_opt_send_buf_size(0),
                m_sock_opt_no_delay(false),
                m_ssl_ctx(NULL),
                m_cipher_list_str(""),
                m_ssl_options_str(""),
                m_ssl_options(0),
                m_ssl_verify(false),
                m_ssl_sni(false),
                m_ssl_ca_file(""),
                m_ssl_ca_path("")
#endif
        {}

private:
        HLX_CLIENT_DISALLOW_COPY_AND_ASSIGN(settings_struct);

} settings_struct_t;

//: ----------------------------------------------------------------------------
//: \details: Signal handler
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
                //NDBG_PRINT("SIGINT\n");
                g_test_finished = true;
                g_cancelled = true;
                g_settings->m_hlx_client->stop();
        }
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
// TODO PUT BACK!!!
void command_exec(settings_struct_t &a_settings)
{
        int i = 0;
        char l_cmd = ' ';
        bool l_sent_stop = false;
        //bool l_first_time = true;

        nonblock(NB_ENABLE);

        //: ------------------------------------
        //:   Loop forever until user quits
        //: ------------------------------------
        while (!g_test_finished)
        {
                i = kbhit();
                if (i != 0)
                {

                        l_cmd = fgetc(stdin);

                        switch (l_cmd)
                        {

                        // Quit -only works when not reading from stdin
                        case 'q':
                        {
                                g_test_finished = true;
                                g_cancelled = true;
                                a_settings.m_hlx_client->stop();
                                l_sent_stop = true;
                                break;
                        }

                        // Default
                        default:
                        {
                                break;
                        }
                        }
                }

                // TODO add define...
                usleep(200000);

                // TODO PUT BACK!!!
                if(a_settings.m_show_stats)
                {
                        a_settings.m_hlx_client->display_status_line(a_settings.m_color);
                }

                // TODO PUT BACK!!!
                if (!a_settings.m_hlx_client->is_running())
                {
                        //NDBG_PRINT("IS NOT RUNNING.\n");
                        g_test_finished = true;
                }

        }

        // Send stop -if unsent
        if(!l_sent_stop)
        {
                a_settings.m_hlx_client->stop();
                l_sent_stop = true;
        }

        nonblock(NB_DISABLE);

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t add_line(FILE *a_file_ptr, ns_hlx::host_list_t &a_host_list)
{

        char l_readline[MAX_READLINE_SIZE];
        while(fgets(l_readline, sizeof(l_readline), a_file_ptr))
        {
                size_t l_readline_len = strnlen(l_readline, MAX_READLINE_SIZE);
                if(MAX_READLINE_SIZE == l_readline_len)
                {
                        // line was truncated
                        // Bail out -reject lines longer than limit
                        // -host names ought not be too long
                        printf("Error: hostnames must be shorter than %d chars\n", MAX_READLINE_SIZE);
                        return STATUS_ERROR;
                }
                // read full line
                // Nuke endline
                l_readline[l_readline_len - 1] = '\0';
                std::string l_string(l_readline);
                l_string.erase( std::remove_if( l_string.begin(), l_string.end(), ::isspace ), l_string.end() );
                ns_hlx::host_t l_host;
                l_host.m_host = l_string;
                if(!l_string.empty())
                        a_host_list.push_back(l_host);
                //NDBG_PRINT("READLINE: %s\n", l_readline);
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
        fprintf(a_stream, "hle HTTP Load Runner.\n");
        fprintf(a_stream, "Copyright (C) 2014 Edgecast Networks.\n");
        fprintf(a_stream, "               Version: %d.%d.%d.%s\n",
                        HLE_VERSION_MAJOR,
                        HLE_VERSION_MINOR,
                        HLE_VERSION_MACRO,
                        HLE_VERSION_PATCH);
        exit(a_exit_code);

}

//: ----------------------------------------------------------------------------
//: \details: Print the command line help.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_usage(FILE* a_stream, int a_exit_code)
{
        fprintf(a_stream, "Usage: hle -u [http[s]://]hostname[:port]/path [options]\n");
        fprintf(a_stream, "Options are:\n");
        fprintf(a_stream, "  -h, --help           Display this help and exit.\n");
        fprintf(a_stream, "  -v, --version        Display the version number and exit.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "URL Options -or without parameter\n");
        fprintf(a_stream, "  -u, --url            URL -REQUIRED.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Hostname Input Options -also STDIN:\n");
        fprintf(a_stream, "  -f, --host_file      Host name file.\n");
        fprintf(a_stream, "  -x, --execute        Script to execute to get host names.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Settings:\n");
        fprintf(a_stream, "  -p, --parallel       Num parallel.\n");
        fprintf(a_stream, "  -t, --threads        Number of parallel threads.\n");
        fprintf(a_stream, "  -H, --header         Request headers -can add multiple ie -H<> -H<>...\n");
        fprintf(a_stream, "  -T, --timeout        Timeout (seconds).\n");
        fprintf(a_stream, "  -R, --recv_buffer    Socket receive buffer size.\n");
        fprintf(a_stream, "  -S, --send_buffer    Socket send buffer size.\n");
        fprintf(a_stream, "  -D, --no_delay       Socket TCP no-delay.\n");
        fprintf(a_stream, "  -A, --ai_cache       Path to Address Info Cache (DNS lookup cache).\n");
        fprintf(a_stream, "  -C, --connect_only   Only connect -do not send request.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "SSL Settings:\n");
        fprintf(a_stream, "  -y, --cipher         Cipher --see \"openssl ciphers\" for list.\n");
        fprintf(a_stream, "  -O, --ssl_options    SSL Options string.\n");
        fprintf(a_stream, "  -V, --ssl_verify     Verify server certificate.\n");
        fprintf(a_stream, "  -N, --ssl_sni        Use SSL SNI.\n");
        fprintf(a_stream, "  -F, --ssl_ca_file    SSL CA File.\n");
        fprintf(a_stream, "  -L, --ssl_ca_path    SSL CA Path.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Print Options:\n");
        fprintf(a_stream, "  -r, --verbose        Verbose logging\n");
        fprintf(a_stream, "  -c, --color          Color\n");
        fprintf(a_stream, "  -q, --quiet          Suppress output\n");
        fprintf(a_stream, "  -s, --show_progress  Show progress\n");
        fprintf(a_stream, "  -m, --show_summary   Show summary output\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Output Options: -defaults to line delimited\n");
        fprintf(a_stream, "  -o, --output         File to write output to. Defaults to stdout\n");
        fprintf(a_stream, "  -l, --line_delimited Output <HOST> <RESPONSE BODY> per line\n");
        fprintf(a_stream, "  -j, --json           JSON { <HOST>: \"body\": <RESPONSE> ...\n");
        fprintf(a_stream, "  -P, --pretty         Pretty output\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Debug Options:\n");
        fprintf(a_stream, "  -G, --gprofile       Google profiler output file\n");

        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Note: If running large jobs consider enabling tcp_tw_reuse -eg:\n");
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

        ns_hlx::hlx_client *l_hlx_client = ns_hlx::hlx_client::get();

        // For sighandler
        settings_struct_t l_settings;
        g_settings = &l_settings;

        // -------------------------------------------
        // Setup default headers before the user
        // -------------------------------------------
        l_hlx_client->set_header("User-Agent", "EdgeCast Parallel Curl hle ");

        //l_hlx_client->set_header("User-Agent", "ONGA_BONGA (╯°□°）╯︵ ┻━┻)");
        //l_hlx_client->set_header("User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/33.0.1750.117 Safari/537.36");
        //l_hlx_client->set_header("x-select-backend", "self");

        l_hlx_client->set_header("Accept", "*/*");

        //l_hlx_client->set_header("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8");
        //l_hlx_client->set_header("Accept-Encoding", "gzip,deflate");
        //l_hlx_client->set_header("Connection", "keep-alive");

        // -------------------------------------------
        // Get args...
        // -------------------------------------------
        char l_opt;
        std::string l_argument;
        int l_option_index = 0;
        struct option l_long_options[] =
                {
                { "help",           0, 0, 'h' },
                { "version",        0, 0, 'v' },
                { "url",            1, 0, 'u' },
                { "host_file",      1, 0, 'f' },
                { "host_file_json", 1, 0, 'J' },
                { "execute",        1, 0, 'x' },
                { "parallel",       1, 0, 'p' },
                { "threads",        1, 0, 't' },
                { "header",         1, 0, 'H' },
                { "timeout",        1, 0, 'T' },
                { "recv_buffer",    1, 0, 'R' },
                { "send_buffer",    1, 0, 'S' },
                { "no_delay",       1, 0, 'D' },
                { "ai_cache",       1, 0, 'A' },
                { "connect_only",   0, 0, 'C' },
                { "cipher",         1, 0, 'y' },
                { "ssl_options",    1, 0, 'O' },
                { "ssl_verify",     0, 0, 'V' },
                { "ssl_sni",        0, 0, 'N' },
                { "ssl_ca_file",    1, 0, 'F' },
                { "ssl_ca_path",    1, 0, 'L' },
                { "verbose",        0, 0, 'r' },
                { "color",          0, 0, 'c' },
                { "quiet",          0, 0, 'q' },
                { "show_progress",  0, 0, 's' },
                { "show_summary",   0, 0, 'm' },
                { "output",         1, 0, 'o' },
                { "line_delimited", 0, 0, 'l' },
                { "json",           0, 0, 'j' },
                { "pretty",         0, 0, 'P' },
                { "gprofile",       1, 0, 'G' },

                // list sentinel
                { 0, 0, 0, 0 }
        };

        std::string l_gprof_file;
        std::string l_execute_line;
        std::string l_host_file_str;
        std::string l_host_file_json_str;
        std::string l_url;
        std::string l_ai_cache;
        std::string l_output_file = "";

        // Defaults
        ns_hlx::output_type_t l_output_mode = ns_hlx::OUTPUT_JSON;
        int l_output_part =   ns_hlx::PART_HOST
                            | ns_hlx::PART_STATUS_CODE
                            | ns_hlx::PART_HEADERS
                            | ns_hlx::PART_BODY
                            ;
        bool l_output_pretty = false;
        // TODO PUT BACK!!!
        UNUSED(l_output_pretty);

        // -------------------------------------------
        // Assume unspecified arg url...
        // TODO Unsure if good way to allow unspecified
        // arg...
        // -------------------------------------------
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
                        break;
                } else {
                        // reset option flag
                        is_opt = false;
                }

        }

        // -------------------------------------------------
        // Args...
        // -------------------------------------------------
        char l_short_arg_list[] = "hvu:f:J:x:y:O:VNF:L:p:t:H:T:R:S:DA:Crcqsmo:ljPG:";
        while ((l_opt = getopt_long_only(argc, argv, l_short_arg_list, l_long_options, &l_option_index)) != -1)
        {

                if (optarg)
                {
                        l_argument = std::string(optarg);
                }
                else
                {
                        l_argument.clear();
                }
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
                // URL
                // ---------------------------------------
                case 'u':
                {
                        l_url = l_argument;
                        break;
                }
                // ---------------------------------------
                // Host file
                // ---------------------------------------
                case 'f':
                {
                        l_host_file_str = l_argument;
                        break;
                }
                // ---------------------------------------
                // Host file JSON
                // ---------------------------------------
                case 'J':
                {
                        l_host_file_json_str = l_argument;
                        break;
                }
                // ---------------------------------------
                // Execute line
                // ---------------------------------------
                case 'x':
                {
                        l_execute_line = l_argument;
                        break;
                }
                // ---------------------------------------
                // cipher
                // ---------------------------------------
                case 'y':
                {
                        std::string l_cipher_str = l_argument;
                        if (strcasecmp(l_cipher_str.c_str(), "fastsec") == 0)
                        {
                                l_cipher_str = "RC4-MD5";
                        }
                        else if (strcasecmp(l_cipher_str.c_str(), "highsec") == 0)
                        {
                                l_cipher_str = "DES-CBC3-SHA";
                        }
                        else if (strcasecmp(l_cipher_str.c_str(), "paranoid") == 0)
                        {
                                l_cipher_str = "AES256-SHA";
                        }
                        // TODO PUT BACK!!!
                        //l_settings.m_cipher_list_str = l_cipher_str;
                        break;
                }
                // ---------------------------------------
                // ssl options
                // ---------------------------------------
                case 'O':
                {
                        // TODO PUT BACK!!!
                        //l_settings.m_ssl_options_str = l_argument;

                        // TODO PUT BACK!!!
                        //int32_t l_status;
                        //l_status = get_ssl_options_str_val(l_settings.m_ssl_options_str, l_settings.m_ssl_options);
                        //if(l_status != STATUS_OK)
                        //{
                        //        return STATUS_ERROR;
                        //}
                        break;
                }
                // ---------------------------------------
                // ssl verify
                // ---------------------------------------
                case 'V':
                {
                        // TODO PUT BACK!!!
                        //l_settings.m_ssl_verify = true;
                        break;
                }
                // ---------------------------------------
                // ssl sni
                // ---------------------------------------
                case 'N':
                {
                        // TODO PUT BACK!!!
                        //l_settings.m_ssl_sni = true;
                        break;
                }
                // ---------------------------------------
                // ssl ca file
                // ---------------------------------------
                case 'F':
                {
                        // TODO PUT BACK!!!
                        //l_settings.m_ssl_ca_file = l_argument;
                        break;
                }
                // ---------------------------------------
                // ssl ca path
                // ---------------------------------------
                case 'L':
                {
                        // TODO PUT BACK!!!
                        //l_settings.m_ssl_ca_path = l_argument;
                        break;
                }
                // ---------------------------------------
                // parallel
                // ---------------------------------------
                case 'p':
                {
                        int l_start_parallel = 1;
                        //NDBG_PRINT("arg: --parallel: %s\n", optarg);
                        //l_settings.m_start_type = START_PARALLEL;
                        l_start_parallel = atoi(optarg);
                        if (l_start_parallel < 1)
                        {
                                printf("parallel must be at least 1\n");
                                print_usage(stdout, -1);
                        }

                        // TODO PUT BACK!!!
                        //l_settings.m_start_parallel = l_start_parallel;
                        break;
                }
                // ---------------------------------------
                // num threads
                // ---------------------------------------
                case 't':
                {
                        int l_max_threads = 1;
                        //NDBG_PRINT("arg: --threads: %s\n", l_argument.c_str());
                        l_max_threads = atoi(optarg);
                        if (l_max_threads < 1)
                        {
                                printf("num-threads must be at least 1\n");
                                print_usage(stdout, -1);
                        }
                        // TODO PUT BACK!!!
                        //l_settings.m_num_threads = l_max_threads;
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
                                print_usage(stdout, -1);
                        }
                        break;
                }
                // ---------------------------------------
                // Timeout
                // ---------------------------------------
                case 'T':
                {
                        int l_timeout_s = -1;
                        //NDBG_PRINT("arg: --threads: %s\n", l_argument.c_str());
                        l_timeout_s = atoi(optarg);
                        if (l_timeout_s < 1)
                        {
                                printf("connection timeout must be at least 1\n");
                                print_usage(stdout, -1);
                        }
                        // TODO PUT BACK!!!
                        //l_settings.m_timeout_s = l_timeout_s;
                        break;
                }
                // ---------------------------------------
                // sock_opt_recv_buf_size
                // ---------------------------------------
                case 'R':
                {
                        // TODO PUT BACK!!!
                        //int l_sock_opt_recv_buf_size = atoi(optarg);
                        //// TODO Check value...
                        //l_settings.m_sock_opt_recv_buf_size = l_sock_opt_recv_buf_size;
                        break;
                }
                // ---------------------------------------
                // sock_opt_send_buf_size
                // ---------------------------------------
                case 'S':
                {
                        // TODO PUT BACK!!!
                        //int l_sock_opt_send_buf_size = atoi(optarg);
                        //// TODO Check value...
                        //l_settings.m_sock_opt_send_buf_size = l_sock_opt_send_buf_size;
                        break;
                }
                // ---------------------------------------
                // No delay
                // ---------------------------------------
                case 'D':
                {
                        // TODO PUT BACK!!!
                        //l_settings.m_sock_opt_no_delay = true;
                        break;
                }
                // ---------------------------------------
                // Address Info cache
                // ---------------------------------------
                case 'A':
                {
                        l_ai_cache = l_argument;
                        break;
                }
                // ---------------------------------------
                // connect only
                // ---------------------------------------
                case 'C':
                {
                        // TODO PUT BACK!!!
                        //l_settings.m_connect_only = true;
                        break;
                }
                // ---------------------------------------
                // verbose
                // ---------------------------------------
                case 'r':
                {
                        // TODO PUT BACK!!!
                        //l_settings.m_verbose = true;
                        break;
                }
                // ---------------------------------------
                // color
                // ---------------------------------------
                case 'c':
                {
                        // TODO PUT BACK!!!
                        //l_settings.m_color = true;
                        break;
                }
                // ---------------------------------------
                // quiet
                // ---------------------------------------
                case 'q':
                {
                        // TODO PUT BACK!!!
                        //l_settings.m_quiet = true;
                        break;
                }
                // ---------------------------------------
                // show progress
                // ---------------------------------------
                case 's':
                {
                        // TODO PUT BACK!!!
                        //l_settings.m_show_stats = true;
                        break;
                }
                // ---------------------------------------
                // show progress
                // ---------------------------------------
                case 'm':
                {
                        // TODO PUT BACK!!!
                        //l_settings.m_show_summary = true;
                        break;
                }
                // ---------------------------------------
                // output file
                // ---------------------------------------
                case 'o':
                {
                        l_output_file = l_argument;
                        break;
                }
                // ---------------------------------------
                // line delimited
                // ---------------------------------------
                case 'l':
                {
                        l_output_mode = ns_hlx::OUTPUT_LINE_DELIMITED;
                        break;
                }
                // ---------------------------------------
                // json output
                // ---------------------------------------
                case 'j':
                {
                        l_output_mode = ns_hlx::OUTPUT_JSON;
                        break;
                }
                // ---------------------------------------
                // pretty output
                // ---------------------------------------
                case 'P':
                {
                        l_output_pretty = true;
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
                // What???
                case '?':
                {
                        // Required argument was missing
                        // '?' is provided when the 3rd arg to getopt_long does not begin with a ':', and is preceeded
                        // by an automatic error message.
                        fprintf(stdout, "  Exiting.\n");
                        print_usage(stdout, -1);
                        break;
                }
                // Huh???
                default:
                {
                        fprintf(stdout, "Unrecognized option.\n");
                        print_usage(stdout, -1);
                        break;
                }
                }
        }

        // Check for required url argument
        if(l_url.empty())
        {
                fprintf(stdout, "No URL specified.\n");
                print_usage(stdout, -1);
        }
        // else set url
        // TODO PUT BACK!!!
        //l_settings.m_url = l_url;

        ns_hlx::host_list_t l_host_list;
        // -------------------------------------------------
        // Host list processing
        // -------------------------------------------------
        // Read from command
        if(!l_execute_line.empty())
        {
                FILE *fp;
                int32_t l_status = STATUS_OK;

                fp = popen(l_execute_line.c_str(), "r");
                // Error executing...
                if (fp == NULL)
                {
                }

                l_status = add_line(fp, l_host_list);
                if(STATUS_OK != l_status)
                {
                        return STATUS_ERROR;
                }

                l_status = pclose(fp);
                // Error reported by pclose()
                if (l_status == -1)
                {
                        printf("Error: performing pclose\n");
                        return STATUS_ERROR;
                }
                // Use macros described under wait() to inspect `status' in order
                // to determine success/failure of command executed by popen()
                else
                {
                }
        }
        // Read from file
        else if(!l_host_file_str.empty())
        {
                FILE * l_file;
                int32_t l_status = STATUS_OK;

                l_file = fopen(l_host_file_str.c_str(),"r");
                if (NULL == l_file)
                {
                        printf("Error opening file: %s.  Reason: %s\n", l_host_file_str.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }

                l_status = add_line(l_file, l_host_list);
                if(STATUS_OK != l_status)
                {
                        return STATUS_ERROR;
                }

                //NDBG_PRINT("ADD_FILE: DONE: %s\n", a_url_file.c_str());

                l_status = fclose(l_file);
                if (0 != l_status)
                {
                        NDBG_PRINT("Error performing fclose.  Reason: %s\n", strerror(errno));
                        return STATUS_ERROR;
                }
        }
        else if(!l_host_file_json_str.empty())
        {
                // TODO Create a function to do all this mess
                // ---------------------------------------
                // Check is a file
                // TODO
                // ---------------------------------------
                struct stat l_stat;
                int32_t l_status = STATUS_OK;
                l_status = stat(l_host_file_json_str.c_str(), &l_stat);
                if(l_status != 0)
                {
                        NDBG_PRINT("Error performing stat on file: %s.  Reason: %s\n", l_host_file_json_str.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }

                // Check if is regular file
                if(!(l_stat.st_mode & S_IFREG))
                {
                        NDBG_PRINT("Error opening file: %s.  Reason: is NOT a regular file\n", l_host_file_json_str.c_str());
                        return STATUS_ERROR;
                }

                // ---------------------------------------
                // Open file...
                // ---------------------------------------
                FILE * l_file;
                l_file = fopen(l_host_file_json_str.c_str(),"r");
                if (NULL == l_file)
                {
                        NDBG_PRINT("Error opening file: %s.  Reason: %s\n", l_host_file_json_str.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }

                // ---------------------------------------
                // Read in file...
                // ---------------------------------------
                int32_t l_size = l_stat.st_size;
                int32_t l_read_size;
                char *l_buf = (char *)malloc(sizeof(char)*l_size);
                l_read_size = fread(l_buf, 1, l_size, l_file);
                if(l_read_size != l_size)
                {
                        NDBG_PRINT("Error performing fread.  Reason: %s [%d:%d]\n", strerror(errno), l_read_size, l_size);
                        return STATUS_ERROR;
                }
                std::string l_buf_str = l_buf;

                Json::Value l_json_value(Json::objectValue);
                Json::Reader l_json_reader;
                bool l_result = l_json_reader.parse(l_buf_str, l_json_value);
                if (!l_result)
                {
                        NDBG_PRINT("Failed to parse JSON document: %s. Reason: %s\n", l_host_file_json_str.c_str(), l_json_reader.getFormattedErrorMessages().c_str());
                        fclose(l_file);
                        // Best effort -not checking return cuz we outtie
                        return STATUS_ERROR;
                }

                // For each line add
                for( Json::ValueIterator itr = l_json_value.begin() ; itr != l_json_value.end() ; itr++ )
                {
                        const Json::Value &l_value = (*itr);

                        if(l_value.isObject())
                        {
                                ns_hlx::host_t l_host;

                                //
                                // "host" : "irobdownload.blob.core.windows.net:443",
                                // "hostname" : "irobdownload.blob.core.windows.net",
                                // "id" : "DE4D",
                                // "where" : "edge"

                                l_host.m_host = l_value.get("host", "NO_HOST").asString();
                                l_host.m_hostname = l_value.get("hostname", "NO_HOSTNAME").asString();
                                l_host.m_id = l_value.get("id", "NO_ID").asString();
                                l_host.m_where = l_value.get("where", "NO_WHERE").asString();
                                // TODO Check exist...
                                l_host_list.push_back(l_host);
                        }

                }

                // ---------------------------------------
                // Close file...
                // ---------------------------------------
                l_status = fclose(l_file);
                if (STATUS_OK != l_status)
                {
                        NDBG_PRINT("Error performing fclose.  Reason: %s\n", strerror(errno));
                        return STATUS_ERROR;
                }
        }
        // Read from stdin
        else
        {
                int32_t l_status = STATUS_OK;
                l_status = add_line(stdin, l_host_list);
                if(STATUS_OK != l_status)
                {
                        return STATUS_ERROR;
                }
        }

        if(l_settings.m_verbose)
        {
                NDBG_PRINT("Showing hostname list:\n");
                for(ns_hlx::host_list_t::iterator i_host = l_host_list.begin(); i_host != l_host_list.end(); ++i_host)
                {
                        NDBG_OUTPUT("%s\n", i_host->m_host.c_str());
                }
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
        // Init resolver with cache
        // -------------------------------------------
        // TODO PUT BACK!!!
        //int32_t l_ldb_init_status;
        //l_ldb_init_status = resolver::get()->init(l_ai_cache, true);
        //if(STATUS_OK != l_ldb_init_status)
        //{
        //        return -1;
        //}

        // -------------------------------------------
        // SSL init...
        // -------------------------------------------
        // TODO PUT BACK!!!
        //l_settings.m_ssl_ctx = ssl_init(l_settings.m_cipher_list_str, // ctx cipher list str
        //                                l_settings.m_ssl_options,     // ctx options
        //                                l_settings.m_ssl_ca_file,     // ctx ca file
        //                                l_settings.m_ssl_ca_path);    // ctx ca path
        //if(NULL == l_settings.m_ssl_ctx)
        //{
        //        NDBG_PRINT("Error: performing ssl_init with cipher_list: %s\n", l_settings.m_cipher_list_str.c_str());
        //        return STATUS_ERROR;
        //}

        // Start Profiler
        if (!l_gprof_file.empty())
        {
                ProfilerStart(l_gprof_file.c_str());
        }

        // Run
        // TODO PUT BACK!!!
        //int32_t l_run_status = 0;
        //l_run_status = run(l_settings, l_host_list);
        //if(0 != l_run_status)
        //{
        //        printf("Error: performing hle::run");
        //        return -1;
        //}

        //uint64_t l_start_time_ms = get_time_ms();

        // -------------------------------------------
        // Run command exec
        // -------------------------------------------
        command_exec(l_settings);

        // TODO PUT BACK!!!
        //if(l_settings.m_verbose)
        //{
        //        printf("Finished -joining all threads\n");
        //}

        // Wait for completion
        l_hlx_client->wait_till_stopped();

        // One more status for the lovers
        // TODO PUT BACK!!!
        l_hlx_client->display_status_line(l_settings.m_color);

        if (!l_gprof_file.empty())
        {
                ProfilerStop();
        }

        //uint64_t l_end_time_ms = get_time_ms() - l_start_time_ms;

        // -------------------------------------------
        // Results...
        // -------------------------------------------
        if(!g_cancelled && !l_settings.m_quiet)
        {
                bool l_use_color = l_settings.m_color;
                if(!l_output_file.empty()) l_use_color = false;
                std::string l_responses_str;
                l_responses_str = l_hlx_client->dump_all_responses(l_use_color, l_output_pretty, l_output_mode, l_output_part);
                if(l_output_file.empty())
                {
                        NDBG_OUTPUT("%s\n", l_responses_str.c_str());
                }
                else
                {
                        int32_t l_num_bytes_written = 0;
                        int32_t l_status = 0;
                        // Open
                        FILE *l_file_ptr = fopen(l_output_file.c_str(), "w+");
                        if(l_file_ptr == NULL)
                        {
                                NDBG_PRINT("Error performing fopen. Reason: %s\n", strerror(errno));
                                return STATUS_ERROR;
                        }

                        // Write
                        l_num_bytes_written = fwrite(l_responses_str.c_str(), 1, l_responses_str.length(), l_file_ptr);
                        if(l_num_bytes_written != (int32_t)l_responses_str.length())
                        {
                                NDBG_PRINT("Error performing fwrite. Reason: %s\n", strerror(errno));
                                fclose(l_file_ptr);
                                return STATUS_ERROR;
                        }

                        // Close
                        l_status = fclose(l_file_ptr);
                        if(l_status != 0)
                        {
                                NDBG_PRINT("Error performing fclose. Reason: %s\n", strerror(errno));
                                return STATUS_ERROR;
                        }
                }
        }

        // -------------------------------------------
        // Summary...
        // -------------------------------------------
        if(l_settings.m_show_summary)
        {
                l_hlx_client->display_summary(l_settings.m_color);
        }

        // -------------------------------------------
        // Cleanup...
        // -------------------------------------------
        // Delete t_client list...
        // TODO PUT BACK!!!
        //for(t_client_list_t::iterator i_client_hle = l_settings.m_t_client_list.begin();
        //                i_client_hle != l_settings.m_t_client_list.end(); )
        //{
        //        t_client *l_t_client_ptr = *i_client_hle;
        //        delete l_t_client_ptr;
        //        l_settings.m_t_client_list.erase(i_client_hle++);
        //}

        // SSL Cleanup
        // TODO PUT BACK!!!
        //ssl_kill_locks();

        // TODO Deprecated???
        //EVP_cleanup();

        //if(l_settings.m_verbose)
        //{
        //      NDBG_PRINT("Cleanup\n");
        //}

        return 0;

}

