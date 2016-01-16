//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    time_util.cc
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
#include "time_util.h"
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
//: \details: Get the rdtsc value
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static __inline__ uint64_t get_rdtsc64()
{
        uint32_t lo, hi;

        // We cannot use "=A", since this would use %rax on x86_64
        __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
        // output registers

        return (uint64_t) hi << 32 | lo;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static inline bool _use_cached_time(uint64_t &a_last_rdtsc)
{
        if(!g_cyles_us)
        {
                uint64_t l_start = get_rdtsc64();
                usleep(100000);
                g_cyles_us = (get_rdtsc64()-l_start)/100000;
        }
        if((get_rdtsc64() - a_last_rdtsc) < MAX_TIMER_RESOLUTION_US*g_cyles_us)
        {
                return true;
        }
        a_last_rdtsc = get_rdtsc64();
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

} //namespace ns_hlx {

