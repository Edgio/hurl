//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    parsed_url.cc
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
#include "parsed_url.h"
#include "ndebug.h"
#include <string.h>

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string munch(const std::string &a_str)
{
	size_t l_endln_pos = 0;
	size_t l_cr_pos = 0;
	l_endln_pos = a_str.find("\n");
	l_cr_pos = a_str.find("\r");
	if(l_endln_pos > l_cr_pos)
	{
		l_endln_pos = l_cr_pos;
	}
	return a_str.substr(0, l_endln_pos);
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t parsed_url::parse(const std::string &a_url)
{

	//NDBG_PRINT("a_url: %s\n", a_url.c_str());

	// -----------------------------------------------------
	// scheme
	// -----------------------------------------------------
	//NDBG_PRINT("SCHEME\n");

	// Find scheme prefix "://"
	size_t l_prefix_pos = 0;
	l_prefix_pos = a_url.find("://", 0);
	if(l_prefix_pos != std::string::npos)
	{
		std::string l_scheme_str;
		l_scheme_str = a_url.substr(0, l_prefix_pos);

		if(strcasecmp(l_scheme_str.c_str(), "https") == 0)
		{
			m_scheme = nconn::SCHEME_HTTPS;
		}
		else if(strcasecmp(l_scheme_str.c_str(), "http") == 0)
		{
			m_scheme = nconn::SCHEME_HTTP;
		} else {
			fprintf(stderr, "Error unrecognnized scheme\n");
			return STATUS_ERROR;
		}

		// Add three past the pos to increment past position
		l_prefix_pos += 3;
	}
	else
	{
		m_scheme = nconn::SCHEME_HTTP;
		l_prefix_pos = 0;
	}

	// -----------------------------------------------------
	// path
	// -----------------------------------------------------
	std::string l_uri_str;
	std::string l_host_str;
	// Get up to end of string or /
	l_uri_str = a_url.substr(l_prefix_pos + strlen(""), a_url.length());
	//NDBG_PRINT("PATH: uri: %s\n", l_uri_str.c_str());

	size_t l_host_slash_pos = 0;
	l_host_slash_pos = l_uri_str.find("/", 0);

	// Get the path
	if(l_host_slash_pos != std::string::npos)
	{
		m_path = munch(l_uri_str.substr(l_host_slash_pos, l_uri_str.length()));
		l_host_str = l_uri_str.substr(0, l_host_slash_pos);
	}
	else
	{
		l_host_str = l_uri_str;
	}


	// -----------------------------------------------------
	// port
	// -----------------------------------------------------

	//NDBG_PRINT("SEARCHING FOR PORT: l_host_str: %s\n", l_host_str.c_str());

	// Find port prefix ":"
	size_t l_port_pos = 0;
	l_port_pos = l_host_str.find(":", 0);
	// Get the path

	//NDBG_PRINT("SEARCHING FOR PORT: l_port_pos: %Zu\n", l_port_pos);

	if(l_port_pos != std::string::npos)
	{
		m_host = munch(l_host_str.substr(0, l_port_pos));
		m_port = (uint16_t)strtoul(l_host_str.data() + l_port_pos + 1, (char **) NULL, 10);
	}
	else
	{

		m_host = munch(l_host_str);

		// Use defaults for types
		if(nconn::SCHEME_HTTP == m_scheme)
		{
			m_port = 80;
		}
		else if(nconn::SCHEME_HTTPS == m_scheme)
		{
			m_port = 443;
		}
		else
		{
			m_port = 80;
		}
	}

	// Debug...
	//show();

	return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void parsed_url::show(void)
{
	if(m_scheme == nconn::SCHEME_HTTP)
	printf("scheme: %s\n", "HTTP");
	else if(m_scheme == nconn::SCHEME_HTTPS)
	printf("scheme: %s\n", "HTTPS");
	else
	printf("scheme: %s\n", "Unrecognized");
	printf("host:   %s\n", m_host.c_str());
	printf("port:   %d\n", m_port);
	printf("path:   %s\n", m_path.c_str());
}


