//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    trace.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    04/15/2016
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
#ifndef _TRACE_H
#define _TRACE_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <stdint.h>
#include <stdio.h>
#include <string>
#include "hurl/support/time_util.h"

//: ----------------------------------------------------------------------------
//: trace macros
//: ----------------------------------------------------------------------------
// TODO -open file if NULL???
#ifndef TRC_PRINT
#define TRC_PRINT(_level, ...) \
        do { \
        if(ns_hurl::g_trc_log_file)\
        {\
        if(ns_hurl::g_trc_log_level >= _level) { \
        fprintf(ns_hurl::g_trc_log_file, "%.3f %s %s:%s.%d: ", \
                        ((double)ns_hurl::get_time_ms())/1000.0, \
                        ns_hurl::trc_log_level_str(_level), \
                        __FILE__, __FUNCTION__, __LINE__); \
        fprintf(ns_hurl::g_trc_log_file, __VA_ARGS__); \
        fflush(ns_hurl::g_trc_log_file); \
        } \
        } \
        } while(0)
#endif

#ifndef TRC_MEM
#define TRC_MEM(_level, _buf, _len) \
        do { \
        if(ns_hurl::g_trc_log_file)\
        {\
        if(ns_hurl::g_trc_log_level >= _level) { \
        ns_hurl::trc_mem_display(ns_hurl::g_trc_log_file, _buf, _len);\
        fflush(ns_hurl::g_trc_log_file); \
        } \
        } \
        } while(0)
#endif

//: ----------------------------------------------------------------------------
//: trace levels
//: ----------------------------------------------------------------------------
#ifndef TRC_ERROR
#define TRC_ERROR(...)  TRC_PRINT(ns_hurl::TRC_LOG_LEVEL_ERROR, __VA_ARGS__)
#endif
#ifndef TRC_WARN
#define TRC_WARN(...)  TRC_PRINT(ns_hurl::TRC_LOG_LEVEL_WARN, __VA_ARGS__)
#endif
#ifndef TRC_DEBUG
#define TRC_DEBUG(...)  TRC_PRINT(ns_hurl::TRC_LOG_LEVEL_DEBUG, __VA_ARGS__)
#endif
#ifndef TRC_VERBOSE
#define TRC_VERBOSE(...)  TRC_PRINT(ns_hurl::TRC_LOG_LEVEL_VERBOSE, __VA_ARGS__)
#endif
#ifndef TRC_ALL
#define TRC_ALL(...)  TRC_PRINT(ns_hurl::TRC_LOG_LEVEL_ALL, __VA_ARGS__)
#endif
#ifndef TRC_ALL_MEM
#define TRC_ALL_MEM(_buf,_len) TRC_MEM(ns_hurl::TRC_LOG_LEVEL_ALL, _buf, _len)
#endif

// TODO -open file if NULL???
#ifndef TRC_OUTPUT
#define TRC_OUTPUT(...) \
        do { \
        if(ns_hurl::g_trc_out_file)\
        {\
        fprintf(ns_hurl::g_trc_out_file, __VA_ARGS__); \
        fflush(ns_hurl::g_trc_out_file); \
        }\
        } while(0)
#endif

namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: trc enum
//: ----------------------------------------------------------------------------
#ifndef _HURL_TRC_LOG_LEVEL_T
#define TRC_LOG_LEVEL_MAP(XX)\
        XX(0,  NONE,        N)\
        XX(1,  ERROR,       E)\
        XX(2,  WARN,        W)\
        XX(3,  DEBUG,       D)\
        XX(4,  VERBOSE,     V)\
        XX(5,  ALL,         A)

typedef enum trc_level_enum
{
#define XX(num, name, string) TRC_LOG_LEVEL_##name = num,
        TRC_LOG_LEVEL_MAP(XX)
#undef XX
} trc_log_level_t;
#endif
const char *trc_log_level_str(trc_log_level_t a_level);

//: ----------------------------------------------------------------------------
//: Open logs
//: ----------------------------------------------------------------------------
void trc_log_level_set(trc_log_level_t a_level);
int32_t trc_log_file_open(const std::string &a_file);
int32_t trc_log_file_close(void);

//: ----------------------------------------------------------------------------
//: Externs
//: ----------------------------------------------------------------------------
extern trc_log_level_t g_trc_log_level;
extern FILE* g_trc_log_file;
extern FILE* g_trc_out_file;

//: ----------------------------------------------------------------------------
//: Utilities
//: ----------------------------------------------------------------------------
void trc_mem_display(FILE *a_file, const uint8_t *a_mem_buf, uint32_t a_length);

} // namespace ns_hurl {

#endif // NDEBUG_H_

