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
#include "hlp.h"
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
//#define ENABLE_PROFILER 1
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
                settings_struct m_settings;

        thread_args_struct() :
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
        fprintf(a_stream, "hlp HTTP Playback.\n");
        fprintf(a_stream, "Copyright (C) 2014 Edgecast Networks.\n");
        fprintf(a_stream, "               Version: %d.%d.%d.%s\n",
                        HLP_VERSION_MAJOR,
                        HLP_VERSION_MINOR,
                        HLP_VERSION_MACRO,
                        HLP_VERSION_PATCH);
        exit(a_exit_code);

}


//: ----------------------------------------------------------------------------
//: \details: Print the command line help.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_usage(FILE* a_stream, int a_exit_code)
{
        fprintf(a_stream, "Usage: hlp [http[s]://]hostname[:port]/path [options]\n");
        fprintf(a_stream, "Options are:\n");
        fprintf(a_stream, "  -h, --help         Display this help and exit.\n");
        fprintf(a_stream, "  -v, --version      Display the version number and exit.\n");

        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Playback Options:\n");
        fprintf(a_stream, "  -a, --playback     Playback file.\n");
        fprintf(a_stream, "  -I, --pb_dest_addr Hard coded destination address for playback.\n");
        fprintf(a_stream, "  -o, --pb_dest_port Hard coded destination port for playback.\n");
        fprintf(a_stream, "  -s, --scale        Time scaling (float).\n");
        fprintf(a_stream, "  -t, --threads      Number of threads.\n");

        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Settings:\n");
        fprintf(a_stream, "  -y, --cipher       Cipher --see \"openssl ciphers\" for list.\n");
        fprintf(a_stream, "  -H, --header       Request headers -can add multiple ie -H<> -H<>...\n");

        //fprintf(a_stream, "  -c, --checksum   Checksum.\n");
        //fprintf(a_stream, "  -t, --timeout    t_client_connection Timeout (seconds).\n");

        fprintf(a_stream, "  -R, --recv_buffer  Socket receive buffer size.\n");
        fprintf(a_stream, "  -S, --send_buffer  Socket send buffer size.\n");
        fprintf(a_stream, "  -D, --no_delay     Socket TCP no-delay.\n");
        fprintf(a_stream, "  -T, --timeout      Timeout (seconds).\n");

        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Print Options:\n");
        fprintf(a_stream, "  -x, --verbose      Verbose logging\n");
        fprintf(a_stream, "  -c, --color        Color\n");
        fprintf(a_stream, "  -q, --quiet        Suppress progress output\n");
        fprintf(a_stream, "  -e, --extra_info   Extra Info output\n");

        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Stat Options:\n");
        fprintf(a_stream, "  -B, --breakdown    Show breakdown\n");

        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Example:\n");
        fprintf(a_stream, "  hlp -a sample.txt -I 127.0.0.1 -c\n");

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

        // Get hlp instance
        hlp *l_hlp = hlp::get();

        settings_struct_t l_settings;
        thread_args_struct_t l_thread_args;
        bool l_show_breakdown = false;
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
                { "playback",     1, 0, 'a' },
                { "pb_dest_addr", 1, 0, 'I' },
                { "pb_dest_port", 1, 0, 'o' },
                { "scale",        1, 0, 's' },
                { "threads",      1, 0, 't' },
                { "cipher",       1, 0, 'y' },
                { "header",       1, 0, 'H' },
                { "recv_buffer",  1, 0, 'R' },
                { "send_buffer",  1, 0, 'S' },
                { "no_delay",     0, 0, 'D' },
                { "timeout",      1, 0, 'T' },
                { "verbose",      0, 0, 'x' },
                { "color",        0, 0, 'c' },
                { "quiet",        0, 0, 'q' },
                { "extra_info",   1, 0, 'e' },
                { "breakdown",    0, 0, 'B' },

                // list sentinel
                { 0, 0, 0, 0 }
        };


        std::string l_playback_file_str;
        std::string l_pb_dest_addr;
        int32_t l_pb_dest_port = -1;

        while ((l_opt = getopt_long_only(argc, argv, "hva:I:o:s:t:y:H:R:S:DT:xcqe:B", l_long_options, &l_option_index)) != -1)
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
                        print_usage(stdout, 0);
                        break;

                // ---------------------------------------
                // Version
                // ---------------------------------------
                case 'v':
                        print_version(stdout, 0);
                        break;

                // ---------------------------------------
                // Playback File
                // ---------------------------------------
                case 'a':
                {
                        l_playback_file_str = l_argument;
                        l_input_flag = true;
                        break;
                }

                // ---------------------------------------
                // Playback Destination Address
                // ---------------------------------------
                case 'I':
                        l_pb_dest_addr = l_argument;
                        break;

                // ---------------------------------------
                // Playback Destination Port
                // ---------------------------------------
                case 'o':
                        l_pb_dest_port = (uint16_t)atoi(optarg);
                        if (l_pb_dest_port < 1)
                        {
                                printf("Error: HTTP Data port must be > 0\n");
                                print_usage(stdout, -1);
                        }
                        break;

                // ---------------------------------------
                // Time scaling
                // ---------------------------------------
                case 's':
                {
                        float l_scale;
                        sscanf(optarg, "%f", &l_scale);
                        // TODO Check return...
                        l_hlp->set_scale(l_scale);
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
                        l_hlp->set_cipher_list(l_cipher_str);
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

                        // Add to reqlet_repo map
                        l_hlp->set_header(l_header_key, l_header_val);
                        // TODO Check status???

                        break;
                }

                // ---------------------------------------
                // sock_opt_recv_buf_size
                // ---------------------------------------
                case 'R':
                {
                        int l_sock_opt_recv_buf_size = atoi(optarg);
                        // TODO Check value...
                        l_hlp->set_sock_opt_recv_buf_size(l_sock_opt_recv_buf_size);

                        break;
                }

                // ---------------------------------------
                // sock_opt_send_buf_size
                // ---------------------------------------
                case 'S':
                {
                        int l_sock_opt_send_buf_size = atoi(optarg);
                        // TODO Check value...
                        l_hlp->set_sock_opt_send_buf_size(l_sock_opt_send_buf_size);
                        break;
                }

                // ---------------------------------------
                // threads
                // ---------------------------------------
                case 't':
                {
                        int32_t l_num_threads = 0;

                        l_num_threads = atoi(optarg);
                        if (l_num_threads < 1)
                        {
                                printf("Error: threads must be > 0\n");
                                print_usage(stdout, -1);
                        }
                        l_hlp->set_num_threads((uint32_t)l_num_threads);
                        break;
                }

                // ---------------------------------------
                // No delay
                // ---------------------------------------
                case 'D':
                {
                        l_hlp->set_sock_opt_no_delay(true);
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
                        l_hlp->set_timeout_s(l_timeout);

                        break;
                }

                // ---------------------------------------
                // verbose
                // ---------------------------------------
                case 'x':
                {
                        l_settings.m_verbose = true;
                        l_hlp->set_verbose(true);
                        break;
                }

                // ---------------------------------------
                // color
                // ---------------------------------------
                case 'c':
                {
                        l_settings.m_color = true;
                        l_hlp->set_color(true);
                        break;
                }

                // ---------------------------------------
                // quiet
                // ---------------------------------------
                case 'q':
                {
                        l_settings.m_quiet = true;
                        l_hlp->set_quiet(true);
                        break;
                }

                // ---------------------------------------
                // Extra Info
                // ---------------------------------------
                case 'e':
                {
                        int32_t l_extra_info = 0;

                        l_extra_info = atoi(optarg);
                        if (l_extra_info < 1)
                        {
                                printf("Error: Extra Info must be > 0\n");
                                print_usage(stdout, -1);
                        }
                        l_hlp->set_extra_info(l_extra_info);
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

        // Verify input
        if(!l_input_flag)
        {
                fprintf(stdout, "Error: Input url/url file/playback file required.");
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
        // Required playback file???
        // -------------------------------------------
        if(!l_playback_file_str.length()) {
                fprintf(stdout, "Error: No specified playback file on cmd line or in file with -a.\n");
                print_usage(stdout, -1);

        }

#ifdef ENABLE_PROFILER
        // Start Profiler
        ProfilerStart("/tmp/my_cool_prof.bin");
#endif

        // Run
        int32_t l_run_status = 0;

        // Message
        fprintf(stdout, "Running playback file: %s\n", l_playback_file_str.c_str());

        l_run_status = l_hlp->run(l_playback_file_str, l_pb_dest_addr, l_pb_dest_port);
        if(0 != l_run_status)
        {
                printf("Error: performing hlp::run_playback_file");
                return -1;
        }

        uint64_t l_start_time_ms = get_time_ms();

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
        //l_hlp->wait_till_stopped();

#ifdef ENABLE_PROFILER
        ProfilerStop();
#endif

        uint64_t l_end_time_ms = get_time_ms() - l_start_time_ms;


        // -------------------------------------------
        // Stats summary
        // -------------------------------------------
        l_hlp->display_results(((double)l_end_time_ms)/1000.0, 100, l_show_breakdown);

        //if(l_settings.m_verbose)
        //{
        //        NDBG_PRINT("Cleanup\n");
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
        hlp *l_hlp = hlp::get();
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
                                l_hlp->stop();
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
                //if(!a_thread_args.m_settings.m_quiet && !a_thread_args.m_settings.m_verbose)
                {
                        if(l_first_time)
                        {
                                l_hlp->display_results_line_desc(a_thread_args.m_settings.m_color);
                                l_first_time = false;
                        }
                        l_hlp->display_results_line(a_thread_args.m_settings.m_color);
                }

                if (!l_hlp->is_running())
                {
                        g_test_finished = true;
                }

        }

        printf("| <<<<----PLAYBACK COMPLETE---->>>>\n");

        // Send stop -if unsent
        if(!l_sent_stop)
        {
                l_hlp->stop();
                l_sent_stop = true;
        }

        nonblock(NB_DISABLE);

}




