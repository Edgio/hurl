//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    stats_collector.cc
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
#include "stats_collector.h"
#include "util.h"
#include "t_client.h"

#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>


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

        SHOW_XSTAT_LINE("ms/connect:", a_stat.m_stat_us_connect);
        SHOW_XSTAT_LINE("ms/1st-response:", a_stat.m_stat_us_first_response);
        SHOW_XSTAT_LINE("ms/download:", a_stat.m_stat_us_download);
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
void stats_collector::display_results(double a_elapsed_time,
                uint32_t a_max_parallel,
                bool a_show_breakdown_flag,
                bool a_color)
{

        tag_stat_map_t l_tag_stat_map;
        total_stat_agg_t l_total;

        // Get stats
        get_stats(l_total, a_show_breakdown_flag, l_tag_stat_map);

        std::string l_tag;
        // TODO Fix elapse and max parallel
        l_tag = "ALL";
        show_total_agg_stat(l_tag, l_total, a_elapsed_time, a_max_parallel, a_color);

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
                        show_total_agg_stat(l_tag, i_stat->second, a_elapsed_time, a_max_parallel, a_color);
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
        SHOW_XSTAT_LINE_LEGACY("msecs/download:", a_stat.m_stat_us_download);
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
void stats_collector::get_stats(total_stat_agg_t &ao_all_stats,
                bool a_get_breakdown,
                tag_stat_map_t &ao_breakdown_stats)
{
        // -------------------------------------------
        // Aggregate
        // -------------------------------------------
        hlp *l_hlp = hlp::get();

#if 1
        for(t_client_list_t::iterator i_c = l_hlp->m_t_client_list.begin();
                        i_c != l_hlp->m_t_client_list.end();
                        ++i_c)
        {

                // Grab a copy of the stats
                tag_stat_map_t l_copy;
                (*i_c)->get_stats_copy(l_copy);
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
#else
        // Grab a copy of the stats
        tag_stat_map_t l_copy;
        l_hlp->get_playback_client()->get_stats_copy(l_copy);
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
#endif


}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void stats_collector::display_results_http_load_style(double a_elapsed_time,
                uint32_t a_max_parallel,
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
        show_total_agg_stat_legacy(l_tag, l_total, l_sep, a_elapsed_time, a_max_parallel);

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
                        show_total_agg_stat_legacy(l_tag, i_stat->second, l_sep, a_elapsed_time, a_max_parallel);
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t stats_collector::get_stats_json(char *l_json_buf, uint32_t l_json_buf_max_len)
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
                                "count", (uint64_t)(i_agg_stat->second.m_num_conn_completed),
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
void stats_collector::display_results_line_desc(bool a_color_flag)
{
        printf("+-----------+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+\n");
        if(a_color_flag)
        {
        printf("| %s%9s%s | %s%9s%s / %s%9s%s | %s%9s%s | %s%9s%s | %s%12s%s | %9s | %11s | %9s |\n",
                        ANSI_COLOR_FG_GREEN, "Cmpltd", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_CYAN, "Cmd_Execd", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_BLUE, "Cmd_Total", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_MAGENTA, "IdlKil", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_RED, "Errors", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_YELLOW, "kBytes Recvd", ANSI_COLOR_OFF,
                        "Elapsed",
                        "Conns",
                        "Reqs");
        }
        else
        {
                printf("| %9s | %9s / %9s | %9s | %9s | %12s | %9s | %11s | %9s |\n",
                                "Cmpltd",
                                "Cmd_Execd",
                                "Cmd_Total",
                                "IdlKil",
                                "Errors",
                                "kBytes Recvd",
                                "Elapsed",
                                "Conns",
                                "Reqs");
        }
        printf("+-----------+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+\n");
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void stats_collector::display_results_line(bool a_color_flag)
{

        total_stat_agg_t l_total;
        tag_stat_map_t l_unused;
        uint64_t l_cmd_total = hlp::get()->get_num_cmds();
        uint64_t l_cmd_execd = hlp::get()->get_num_cmds_serviced();
        uint32_t l_conns = hlp::get()->get_num_conns();
        uint64_t l_reqs = hlp::get()->get_num_reqs_sent();

        // Get stats
        get_stats(l_total, false, l_unused);

        if(a_color_flag)
        {
                        printf("| %s%9" PRIu64 "%s | %s%9" PRIu64 "%s / %s%9" PRIi64 "%s | %s%9" PRIu64 "%s | %s%9" PRIu64 "%s | %s%12.2f%s | %8.2fs |  %9u  | %9" PRIu64 " |\n",
                                        ANSI_COLOR_FG_GREEN, l_total.m_num_conn_completed, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_CYAN, l_cmd_execd, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_BLUE, l_cmd_total, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_MAGENTA, l_total.m_num_idle_killed, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_RED, l_total.m_num_errors, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_YELLOW, ((double)(l_total.m_num_bytes_read))/(1024.0), ANSI_COLOR_OFF,
                                        ((double)(get_delta_time_ms(m_start_time_ms))) / 1000.0,
                                        l_conns,
                                        l_reqs
                                        );
        }
        else
        {
                printf("| %9" PRIu64 " | %9" PRIu64 " / %9" PRIi64 " | %9" PRIu64 " | %9" PRIu64 " | %12.2f | %8.2fs | %9u | %9" PRIu64 " |\n",
                                l_total.m_num_conn_completed,
                                l_cmd_execd,
                                l_cmd_total,
                                l_total.m_num_idle_killed,
                                l_total.m_num_errors,
                                ((double)(l_total.m_num_bytes_read))/(1024.0),
                                ((double)(get_delta_time_ms(m_start_time_ms)) / 1000.0),
                                l_conns,
                                l_reqs
                                );

        }

        m_last_display_time_ms = get_time_ms();
        m_last_stat = l_total;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
stats_collector::stats_collector(void):
                m_start_time_ms(0),
                m_last_display_time_ms(0),
                m_last_stat()
{
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
stats_collector *stats_collector::get(void)
{
        if (m_singleton_ptr == NULL) {
                //If not yet created, create the singleton instance
                m_singleton_ptr = new stats_collector();
        }
        return m_singleton_ptr;
}

//: ----------------------------------------------------------------------------
//: Class variables
//: ----------------------------------------------------------------------------

// the pointer to the singleton for the instance 
stats_collector *stats_collector::m_singleton_ptr;


#if 0
getrusage (RUSAGE_SELF, &test_rusage_start);
test_time_start = timer_now ();
core_loop ();
test_time_stop = timer_now ();
getrusage (RUSAGE_SELF, &test_rusage_stop);
#endif


