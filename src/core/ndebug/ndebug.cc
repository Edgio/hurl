//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    ndebug.cc
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
#include "ndebug.h"
#include <string>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>

#include <execinfo.h> // support backtrace
#include <cxxabi.h>   // support demangled symbols

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------

namespace ns_hurl {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int get_stack_string(char* ao_stack_str)
{
        size_t stack_depth = 0;
        void* stack_addrs[NDBG_NUM_BACKTRACE_IN_TAG] = { (void*)0 }; //Max depth == 20
        char** stack_strings = NULL;
        int status = 0;

        stack_depth = backtrace(stack_addrs, NDBG_NUM_BACKTRACE_IN_TAG);
        stack_strings = backtrace_symbols(stack_addrs, stack_depth);

        ao_stack_str[0] = '\0';

        for (size_t i = 0; i < stack_depth; i++)
        {
                // for each frame in the backtrace, excluding the current one
                // parse symbol and demangle
                std::string current_entry(stack_strings[i]);
                char frame_str[64]="";

                int open_paren = current_entry.find('(');
                char* pretty_name = abi::__cxa_demangle(
                        current_entry.substr(open_paren + 1, current_entry.find("+") - open_paren - 1).c_str(), 0, 0, &status);

                //Skip 0 -<this frame>
                if ((i != 0) && (i != 1))
                {
                        if (i == 2)
                                snprintf(frame_str, sizeof(frame_str), "%sFrm[%zd]:%s", ANSI_COLOR_FG_RED, i - 2, ANSI_COLOR_FG_GREEN);
                        else
                                snprintf(frame_str, sizeof(frame_str), "%sFrm[%zd]:%s", ANSI_COLOR_FG_BLUE, i - 2, ANSI_COLOR_FG_GREEN);

                        strcat(ao_stack_str, frame_str);

                        if (pretty_name)
                        {
                                strncat(ao_stack_str, pretty_name, 256);
                                free(pretty_name);
                        }
                        else
                        {
                                strncat(ao_stack_str, current_entry.c_str(), 256);
                        }

                        strcat(ao_stack_str, "\033[0m\n");
                }

        }

        free(stack_strings); /* malloc'd by backtrace call */

        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_bt(const char* a_file, const char* a_func, const int a_line)
{
        //char tag[MAX_BACKTRACE_TAG_SIZE];
        char func_str[NDBG_MAX_BACKTRACE_TAG_SIZE] = "";

        get_stack_string(func_str);
        //snprintf(tag, MAX_BACKTRACE_TAG_SIZE - 1, "%s || %s::%s::%d", func_str, a_file, a_func, a_line);

        printf("%s=====>> B A C K T R A C E <<=====%s \n(%s%s::%s%s::%d)\n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF,
               ANSI_COLOR_FG_YELLOW, a_file, a_func, ANSI_COLOR_OFF, a_line);
        printf("%s\n", func_str);

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void mem_display(const uint8_t* a_mem_buf, uint32_t a_length)
{

        char l_display_line[256] = "";
        unsigned int l_bytes_displayed = 0;
        char l_byte_display[8] = "";
        char l_ascii_display[17]="";

        while (l_bytes_displayed < a_length)
        {

                unsigned int l_col = 0;

                //printf("%s.%d: Display length = %d\n", __FILE__,__LINE__,length);

                //Show offset
                snprintf(l_display_line, sizeof(l_display_line), "%s0x%08X %s", ANSI_COLOR_FG_BLUE, l_bytes_displayed, ANSI_COLOR_OFF);

                strcat(l_display_line, " ");
                strcat(l_display_line, ANSI_COLOR_FG_GREEN);

                while ((l_col < 16) && (l_bytes_displayed < a_length))
                {

                        snprintf(l_byte_display, sizeof(l_byte_display), "%02X", (unsigned char) a_mem_buf[l_bytes_displayed]);
                        strcat(l_display_line, l_byte_display);

                        if (isprint(a_mem_buf[l_bytes_displayed]))
                                l_ascii_display[l_col] = a_mem_buf[l_bytes_displayed];
                        else
                                l_ascii_display[l_col] = '.';

                        l_col++;
                        l_bytes_displayed++;

                        if (!(l_col % 4))
                                strcat(l_display_line, " ");
                }

                if ((l_col < 16) && (l_bytes_displayed >= a_length))
                {

                        while (l_col < 16)
                        {

                                strcat(l_display_line, "..");
                                l_ascii_display[l_col] = '.';
                                l_col++;

                                if (!(l_col % 4))
                                        strcat(l_display_line, " ");

                        }

                }

                l_ascii_display[l_col] = '\0';

                strcat(l_display_line, ANSI_COLOR_OFF);

                strcat(l_display_line, " ");
                strcat(l_display_line, l_ascii_display);

                printf("%s\n", l_display_line);

        }

}

}
