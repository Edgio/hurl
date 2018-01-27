//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    string_util.cc
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
#include "hurl/support/string_util.h"
namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t break_header_string(const std::string &a_header_str,
		std::string &ao_header_key,
		std::string &ao_header_val)
{

	// Find port prefix ":"
	size_t l_colon_pos = 0;
	l_colon_pos = a_header_str.find(":", 0);
	if(l_colon_pos == std::string::npos)
	{
		return -1;
	}
	ao_header_key = a_header_str.substr(0, l_colon_pos);
	++l_colon_pos;
	// ignore spaces...
	while(a_header_str[l_colon_pos] == ' ') ++l_colon_pos;
	ao_header_val = a_header_str.substr(l_colon_pos, a_header_str.length());
	return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string get_file_wo_path(const std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind("/");

        if(pos == std::string::npos)
        {
                return fName;
        }
        if(pos == 0)
        {
                return fName;
        }
        return fName.substr(pos + 1, fName.length());
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string get_file_path(const std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind("/");
        if(pos == std::string::npos)
        {
                return fName;
        }
        if(pos == 0)
        {
                return fName;
        }
        return fName.substr(0, pos);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string get_base_filename(const std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind(".");
        if(pos == std::string::npos)
        {
                return fName;
        }
        if(pos == 0)
        {
                return fName;
        }
        return fName.substr(0, pos);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string get_file_ext(const std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind(".");
        if(pos == std::string::npos)
        {
                return std::string();
        }
        if(pos == 0)
        {
                return std::string();
        }
        return fName.substr(pos + 1, fName.length());
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string get_file_wo_ext(const std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind(".");
        if(pos == std::string::npos)
        {
                return std::string();
        }
        if(pos == 0)
        {
                return std::string();
        }
        return fName.substr(0, pos);
}

} //namespace ns_hurl {
