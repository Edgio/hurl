//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    reqlet_repo.h
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
#ifndef _REQLET_REPO_H
#define _REQLET_REPO_H


//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "ndebug.h"

#include <pthread.h>
#include <list>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
class reqlet;
typedef std::list <reqlet *> reqlet_list_t;


//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class reqlet_repo
{

public:
        // -------------------------------------------------
        // enums
        // -------------------------------------------------
        typedef enum {
                OUTPUT_LINE_DELIMITED,
                OUTPUT_JSON
        } output_type_t;

        typedef enum {
                PART_HOST = 1,
                PART_STATUS_CODE = 1 << 1,
                PART_HEADERS = 1 << 2,
                PART_BODY = 1 << 3
        } output_part_t;

        reqlet *get_reqlet(void);
        int32_t add_reqlet(reqlet *a_reqlet);

        // Get the singleton instance
        static reqlet_repo *get(void);

        uint32_t get_num_reqlets(void) {return m_num_reqlets;};
        uint32_t get_num_get(void) {return m_num_get;};
        bool empty(void) {return m_reqlet_list.empty();};
        bool done(void) {return (m_num_get >= m_num_reqlets);};
        void up_done(bool a_error) { ++m_num_done; if(a_error)++m_num_error;};
        void up_resolved(bool a_error) {if(a_error)++m_num_error; else ++m_num_resolved;};
        void display_status_line(bool a_color);
        void dump_all_responses(bool a_color, bool a_pretty, output_type_t a_output_type, int a_part_map);
        reqlet *try_get_resolved(void);

private:
        DISALLOW_COPY_AND_ASSIGN(reqlet_repo)
        reqlet_repo();

        reqlet_list_t m_reqlet_list;
        reqlet_list_t::iterator m_reqlet_list_iter;
        pthread_mutex_t m_mutex;
        uint32_t m_num_reqlets;
        uint32_t m_num_get;
        uint32_t m_num_done;
        uint32_t m_num_resolved;
        uint32_t m_num_error;


        // -------------------------------------------------
        // Class members
        // -------------------------------------------------
        // the pointer to the singleton for the instance 
        static reqlet_repo *m_singleton_ptr;

};

#endif
