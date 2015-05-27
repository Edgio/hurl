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

// Profiler
#define ENABLE_PROFILER 1
#ifdef ENABLE_PROFILER
#include <google/profiler.h>
#endif

// json support
#include "rapidjson/document.h"

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
        bool m_cli;
        ns_hlx::hlx_client *m_hlx_client;

        // ---------------------------------
        // Defaults...
        // ---------------------------------
        settings_struct(void) :
                m_verbose(false),
                m_color(false),
                m_quiet(false),
                m_show_stats(false),
                m_show_summary(false),
                m_cli(false),
                m_hlx_client(NULL)
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
int command_exec(settings_struct_t &a_settings)
{
        int i = 0;
        char l_cmd = ' ';
        bool l_sent_stop = false;
        //bool l_first_time = true;

        nonblock(NB_ENABLE);

        // run...
        int l_status;
        l_status = a_settings.m_hlx_client->run();
        if(HLX_CLIENT_STATUS_OK != l_status)
        {
                return -1;
        }

        g_test_finished = false;

        // ---------------------------------------
        //   Loop forever until user quits
        // ---------------------------------------
        while (!g_test_finished)
        {
                i = kbhit();
                if (i != 0)
                {
                        l_cmd = fgetc(stdin);

                        switch (l_cmd)
                        {

                        // -------------------------------------------
                        // Quit
                        // -only works when not reading from stdin
                        // -------------------------------------------
                        case 'q':
                        {
                                g_test_finished = true;
                                g_cancelled = true;
                                a_settings.m_hlx_client->stop();
                                l_sent_stop = true;
                                break;
                        }

                        // -------------------------------------------
                        // Default
                        // -------------------------------------------
                        default:
                        {
                                break;
                        }
                        }
                }

                // TODO add define...
                usleep(200000);

                if(a_settings.m_show_stats)
                {
                        a_settings.m_hlx_client->display_status_line(a_settings.m_color);
                }

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

        // wait for completion...
        a_settings.m_hlx_client->wait_till_stopped();

        // One more status for the lovers
        if(a_settings.m_show_stats)
        {
                a_settings.m_hlx_client->display_status_line(a_settings.m_color);
        }

        nonblock(NB_DISABLE);

        return 0;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void show_help(void)
{
        printf(" hle commands: \n");
        printf("  h    Help or ?\n");
        printf("  r    Run\n");
        printf("  q    Quit\n");
}

#define MAX_CMD_SIZE 64
int command_exec_cli(settings_struct_t &a_settings)
{
        bool l_done = false;

        // Set to keep-alive -and reuse
        a_settings.m_hlx_client->set_header("Connection","keep-alive");
        a_settings.m_hlx_client->set_num_reqs_per_conn(-1);
        a_settings.m_hlx_client->set_conn_reuse(true);

        // -------------------------------------------
        // Interactive mode banner
        // -------------------------------------------
        if(a_settings.m_color)
        {
                printf("%shle interactive mode%s: (%stype h for command help%s)\n",
                                ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF);
        }
        else
        {
                printf("hle interactive mode: (type h for command help)\n");
        }

        // ---------------------------------------
        //   Loop forever until user quits
        // ---------------------------------------
        while (!l_done && !g_cancelled)
        {

                // -------------------------------------------
                // Interactive mode prompt
                // -------------------------------------------
                if(a_settings.m_color)
                {
                        printf("%shle>>%s", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF);
                }
                else
                {
                        printf("hle>>");
                }
                fflush(stdout);

                char l_cmd[MAX_CMD_SIZE] = {' '};
                char *l_status;
                l_status = fgets(l_cmd, MAX_CMD_SIZE, stdin);
                if(!l_status)
                {
                        printf("Error reading cmd from stdin\n");
                        return -1;
                }

                switch (l_cmd[0])
                {
                // -------------------------------------------
                // Quit
                // -only works when not reading from stdin
                // -------------------------------------------
                case 'q':
                {
                        l_done = true;
                        break;
                }
                // -------------------------------------------
                // run
                // -------------------------------------------
                case 'r':
                {
                        int l_status;
                        l_status = command_exec(a_settings);
                        if(l_status != 0)
                        {
                                return -1;
                        }
                        break;
                }
                // -------------------------------------------
                // Help
                // -------------------------------------------
                case 'h':
                case '?':
                {
                        show_help();
                        break;
                }

                // -------------------------------------------
                // Default
                // -------------------------------------------
                default:
                {
                        break;
                }
                }

        }

        return 0;

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
        fprintf(a_stream, "  -r, --version        Display the version number and exit.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "URL Options -or without parameter\n");
        fprintf(a_stream, "  -u, --url            URL -REQUIRED (unless running cli: see --cli option).\n");
        fprintf(a_stream, "  -d, --data           HTTP body data -supports bodies up to 8k.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Hostname Input Options -also STDIN:\n");
        fprintf(a_stream, "  -f, --host_file      Host name file.\n");
        fprintf(a_stream, "  -x, --execute        Script to execute to get host names.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Settings:\n");
        fprintf(a_stream, "  -p, --parallel       Num parallel.\n");
        fprintf(a_stream, "  -t, --threads        Number of parallel threads.\n");
        fprintf(a_stream, "  -H, --header         Request headers -can add multiple ie -H<> -H<>...\n");
        fprintf(a_stream, "  -X, --verb           Request command -HTTP verb to use -GET/PUT/etc\n");
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

        fprintf(a_stream, "Command Line Client:\n");
        fprintf(a_stream, "  -I, --cli            Start interactive command line -URL not required.\n");

        fprintf(a_stream, "Print Options:\n");
        fprintf(a_stream, "  -v, --verbose        Verbose logging\n");
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
        settings_struct_t l_settings;
        ns_hlx::hlx_client *l_hlx_client = new ns_hlx::hlx_client();
        l_settings.m_hlx_client = l_hlx_client;

        // For sighandler
        g_settings = &l_settings;

        // Turn on wildcarding by default
        l_hlx_client->set_wildcarding(false);
        l_hlx_client->set_split_requests_by_thread(true);
        l_hlx_client->set_collect_stats(false);
        l_hlx_client->set_save_response(true);
        l_hlx_client->set_use_ai_cache(true);

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
                { "version",        0, 0, 'r' },
                { "url",            1, 0, 'u' },
                { "data",           1, 0, 'd' },
                { "host_file",      1, 0, 'f' },
                { "host_file_json", 1, 0, 'J' },
                { "execute",        1, 0, 'x' },
                { "parallel",       1, 0, 'p' },
                { "threads",        1, 0, 't' },
                { "header",         1, 0, 'H' },
                { "verb",           1, 0, 'X' },
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
                { "cli",            0, 0, 'I' },
                { "verbose",        0, 0, 'v' },
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
        bool l_cli = false;

        // Defaults
        ns_hlx::output_type_t l_output_mode = ns_hlx::OUTPUT_JSON;
        int l_output_part =   ns_hlx::PART_HOST
                            | ns_hlx::PART_SERVER
                            | ns_hlx::PART_STATUS_CODE
                            | ns_hlx::PART_HEADERS
                            | ns_hlx::PART_BODY
                            ;
        bool l_output_pretty = false;

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
        char l_short_arg_list[] = "hvu:d:f:J:x:y:O:VNF:L:Ip:t:H:X:T:R:S:DA:Crcqsmo:ljPG:";
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
                case 'r':
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
                // Data
                // ---------------------------------------
                case 'd':
                {
                        int32_t l_status;
                        l_status = l_hlx_client->set_data(l_argument.c_str(), l_argument.length());
                        if(l_status != HLX_CLIENT_STATUS_OK)
                        {
                                printf("Error setting HTTP body data with: %s\n", l_argument.c_str());
                                //print_usage(stdout, -1);
                                return -1;
                        }
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
                        l_hlx_client->set_ssl_cipher_list(l_cipher_str);
                        break;
                }
                // ---------------------------------------
                // ssl options
                // ---------------------------------------
                case 'O':
                {
                        int32_t l_status;
                        l_status = l_hlx_client->set_ssl_options(l_argument);
                        if(l_status != STATUS_OK)
                        {
                                return STATUS_ERROR;
                        }

                        break;
                }
                // ---------------------------------------
                // ssl verify
                // ---------------------------------------
                case 'V':
                {
                        l_hlx_client->set_ssl_verify(true);
                        break;
                }
                // ---------------------------------------
                // ssl sni
                // ---------------------------------------
                case 'N':
                {

                        l_hlx_client->set_ssl_sni_verify(true);
                        break;
                }
                // ---------------------------------------
                // ssl ca file
                // ---------------------------------------
                case 'F':
                {
                        l_hlx_client->set_ssl_ca_file(l_argument);
                        break;
                }
                // ---------------------------------------
                // ssl ca path
                // ---------------------------------------
                case 'L':
                {
                        l_hlx_client->set_ssl_ca_path(l_argument);
                        break;
                }
                // ---------------------------------------
                // cli
                // ---------------------------------------
                case 'I':
                {
                        l_settings.m_cli = true;
                        break;
                }

                // ---------------------------------------
                // parallel
                // ---------------------------------------
                case 'p':
                {
                        int l_num_parallel = 1;
                        //NDBG_PRINT("arg: --parallel: %s\n", optarg);
                        //l_settings.m_start_type = START_PARALLEL;
                        l_num_parallel = atoi(optarg);
                        if (l_num_parallel < 1)
                        {
                                printf("parallel must be at least 1\n");
                                print_usage(stdout, -1);
                        }

                        l_hlx_client->set_num_parallel(l_num_parallel);
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
                                print_usage(stdout, -1);
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
                                //print_usage(stdout, -1);
                                return -1;
                        }
                        l_hlx_client->set_verb(l_argument);
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
                                printf("connection timeout must be > 0\n");
                                print_usage(stdout, -1);
                        }
                        l_hlx_client->set_timeout_s(l_timeout_s);
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
                // Address Info cache
                // ---------------------------------------
                case 'A':
                {
                        l_hlx_client->set_ai_cache(l_argument);
                        break;
                }
                // ---------------------------------------
                // connect only
                // ---------------------------------------
                case 'C':
                {
                        l_hlx_client->set_connect_only(true);
                        break;
                }
                // ---------------------------------------
                // verbose
                // ---------------------------------------
                case 'v':
                {
                        l_settings.m_verbose = true;
                        l_hlx_client->set_verbose(true);
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
                // show progress
                // ---------------------------------------
                case 's':
                {
                        l_settings.m_show_stats = true;
                        break;
                }
                // ---------------------------------------
                // show progress
                // ---------------------------------------
                case 'm':
                {
                        l_settings.m_show_summary = true;
                        l_hlx_client->set_show_summary(true);
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
        if(l_url.empty() && !l_cli)
        {
                fprintf(stdout, "No URL specified.\n");
                print_usage(stdout, -1);
        }
        // else set url
        if(!l_url.empty())
        {
                int l_status;
                l_status = l_hlx_client->set_url(l_url);
                if(HLX_CLIENT_STATUS_OK != l_status)
                {
                        printf("Error: performing set_url with url: %s.\n", l_url.c_str());
                        return -1;
                }
        }


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
                        NDBG_PRINT("Error performing fread.  Reason: %s [%d:%d]\n",
                                        strerror(errno), l_read_size, l_size);
                        return STATUS_ERROR;
                }
                std::string l_buf_str;
                l_buf_str.assign(l_buf, l_size);


                // NOTE: rapidjson assert's on errors -interestingly
                rapidjson::Document l_doc;
                l_doc.Parse(l_buf_str.c_str());
                if(!l_doc.IsArray())
                {
                        NDBG_PRINT("Error reading json from file: %s.  Reason: data is not an array\n",
                                        l_host_file_json_str.c_str());
                        return STATUS_ERROR;
                }

                // rapidjson uses SizeType instead of size_t.
                for(rapidjson::SizeType i_record = 0; i_record < l_doc.Size(); ++i_record)
                {
                        if(!l_doc[i_record].IsObject())
                        {
                                NDBG_PRINT("Error reading json from file: %s.  Reason: array membe not an object\n",
                                                l_host_file_json_str.c_str());
                                return STATUS_ERROR;
                        }

                        ns_hlx::host_t l_host;

                        // "host" : "irobdownload.blob.core.windows.net:443",
                        // "hostname" : "irobdownload.blob.core.windows.net",
                        // "id" : "DE4D",
                        // "where" : "edge"

                        if(l_doc[i_record].HasMember("host")) l_host.m_host = l_doc[i_record]["host"].GetString();
                        else l_host.m_host = "NO_HOST";

                        if(l_doc[i_record].HasMember("hostname")) l_host.m_hostname = l_doc[i_record]["hostname"].GetString();
                        else l_host.m_hostname = "NO_HOSTNAME";

                        if(l_doc[i_record].HasMember("id")) l_host.m_id = l_doc[i_record]["id"].GetString();
                        else l_host.m_id = "NO_ID";

                        if(l_doc[i_record].HasMember("where")) l_host.m_hostname = l_doc[i_record]["where"].GetString();
                        else l_host.m_where = "NO_WHERE";

                        l_host_list.push_back(l_host);
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

        // Initializer hlx_client
        int l_status = 0;

        // Set host list
        l_status = l_hlx_client->set_host_list(l_host_list);
        if(HLX_CLIENT_STATUS_OK != l_status)
        {
                printf("Error: performing set_host_list.\n");
                return -1;
        }

        // Start Profiler
        if (!l_gprof_file.empty())
        {
                ProfilerStart(l_gprof_file.c_str());
        }

        // -------------------------------------------
        // Run command exec
        // -------------------------------------------
        if(l_settings.m_cli)
        {
                l_status = command_exec_cli(l_settings);
                if(l_status != 0)
                {
                        return -1;
                }
        }
        else
        {
                l_status = command_exec(l_settings);
                if(l_status != 0)
                {
                        return -1;
                }
        }

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
        if(l_hlx_client)
        {
                delete l_hlx_client;
                l_hlx_client = NULL;
        }

        //if(l_settings.m_verbose)
        //{
        //      NDBG_PRINT("Cleanup\n");
        //}

        return 0;

}
