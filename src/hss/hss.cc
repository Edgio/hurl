//: ----------------------------------------------------------------------------
//: file server example:
//: compile with:
//:   g++ files.cc -lhlxcore -lssl -lcrypto -lpthread -o files
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/hlx.h"
#include "hlx/stat.h"
#include "hlx/file_h.h"

// getrlimit
#include <sys/time.h>
#include <sys/resource.h>

// signal
#include <signal.h>

#include <list>

#include <stdio.h>
#include <stdlib.h>
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
#ifdef ENABLE_PROFILER
#include <google/profiler.h>
#include <google/heap-profiler.h>
#endif

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define NB_ENABLE  1
#define NB_DISABLE 0

// Version
#define HSS_VERSION_MAJOR 0
#define HSS_VERSION_MINOR 0
#define HSS_VERSION_MACRO 1
#define HSS_VERSION_PATCH "alpha"

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
//: Types
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Settings
//: ----------------------------------------------------------------------------
typedef struct settings_struct
{
        bool m_verbose;
        bool m_color;
        bool m_show_stats;
        ns_hlx::hlx *m_hlx;
        ns_hlx::t_stat_t *m_last_stat;
        uint64_t m_start_time_ms;
        uint64_t m_last_display_time_ms;
        int32_t m_run_time_ms;

        // ---------------------------------
        // Defaults...
        // ---------------------------------
        settings_struct() :
                m_verbose(false),
                m_color(false),
                m_show_stats(false),
                m_hlx(NULL),
                m_last_stat(NULL),
                m_start_time_ms(),
                m_last_display_time_ms(),
                m_run_time_ms(-1)
        {
                m_last_stat = new ns_hlx::t_stat_struct();
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
uint64_t hss_get_time_ms(void);
uint64_t hss_get_delta_time_ms(uint64_t a_start_time_ms);
void display_results_line_desc(settings_struct &a_settings);
void display_results_line(settings_struct &a_settings);

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
                g_settings->m_hlx->stop();
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
                                l_hlx->stop();
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
                usleep(1000000);

                if(a_settings.m_show_stats && !a_settings.m_verbose)
                {
                        if(l_first_time)
                        {
                                display_results_line_desc(a_settings);
                                l_first_time = false;
                        }
                        display_results_line(a_settings);
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
//: \details: Print the version.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_version(FILE* a_stream, int a_exit_code)
{

        // print out the version information
        fprintf(a_stream, "hlx Simple File Server (hss).\n");
        fprintf(a_stream, "Copyright (C) 2015 Verizon Digital Media.\n");
        fprintf(a_stream, "               Version: %d.%d.%d.%s\n",
                        HSS_VERSION_MAJOR,
                        HSS_VERSION_MINOR,
                        HSS_VERSION_MACRO,
                        HSS_VERSION_PATCH);
        exit(a_exit_code);

}


//: ----------------------------------------------------------------------------
//: \details: Print the command line help.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_usage(FILE* a_stream, int a_exit_code)
{
        fprintf(a_stream, "Usage: hss [options]\n");
        fprintf(a_stream, "Options are:\n");
        fprintf(a_stream, "  -h, --help           Display this help and exit.\n");
        fprintf(a_stream, "  -V, --version        Display the version number and exit.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Server Options:\n");
        fprintf(a_stream, "  -p, --port           Server port -defaults to 12345.\n");
        fprintf(a_stream, "  -t, --threads        Number of server threads.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "TLS Options:\n");
        fprintf(a_stream, "  -T, --TLS            Use TLS.\n");
        fprintf(a_stream, "  -K, --tls_key        TLS Private key file (key).\n");
        fprintf(a_stream, "  -P, --tls_crt        TLS Public certificate file (crt).\n");
        fprintf(a_stream, "  -y, --tls_cipher     TLS Cipher --see \"openssl ciphers\" for list.\n");
        fprintf(a_stream, "  -O, --tls_options    TLS Options string.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Print Options:\n");
        fprintf(a_stream, "  -v, --verbose        Verbose logging\n");
        fprintf(a_stream, "  -c, --color          Color\n");
        fprintf(a_stream, "  -s, --status         Status -show server statistics\n");
        fprintf(a_stream, "  \n");

#ifdef ENABLE_PROFILER
        fprintf(a_stream, "Debug Options:\n");
        fprintf(a_stream, "  -G, --gprofile       Google cpu profiler output file\n");
        fprintf(a_stream, "  -H, --hprofile       Google heap profiler output file\n");
        fprintf(a_stream, "\n");
#endif

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
        ns_hlx::hlx *l_hlx = new ns_hlx::hlx();
        l_settings.m_hlx = l_hlx;
        // For sighandler
        g_settings = &l_settings;

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
                { "port",           1, 0, 'p' },
                { "threads",        1, 0, 't' },
                { "TLS",            0, 0, 'T' },
                { "tls_key",        1, 0, 'K' },
                { "tls_crt",        1, 0, 'P' },
                { "tls_cipher",     1, 0, 'y' },
                { "tls_options",    1, 0, 'O' },
                { "verbose",        0, 0, 'v' },
                { "color",          0, 0, 'c' },
                { "status",         0, 0, 's' },
#ifdef ENABLE_PROFILER
                { "gprofile",       1, 0, 'G' },
                { "hprofile",       1, 0, 'H' },
#endif
                // list sentinel
                { 0, 0, 0, 0 }
        };

        std::string l_gprof_file;
        std::string l_hprof_file;
        bool l_show_status = false;
        uint16_t l_server_port = 12345;
        ns_hlx::scheme_t l_scheme = ns_hlx::SCHEME_TCP;
        std::string l_tls_key;
        std::string l_tls_crt;

        // -------------------------------------------------
        // Args...
        // -------------------------------------------------
#ifdef ENABLE_PROFILER
        char l_short_arg_list[] = "hVp:t:TK:P:y:O:vcsG:H:";
#else
        char l_short_arg_list[] = "hVp:t:TK:P:y:O:vcs";
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
                // Server port
                // ---------------------------------------
                case 'p':
                {
                        l_server_port = (uint16_t)strtoul(l_argument.c_str(), NULL, 10);
                        break;
                }
                // ---------------------------------------
                // num threads
                // ---------------------------------------
                case 't':
                {
                        int l_num_threads = 1;
                        //NDBG_PRINT("arg: --threads: %s\n", l_argument.c_str());
                        l_num_threads = atoi(optarg);
                        if (l_num_threads < 0)
                        {
                                printf("num-threads must be at least 0\n");
                                print_usage(stdout, -1);
                        }
                        l_hlx->set_num_threads(l_num_threads);
                        break;
                }
                // ---------------------------------------
                // Use tls
                // ---------------------------------------
                case 'T':
                {
                        l_scheme = ns_hlx::SCHEME_TLS;
                }
                // ---------------------------------------
                // TLS private key
                // ---------------------------------------
                case 'K':
                {
                        l_tls_key = l_argument;
                        break;
                }
                // ---------------------------------------
                // TLS public cert
                // ---------------------------------------
                case 'P':
                {
                        l_tls_crt = l_argument;
                        break;
                }
                // ---------------------------------------
                // cipher
                // ---------------------------------------
                case 'y':
                {
                        l_hlx->set_tls_server_ctx_cipher_list(l_argument);
                        break;
                }
                // ---------------------------------------
                // ssl options
                // ---------------------------------------
                case 'O':
                {
                        int32_t l_status;
                        l_status = l_hlx->set_tls_server_ctx_options(l_argument);
                        if(l_status != STATUS_OK)
                        {
                                return STATUS_ERROR;
                        }

                        break;
                }
                // ---------------------------------------
                // verbose
                // ---------------------------------------
                case 'v':
                {
                        l_settings.m_verbose = true;
                        l_hlx->set_verbose(true);
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
                // status
                // ---------------------------------------
                case 's':
                {
                        l_settings.m_show_stats = true;
                        l_show_status = true;
                        l_hlx->set_stats(true);
                        break;
                }
#ifdef ENABLE_PROFILER
                // ---------------------------------------
                // Google CPU Profiler Output File
                // ---------------------------------------
                case 'G':
                {
                        l_gprof_file = l_argument;
                        break;
                }
#endif
#ifdef ENABLE_PROFILER
                // ---------------------------------------
                // Google Heap Profiler Output File
                // ---------------------------------------
                case 'H':
                {
                        l_hprof_file = l_argument;
                        break;
                }
#endif
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

        // -------------------------------------------
        // Check for inputs
        // -------------------------------------------
        // TLS Check
        if(l_scheme == ns_hlx::SCHEME_TLS)
        {
                if(l_tls_key.empty() || l_tls_crt.empty())
                {
                        printf("Error: TLS selected but not private key or public crt provided\n");
                        return -1;
                }
                l_hlx->set_tls_server_ctx_key(l_tls_key);
                l_hlx->set_tls_server_ctx_crt(l_tls_crt);
        }
        ns_hlx::lsnr *l_lsnr = new ns_hlx::lsnr(l_server_port, l_scheme);

        // -------------------------------------------
        // Add endpoints
        // -------------------------------------------
        int32_t l_status = 0;
        ns_hlx::file_h *l_file_getter = new ns_hlx::file_h();
        l_status = l_lsnr->register_endpoint("/*", l_file_getter);
        if(l_status != 0)
        {
                printf("Error: adding endpoint: %s\n", "/*");
                return -1;
        }

        // -------------------------------------------
        // Register lsnr
        // -------------------------------------------
        l_hlx->register_lsnr(l_lsnr);

        // -------------------------------------------
        // Sigint handler
        // -------------------------------------------
        if (signal(SIGINT, sig_handler) == SIG_ERR)
        {
                printf("Error: can't catch SIGINT\n");
                return -1;
        }
        // -------------------------------------------
        // Profiling
        // -------------------------------------------
#ifdef ENABLE_PROFILER
        if (!l_gprof_file.empty())
        {
                ProfilerStart(l_gprof_file.c_str());
        }
#endif
#ifdef ENABLE_PROFILER
        if (!l_hprof_file.empty())
        {
                HeapProfilerStart(l_hprof_file.c_str());
        }
#endif

        uint64_t l_start_time_ms = hss_get_time_ms();
        l_settings.m_start_time_ms = l_start_time_ms;

        // -------------------------------------------
        // Run...
        // -------------------------------------------
        int32_t l_run_status = 0;
        l_run_status = l_hlx->run();
        if(l_run_status != 0)
        {
                printf("Error: performing hlx::run");
                return -1;
        }
        //uint64_t l_start_time_ms = get_time_ms();
        command_exec(l_settings);
        if(l_settings.m_verbose)
        {
                printf("Finished -joining all threads\n");
        }
        l_hlx->wait_till_stopped();

        // -------------------------------------------
        // Profiling
        // -------------------------------------------
#ifdef ENABLE_PROFILER
        if (!l_gprof_file.empty())
        {
                ProfilerStop();
        }
#endif
#ifdef ENABLE_PROFILER
        if (!l_hprof_file.empty())
        {
                HeapProfilerStop();
        }
#endif
        //uint64_t l_end_time_ms = get_time_ms() - l_start_time_ms;
        // Status???
        if(l_show_status)
        {
                // TODO
        }

        // Cleanup...
        if(l_hlx)
        {
                delete l_hlx;
                l_hlx = NULL;
        }
        if(l_file_getter)
        {
                delete l_file_getter;
                l_file_getter = NULL;
        }
        return 0;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t hss_get_time_ms(void)
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
uint64_t hss_get_delta_time_ms(uint64_t a_start_time_ms)
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
void display_results_line_desc(settings_struct &a_settings)
{

        // TODO m_num_reqs

        printf("+-----------/-----------/-----------/-----------+-----------+-----------+--------------+--------------+-------------+-------------+\n");
        if(a_settings.m_color)
        {
        printf("| %s%9s%s / %s%9s%s / %s%9s%s / %s%9s%s | %s%9s%s | %s%9s%s | %s%12s%s | %s%12s%s | %11s | %11s |\n",
                        ANSI_COLOR_FG_WHITE, "CCurnt", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_GREEN, "CStart", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_BLUE,  "CComp", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_BLACK, "TReqs", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_MAGENTA, "IdlKil", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_RED, "Errors", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_YELLOW, "kBytes  Rd/s", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_CYAN,   "kBytes  Wr/s", ANSI_COLOR_OFF,
                        "Req/s",
                        "Elapsed");
        }
        else
        {
                printf("| %9s / %9s / %9s / %9s | %9s | %9s | %12s | %12s | %11s | %11s |\n",
                                "CCurnt",
                                "CStart",
                                "CComp",
                                "TReqs",
                                "IdlKil",
                                "Errors",
                                "kBytes  Rd/s",
                                "kBytes  Wr/s",
                                "Req/s",
                                "Elapsed");
        }
        printf("+-----------/-----------/-----------/-----------+-----------+-----------+--------------+--------------+-------------+-------------+\n");
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void display_results_line(settings_struct &a_settings)
{
        ns_hlx::t_stat_t l_total;
        uint64_t l_cur_time_ms = hss_get_time_ms();

        // Get stats
        a_settings.m_hlx->get_stats(l_total);

        double l_reqs_per_s = ((double)(l_total.m_num_ups_reqs - a_settings.m_last_stat->m_num_ups_reqs)*1000.0) /
                        ((double)(l_cur_time_ms - a_settings.m_last_display_time_ms));
        double l_kb_read_per_s = ((double)(l_total.m_num_bytes_read - a_settings.m_last_stat->m_num_bytes_read)*1000.0/1024) /
                        ((double)(l_cur_time_ms - a_settings.m_last_display_time_ms));
        double l_kb_written_per_s = ((double)(l_total.m_num_bytes_written - a_settings.m_last_stat->m_num_bytes_written)*1000.0/1024) /
                        ((double)(l_cur_time_ms - a_settings.m_last_display_time_ms));
        a_settings.m_last_display_time_ms = hss_get_time_ms();
        *a_settings.m_last_stat = l_total;

        if(a_settings.m_color)
        {
                        printf("| %s%9" PRIu32 "%s / %s%9" PRIu64 "%s / %s%9" PRIi64 "%s / %s%9" PRIi64 "%s | %s%9" PRIu64 "%s | %s%9" PRIu64 "%s | %s%12.2f%s | %s%12.2f%s |  %10.2f | %10.2fs |\n",
                                        ANSI_COLOR_FG_WHITE, l_total.m_cur_ups_conn_count, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_GREEN, l_total.m_num_ups_conn_started, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_BLUE, l_total.m_num_ups_conn_completed, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_BLACK, l_total.m_num_ups_reqs, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_MAGENTA, l_total.m_num_ups_idle_killed, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_RED, l_total.m_num_errors, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_YELLOW, l_kb_read_per_s, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_CYAN, l_kb_written_per_s, ANSI_COLOR_OFF,
                                        l_reqs_per_s,
                                        ((double)(hss_get_delta_time_ms(a_settings.m_start_time_ms))) / 1000.0);
        }
        else
        {
                printf("| %9" PRIu32 " / %9" PRIu64 " / %9" PRIi64 " / %9" PRIi64 " | %9" PRIu64 " | %9" PRIu64 " | %12.2f | %12.2f |  %10.2f | %10.2fs |\n",
                                l_total.m_cur_ups_conn_count,
                                l_total.m_num_ups_conn_started,
                                l_total.m_num_ups_conn_completed,
                                l_total.m_num_ups_reqs,
                                l_total.m_num_ups_idle_killed,
                                l_total.m_num_errors,
                                l_kb_read_per_s,
                                l_kb_written_per_s,
                                l_reqs_per_s,
                                ((double)(hss_get_delta_time_ms(a_settings.m_start_time_ms)) / 1000.0));

        }
}
