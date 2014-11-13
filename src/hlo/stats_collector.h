//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    stats_collector.h
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
#ifndef _STATS_COLLECTOR_H
#define _STATS_COLLECTOR_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "ndebug.h"
#include "reqlet.h"

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define STATS_COLLECTOR_DEFAULT_AGG_STATS_SIZE 1024

//: ----------------------------------------------------------------------------
//: Fwd' Decls
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: \details: stats_collector
//: ----------------------------------------------------------------------------
class stats_collector
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        //int32_t add_stat(const std::string &a_tag, const req_stat_t &a_req_stat);
        void display_results(double a_elapsed_time,
                        uint32_t a_max_parallel,
                        bool a_show_breakdown_flag = false,
                        bool a_color = false);
        void display_results_http_load_style(double a_elapsed_time,
                        uint32_t a_max_parallel,
                        bool a_show_breakdown_flag = false,
                        bool a_one_line_flag = false);

        void display_results_line_desc(bool a_color_flag);
        void display_results_line(bool a_color_flag);

        void get_stats(total_stat_agg_t &ao_all_stats, bool a_get_breakdown, tag_stat_map_t &ao_breakdown_stats);
        int32_t get_stats_json(char *l_json_buf, uint32_t l_json_buf_max_len);
        void set_start_time_ms(uint64_t a_start_time_ms) {m_start_time_ms = a_start_time_ms;}


        // -------------------------------------------------
        // Public members
        // -------------------------------------------------

        // -------------------------------------------------
        // Class methods
        // -------------------------------------------------
    // Get the singleton instance
    static stats_collector *get(void);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(stats_collector)
        stats_collector();

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        uint64_t m_start_time_ms;
        uint64_t m_last_display_time_ms;
        total_stat_agg_t m_last_stat;

        // -------------------------------------------------
        // Class members
        // -------------------------------------------------
        // the pointer to the singleton for the instance 
        static stats_collector *m_singleton_ptr;

};

#endif








