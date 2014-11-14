//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    reqlet_repo.cc
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
#include "reqlet_repo.h"
#include "reqlet.h"
#include "ndebug.h"

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void reqlet_repo::dump_all_responses(bool a_color, bool a_pretty, output_type_t a_output_type, int a_part_map)
{
        std::string l_host_color = "";
        std::string l_status_color = "";
        std::string l_header_color = "";
        std::string l_body_color = "";
        std::string l_off_color = "";
        if(a_color)
        {
                l_host_color = ANSI_COLOR_FG_BLUE;
                l_status_color = ANSI_COLOR_FG_MAGENTA;
                l_header_color = ANSI_COLOR_FG_GREEN;
                l_body_color = ANSI_COLOR_FG_YELLOW;
                l_off_color = ANSI_COLOR_OFF;
        }

        if(a_output_type == OUTPUT_JSON)
        {
                NDBG_OUTPUT("[");
                if(a_pretty) NDBG_OUTPUT("\n");
        }

        int l_cur_reqlet = 0;
        int l_reqlet_num = m_reqlet_list.size();
        for(reqlet_list_t::iterator i_reqlet = m_reqlet_list.begin();
            i_reqlet != m_reqlet_list.end();
            ++i_reqlet, ++l_cur_reqlet)
        {


                bool l_fbf = false;
                if(a_output_type == OUTPUT_JSON)
                {
                        if(a_pretty) if(a_output_type == OUTPUT_JSON) NDBG_OUTPUT("  ");
                        NDBG_OUTPUT("{");
                        if(a_pretty) if(a_output_type == OUTPUT_JSON) NDBG_OUTPUT("\n");
                }

                // Host
                if(a_part_map & PART_HOST)
                {
                        if(a_pretty) if(a_output_type == OUTPUT_JSON) NDBG_OUTPUT("    ");
                        NDBG_OUTPUT("\"%sHost%s\": \"%s\"", l_host_color.c_str(), l_off_color.c_str(), (*i_reqlet)->m_url.m_host.c_str());
                        l_fbf = true;
                }

                // Status Code
                if(a_part_map & PART_STATUS_CODE)
                {
                        if(l_fbf) {NDBG_OUTPUT(", "); l_fbf = false;}
                        if(a_pretty) if(a_output_type == OUTPUT_JSON) NDBG_OUTPUT("\n    ");
                        NDBG_OUTPUT("\"%sStatus-Code%s\": %d", l_status_color.c_str(), l_off_color.c_str(), (*i_reqlet)->m_response_status);
                        l_fbf = true;
                }

                // Headers
                // TODO -only in json mode for now
                if(a_output_type == OUTPUT_JSON)
                if(a_part_map & PART_HEADERS)
                {
                        for(header_map_t::iterator i_header = (*i_reqlet)->m_response_headers.begin();
                                        i_header != (*i_reqlet)->m_response_headers.end();
                            ++i_header)
                        {
                                if(l_fbf) {NDBG_OUTPUT(", "); l_fbf = false;}
                                if(a_pretty) if(a_output_type == OUTPUT_JSON) NDBG_OUTPUT("\n    ");
                                NDBG_OUTPUT("\"%s%s%s\": \"%s\"",
                                                l_header_color.c_str(), i_header->first.c_str(), l_off_color.c_str(),
                                                i_header->second.c_str());
                                l_fbf = true;
                        }
                }

                // Body
                if(a_part_map & PART_BODY)
                {
                        if(l_fbf) {NDBG_OUTPUT(", "); l_fbf = false;}
                        if(a_pretty) if(a_output_type == OUTPUT_JSON) NDBG_OUTPUT("\n    ");
                        NDBG_OUTPUT("\"%sBody%s\": %s", l_body_color.c_str(), l_off_color.c_str(), (*i_reqlet)->m_response_body.c_str());
                        l_fbf = true;
                }

                if(a_output_type == OUTPUT_JSON)
                {
                        if(a_pretty) NDBG_OUTPUT("\n  ");
                        NDBG_OUTPUT("}");
                        if(l_cur_reqlet < (l_reqlet_num - 1)) NDBG_OUTPUT(", ");
                }
                NDBG_OUTPUT("\n");
        }

        if(a_output_type == OUTPUT_JSON)
        {
                NDBG_OUTPUT("]\n");
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
reqlet *reqlet_repo::get_reqlet(void)
{
        reqlet *l_reqlet = NULL;

        pthread_mutex_lock(&m_mutex);
        if(!m_reqlet_list.empty() &&
           (m_num_get < m_num_reqlets))
        {
                l_reqlet = *m_reqlet_list_iter;
                ++m_num_get;
                ++m_reqlet_list_iter;
        }
        pthread_mutex_unlock(&m_mutex);

        return l_reqlet;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
reqlet *reqlet_repo::try_get_resolved(void)
{
        reqlet *l_reqlet = NULL;
        int32_t l_status;

        l_reqlet = get_reqlet();
        if(NULL == l_reqlet)
        {
                return NULL;
        }

        // Try resolve
        l_status = l_reqlet->resolve();
        if(STATUS_OK != l_status)
        {
                up_resolved(true);
                return NULL;
        }

        up_resolved(false);
        return l_reqlet;

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t reqlet_repo::add_reqlet(reqlet *a_reqlet)
{
        bool l_was_empty = false;
        if(m_reqlet_list.empty())
        {
                l_was_empty = true;
        }

        //NDBG_PRINT("Adding to repo.\n");
        m_reqlet_list.push_back(a_reqlet);
        ++m_num_reqlets;

        if(l_was_empty)
        {
                m_reqlet_list_iter = m_reqlet_list.begin();
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void reqlet_repo::display_status_line(bool a_color)
{
        if(a_color)
        {
                printf("Done/Resolved/Req'd/Total/Error %s%8u%s / %s%8u%s / %s%8u%s / %s%8u%s / %s%8u%s\n",
                                ANSI_COLOR_FG_GREEN, m_num_done, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_MAGENTA, m_num_resolved, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_YELLOW, m_num_get, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_BLUE, m_num_reqlets, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_RED, m_num_error, ANSI_COLOR_OFF);
        }
        else
        {
                printf("Done/Resolved/Req'd/Total/Error %8u / %8u / %8u / %8u / %8u\n",m_num_done, m_num_resolved, m_num_get, m_num_reqlets, m_num_error);
        }
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
reqlet_repo::reqlet_repo(void):
        m_reqlet_list(),
        m_reqlet_list_iter(),
        m_mutex(),
        m_num_reqlets(0),
        m_num_get(0),
        m_num_done(0),
        m_num_resolved(0),
        m_num_error(0)
{
        // Init mutex
        pthread_mutex_init(&m_mutex, NULL);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
reqlet_repo *reqlet_repo::get(void)
{
        if (m_singleton_ptr == NULL) {
                //If not yet created, create the singleton instance
                m_singleton_ptr = new reqlet_repo();

                // Initialize

        }
        return m_singleton_ptr;
}

//: ----------------------------------------------------------------------------
//: Class variables
//: ----------------------------------------------------------------------------
// the pointer to the singleton for the instance 
reqlet_repo *reqlet_repo::m_singleton_ptr;

