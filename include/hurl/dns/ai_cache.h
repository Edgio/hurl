//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    ai_cache.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    11/20/2015
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
#ifndef _AI_CACHE_H
#define _AI_CACHE_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <string>
#include <map>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define NRESOLVER_DEFAULT_AI_CACHE_FILE "/tmp/addr_info_cache.json"

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------

namespace ns_hurl {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
struct host_info;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class ai_cache
{
public:
        //: ------------------------------------------------
        //: Public methods
        //: ------------------------------------------------
        ai_cache(const std::string &a_ai_cache_file);
        ~ai_cache();
        host_info *lookup(const std::string a_label);
        host_info *lookup(const std::string a_label, host_info *a_host_info);
        void add(const std::string a_label, host_info *a_host_info);

private:
        //: ------------------------------------------------
        //: Types
        //: ------------------------------------------------
        typedef std::map <std::string, host_info *> ai_cache_map_t;

        //: ------------------------------------------------
        //: Private methods
        //: ------------------------------------------------
        // Disallow copy/assign
        ai_cache& operator=(const ai_cache &);
        ai_cache(const ai_cache &);

        static int32_t sync(const std::string &a_ai_cache_file,
                            const ai_cache_map_t &a_ai_cache_map);
        static int32_t read(const std::string &a_ai_cache_file,
                            ai_cache_map_t &ao_ai_cache_map);

        //: ------------------------------------------------
        //: Private members
        //: ------------------------------------------------
        ai_cache_map_t m_ai_cache_map;
        std::string m_ai_cache_file;
};

} //namespace ns_hurl {

#endif



