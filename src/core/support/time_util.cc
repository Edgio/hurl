//! ----------------------------------------------------------------------------
//! Copyright Edgecast Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include "support/time_util.h"
#include <unistd.h>
#include <time.h>
// Mach time support clock_get_time
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif
//! ----------------------------------------------------------------------------
//! Constants
//! ----------------------------------------------------------------------------
#define MAX_TIMER_RESOLUTION_US 10
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! global static
//! ----------------------------------------------------------------------------
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
//! ----------------------------------------------------------------------------
//! \details: Get the rdtsc value
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
// https://github.com/mellanox/sockperf/issues/47#issuecomment-97041796
static __inline__ uint64_t get_rdtsc64()
{
        uint64_t tm;
#if defined(__arm__) && defined(__linux__)
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        tm = ts.tv_sec;
        tm = tm * 1000000000ULL;
        tm += ts.tv_nsec;
#else
        uint32_t l_lo;
        uint32_t l_hi;
        // We cannot use "=A", since this would use %rax on x86_64
        __asm__ __volatile__ ("rdtsc" : "=a" (l_lo), "=d" (l_hi));
        // output registers
        tm = (uint64_t) l_hi << 32 | l_lo;
#endif
        return tm;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
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
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
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
//! ----------------------------------------------------------------------------
//! \details: Portable gettime function
//! \return:  NA
//! \param:   ao_timespec: struct timespec -with gettime result
//! ----------------------------------------------------------------------------
static void _rt_gettime(struct timespec &ao_timespec)
{
#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
        clock_serv_t l_cclock;
        mach_timespec_t l_mts;
        host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &l_cclock);
        clock_get_time(l_cclock, &l_mts);
        mach_port_deallocate(mach_task_self(), l_cclock);
        ao_timespec.tv_sec = l_mts.tv_sec;
        ao_timespec.tv_nsec = l_mts.tv_nsec;
// TODO -if __linux__
#else
        clock_gettime(CLOCK_REALTIME, &ao_timespec);
#endif
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
uint64_t get_time_s(void)
{
        if(_use_cached_time(g_last_s_rdtsc) && g_last_s)
        {
                return g_last_s;
        }
        //NDBG_PRINT("HERE g_count: %lu\n", ++g_count);
	struct timespec l_timespec;
	_rt_gettime(l_timespec);
	g_last_s = (((uint64_t)l_timespec.tv_sec));
	return g_last_s;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
uint64_t get_time_ms(void)
{
        if(_use_cached_time(g_last_ms_rdtsc) && g_last_ms)
        {
                return g_last_ms;
        }
        //NDBG_PRINT("HERE g_count: %lu\n", ++g_count);
	struct timespec l_timespec;
        _rt_gettime(l_timespec);
	g_last_ms = (((uint64_t)l_timespec.tv_sec) * 1000) + (((uint64_t)l_timespec.tv_nsec) / 1000000);
	return g_last_ms;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
uint64_t get_time_us(void)
{
        if(_use_cached_time(g_last_us_rdtsc) && g_last_us)
        {
                return g_last_us;
        }
        //NDBG_PRINT("HERE g_count: %lu\n", ++g_count);
	struct timespec l_timespec;
        _rt_gettime(l_timespec);
        g_last_us = (((uint64_t)l_timespec.tv_sec) * 1000000) + (((uint64_t)l_timespec.tv_nsec) / 1000);
	return g_last_us;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
uint64_t get_delta_time_ms(uint64_t a_start_time_ms)
{
	return get_time_ms() - a_start_time_ms;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
uint64_t get_delta_time_us(uint64_t a_start_time_us)
{
	return get_time_us() - a_start_time_us;
}
} //namespace ns_hurl {
