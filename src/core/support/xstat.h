//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    xstat.h
//: \details: Simple stat objects
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
#ifndef _XSTAT_H
#define _XSTAT_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <stdio.h>
#include <math.h>
#include <stdint.h>
//#include "ndebug.h"

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef struct xstat_struct
{
        double m_sum_x;
        double m_sum_x2;
        double m_min;
        double m_max;
        uint64_t m_num;

        double min() const { return m_min; }
        double max() const { return m_max; }
        double mean() const { return (m_num > 0) ? m_sum_x / m_num : 0.0; }
        double var() const { return (m_num > 0) ? (m_sum_x2 - m_sum_x) / m_num : 0.0; }
        double stdev() const { return sqrt(var()); }

        xstat_struct():
                m_sum_x(0.0),
                m_sum_x2(0.0),
                m_min(1000000000.0),
                m_max(0.0),
                m_num(0)
        {}

} xstat_t;

//: ----------------------------------------------------------------------------
//: \details: Update stat with new value
//: \return:  n/a
//: \param:   ao_stat stat to be updated
//: \param:   a_val value to update stat with
//: ----------------------------------------------------------------------------
inline void update_stat(xstat_t &ao_stat, double a_val)
{
        ao_stat.m_num++;
        ao_stat.m_sum_x += a_val;
        ao_stat.m_sum_x2 += a_val*a_val;
        if(a_val > ao_stat.m_max) ao_stat.m_max = a_val;
        if(a_val < ao_stat.m_min) ao_stat.m_min = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: Add stats
//: \return:  n/a
//: \param:   ao_stat stat to be updated
//: \param:   a_from_stat stat to add
//: ----------------------------------------------------------------------------
inline void add_stat(xstat_t &ao_stat, const xstat_t &a_from_stat)
{
        ao_stat.m_num += a_from_stat.m_num;
        ao_stat.m_sum_x += a_from_stat.m_sum_x;
        ao_stat.m_sum_x2 += a_from_stat.m_sum_x2;
        if(a_from_stat.m_min < ao_stat.m_min)
                ao_stat.m_min = a_from_stat.m_min;
        if(a_from_stat.m_max > ao_stat.m_max)
                ao_stat.m_max = a_from_stat.m_max;
}

//: ----------------------------------------------------------------------------
//: \details: Clear stat
//: \return:  n/a
//: \param:   ao_stat stat to be cleared
//: ----------------------------------------------------------------------------
inline void clear_stat(xstat_t &ao_stat)
{
        ao_stat.m_sum_x = 0.0;
        ao_stat.m_sum_x2 = 0.0;
        ao_stat.m_min = 0.0;
        ao_stat.m_max = 0.0;
        ao_stat.m_num = 0;
}

//: ----------------------------------------------------------------------------
//: \details: Show stat
//: \return:  n/a
//: \param:   ao_stat stat display
//: ----------------------------------------------------------------------------
inline void show_stat(const xstat_t &ao_stat)
{
        printf("Stat: Mean: %4.2f, StdDev: %4.2f, Min: %4.2f, Max: %4.2f Num: %lu\n",
               ao_stat.mean(),
               ao_stat.stdev(),
               ao_stat.m_min,
               ao_stat.m_max,
               ao_stat.m_num);
}

//: ----------------------------------------------------------------------------
//: Example...
//: ----------------------------------------------------------------------------
#if 0
int main(void)
{
    xstat_t l_s;
    update_stat(l_s, 30.0);
    update_stat(l_s, 15.0);
    update_stat(l_s, 22.0);
    update_stat(l_s, 10.0);
    update_stat(l_s, 24.0);
    show_stat(l_s);
    return 0;
}
#endif

#endif
