
//: ----------------------------------------------------------------------------
;//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    mime_types.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    01/02/2016
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
#ifndef _MIME_TYPES_H
#define _MIME_TYPES_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <string>
#include <map>

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: mime types
//: ----------------------------------------------------------------------------
class mime_types
{
public:
        // -------------------------------------------------
        // Types
        // -------------------------------------------------
        typedef std::map<std::string, std::string> ext_type_map_t;

        // -------------------------------------------------
        // ext->type pair
        // -------------------------------------------------
        struct T
        {
                const char* m_ext;
                const char* m_content_type;

                operator ext_type_map_t::value_type() const
                {
                        return std::pair<std::string, std::string>(m_ext, m_content_type);
                }
        };

        // -------------------------------------------------
        // Private class members
        // -------------------------------------------------
        static const T S_EXT_TYPE_PAIRS[];
        static const ext_type_map_t S_EXT_TYPE_MAP;

};

//: ----------------------------------------------------------------------------
//: Generated file extensions -> mime types associations
//: ----------------------------------------------------------------------------
const mime_types::T mime_types::S_EXT_TYPE_PAIRS[] =
{
#include "_gen_mime_types.h"
};

//: ----------------------------------------------------------------------------
//: Map
//: ----------------------------------------------------------------------------
const mime_types::ext_type_map_t mime_types::S_EXT_TYPE_MAP(S_EXT_TYPE_PAIRS,
                                                            S_EXT_TYPE_PAIRS +
                                                                    ARRAY_SIZE(mime_types::S_EXT_TYPE_PAIRS));

} // ns_hlx
#endif
