//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    parsed_url.h
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
#ifndef _PARSED_URL_H
#define _PARSED_URL_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <string>
#include <vector>
#include <map>
#include <list>
#include "nconn.h"

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------



//: ----------------------------------------------------------------------------
//: \details: parsed_url
//: ----------------------------------------------------------------------------
class parsed_url
{

public:

        // -------------------------------------------
        // Public methods
        // -------------------------------------------
        parsed_url() :
                m_scheme(nconn::SCHEME_HTTP),
                m_host(""),
                m_port(80),
                m_path("")
        {}

        void show(void);
        int32_t parse(const std::string &a_url);

        // -------------------------------------------
        // Public members
        // -------------------------------------------
        nconn::scheme_t m_scheme;
        std::string m_host;
        uint16_t m_port;
        std::string m_path;

private:

        // -------------------------------------------
        // Private methods
        // -------------------------------------------

        // -------------------------------------------
        // Private members
        // -------------------------------------------
        //std::string m_query;


};

#endif
