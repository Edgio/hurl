//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    util.cc
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
#include "util.h"
#include "ndebug.h"

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <unistd.h>
#include <time.h>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define MAX_TIMER_RESOLUTION_US 10

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t break_header_string(const std::string &a_header_str,
		std::string &ao_header_key,
		std::string &ao_header_val)
{

	// Find port prefix ":"
	size_t l_colon_pos = 0;
	l_colon_pos = a_header_str.find(":", 0);
	if(l_colon_pos == std::string::npos)
	{
		return -1;
	}

	ao_header_key = a_header_str.substr(0, l_colon_pos);
	ao_header_val = a_header_str.substr(l_colon_pos+1, a_header_str.length());

	return 0;
}

//: ----------------------------------------------------------------------------
//: global static
//: ----------------------------------------------------------------------------
__thread uint64_t g_last_s_rdtsc  = 0;
__thread uint64_t g_last_s        = 0;
__thread uint64_t g_last_ms_rdtsc = 0;
__thread uint64_t g_last_ms       = 0;
__thread uint64_t g_last_us_rdtsc = 0;
__thread uint64_t g_last_us       = 0;

__thread uint64_t g_cyles_us      = 0;

// Date cache...
__thread uint64_t g_last_date_str_s_rdtsc  = 0;
__thread uint64_t g_last_date_str_s        = 0;
__thread char g_last_date_str[128];

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static inline bool _use_cached_time(uint64_t l_last_rdtsc)
{
        if(!g_cyles_us)
        {
                uint64_t l_start = rdtsc();
                usleep(100000);
                g_cyles_us = (rdtsc()-l_start)/100000;
        }
        if((rdtsc() - l_last_rdtsc) < MAX_TIMER_RESOLUTION_US*g_cyles_us)
        {
                return true;
        }
        l_last_rdtsc = rdtsc();
        return false;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const char *get_date_str(void)
{
        if(_use_cached_time(g_last_date_str_s_rdtsc) && g_last_s)
        {
                return g_last_date_str;
        }
        time_t l_now = time(0);
        struct tm l_tm = *gmtime(&l_now);
        strftime(g_last_date_str, sizeof g_last_date_str, "%a, %d %b %Y %H:%M:%S %Z", &l_tm);
        return g_last_date_str;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t get_time_s(void)
{
        if(_use_cached_time(g_last_s_rdtsc) && g_last_s)
        {
                return g_last_s;
        }
        //NDBG_PRINT("HERE g_count: %lu\n", ++g_count);
	struct timespec l_timespec;
	clock_gettime(CLOCK_REALTIME, &l_timespec);
	g_last_s = (((uint64_t)l_timespec.tv_sec));
	return g_last_s;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t get_time_ms(void)
{
        if(_use_cached_time(g_last_ms_rdtsc) && g_last_ms)
        {
                return g_last_ms;
        }
        //NDBG_PRINT("HERE g_count: %lu\n", ++g_count);
	struct timespec l_timespec;
	clock_gettime(CLOCK_REALTIME, &l_timespec);
	g_last_ms = (((uint64_t)l_timespec.tv_sec) * 1000) + (((uint64_t)l_timespec.tv_nsec) / 1000000);
	return g_last_ms;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t get_time_us(void)
{
        if(_use_cached_time(g_last_us_rdtsc) && g_last_us)
        {
                return g_last_us;
        }
        //NDBG_PRINT("HERE g_count: %lu\n", ++g_count);
	struct timespec l_timespec;
	clock_gettime(CLOCK_REALTIME, &l_timespec);
        g_last_us = (((uint64_t)l_timespec.tv_sec) * 1000000) + (((uint64_t)l_timespec.tv_nsec) / 1000);
	return g_last_us;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t get_delta_time_ms(uint64_t a_start_time_ms)
{
	return get_time_ms() - a_start_time_ms;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t get_delta_time_us(uint64_t a_start_time_us)
{
	return get_time_us() - a_start_time_us;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string get_file_wo_path(std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind("/");

        if(pos == std::string::npos)  //No extension.
                return fName;
        if(pos == 0)    //. is at the front. Not an extension.
                return fName;

        return fName.substr(pos + 1, fName.length());
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string get_file_path(std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind("/");

        if(pos == std::string::npos)  //No extension.
                return fName;
        if(pos == 0)    //. is at the front. Not an extension.
                return fName;

        return fName.substr(0, pos);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string get_base_filename(std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind(".");

        if(pos == std::string::npos)  //No extension.
                return fName;
        if(pos == 0)    //. is at the front. Not an extension.
                return fName;

        return fName.substr(0, pos);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string get_file_ext(std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind(".");

        if(pos == std::string::npos)  //No extension.
                return NULL;
        if(pos == 0)    //. is at the front. Not an extension.
                return NULL;

        return fName.substr(pos + 1, fName.length());
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string get_file_wo_ext(std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind(".");

        if(pos == std::string::npos)  //No extension.
                return NULL;
        if(pos == 0)    //. is at the front. Not an extension.
                return NULL;

        return fName.substr(0, pos);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void add_stat_to_agg(t_stat_t &ao_stat_agg, const req_stat_t &a_req_stat)
{
        update_stat(ao_stat_agg.m_stat_us_connect, a_req_stat.m_tt_connect_us);
        update_stat(ao_stat_agg.m_stat_us_first_response, a_req_stat.m_tt_first_read_us);
        update_stat(ao_stat_agg.m_stat_us_end_to_end, a_req_stat.m_tt_completion_us);

        // Totals
        ++ao_stat_agg.m_total_reqs;
        ao_stat_agg.m_total_bytes += a_req_stat.m_total_bytes;

        // Status code
        //NDBG_PRINT("%sSTATUS_CODE%s: %d\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, a_req_stat.m_status_code);
        ++ao_stat_agg.m_status_code_count_map[a_req_stat.m_status_code];

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void add_to_total_stat_agg(t_stat_t &ao_stat_agg, const t_stat_t &a_add_total_stat)
{

        // Stats
        add_stat(ao_stat_agg.m_stat_us_connect , a_add_total_stat.m_stat_us_connect);
        add_stat(ao_stat_agg.m_stat_us_first_response , a_add_total_stat.m_stat_us_first_response);
        add_stat(ao_stat_agg.m_stat_us_end_to_end , a_add_total_stat.m_stat_us_end_to_end);

        ao_stat_agg.m_total_bytes += a_add_total_stat.m_total_bytes;
        ao_stat_agg.m_total_reqs += a_add_total_stat.m_total_reqs;

        ao_stat_agg.m_num_resolved += a_add_total_stat.m_num_resolved;
        ao_stat_agg.m_num_conn_started += a_add_total_stat.m_num_conn_started;
        ao_stat_agg.m_num_conn_completed += a_add_total_stat.m_num_conn_completed;
        ao_stat_agg.m_num_idle_killed += a_add_total_stat.m_num_idle_killed;
        ao_stat_agg.m_num_errors += a_add_total_stat.m_num_errors;
        ao_stat_agg.m_num_bytes_read += a_add_total_stat.m_num_bytes_read;

        for(status_code_count_map_t::const_iterator i_code = a_add_total_stat.m_status_code_count_map.begin();
                        i_code != a_add_total_stat.m_status_code_count_map.end();
                        ++i_code)
        {
                status_code_count_map_t::iterator i_code2;
                if((i_code2 = ao_stat_agg.m_status_code_count_map.find(i_code->first)) == ao_stat_agg.m_status_code_count_map.end())
                {
                        ao_stat_agg.m_status_code_count_map[i_code->first] = i_code->second;
                }
                else
                {
                        i_code2->second += i_code->second;
                }
        }

}

} //namespace ns_hlx {

