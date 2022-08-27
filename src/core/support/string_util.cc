//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include "support/string_util.h"
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t break_header_string(const std::string &a_header_str,
		std::string &ao_header_key,
		std::string &ao_header_val)
{
	// Find port prefix ":"
	size_t l_colon_pos = 0;
	l_colon_pos = a_header_str.find(":", 0);
	if (l_colon_pos == std::string::npos)
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
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
std::string get_file_wo_path(const std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind("/");
        if (pos == std::string::npos)
        {
                return fName;
        }
        if (pos == 0)
        {
                return fName;
        }
        return fName.substr(pos + 1, fName.length());
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
std::string get_file_path(const std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind("/");
        if (pos == std::string::npos)
        {
                return fName;
        }
        if (pos == 0)
        {
                return fName;
        }
        return fName.substr(0, pos);
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
std::string get_base_filename(const std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind(".");
        if (pos == std::string::npos)
        {
                return fName;
        }
        if (pos == 0)
        {
                return fName;
        }
        return fName.substr(0, pos);
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
std::string get_file_ext(const std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind(".");
        if (pos == std::string::npos)
        {
                return std::string();
        }
        if (pos == 0)
        {
                return std::string();
        }
        return fName.substr(pos + 1, fName.length());
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
std::string get_file_wo_ext(const std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind(".");
        if (pos == std::string::npos)
        {
                return std::string();
        }
        if (pos == 0)
        {
                return std::string();
        }
        return fName.substr(0, pos);
}
} //namespace ns_hurl {
