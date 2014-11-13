//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    httpd.cc
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
#include "t_microhttpd.h"
#include "hlo.h"

#include <stdarg.h>
#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <string.h>
#include <microhttpd.h>
#include <stdio.h>


//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define RESP_BUFFER_SIZE (1024*1024)

//: ----------------------------------------------------------------------------
//: Globals
//: ----------------------------------------------------------------------------
struct MHD_Daemon *g_daemon;
static char g_char_buf[RESP_BUFFER_SIZE];

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int
answer_to_connection (void *cls,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const char *upload_data,
		size_t *upload_data_size,
		void **con_cls)
{

	hlo::get()->get_stats_json(g_char_buf, RESP_BUFFER_SIZE);

	struct MHD_Response *response;
	int ret;

	response = MHD_create_response_from_data(strlen(g_char_buf),
		(void *) g_char_buf,
		0,
		0);

	ret = MHD_add_response_header(response, "Content-Type", "application/json");
	ret = MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
	ret = MHD_add_response_header(response, "Access-Control-Allow-Credentials", "true");
	ret = MHD_add_response_header(response, "Access-Control-Max-Age", "86400");

	ret = MHD_queue_response(connection, 200, response);

	MHD_destroy_response(response);

	return ret;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t start_microhttpd(int32_t a_port)
{

	g_daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY,
			a_port,
			NULL,
			NULL,
			&answer_to_connection,
			NULL,
			MHD_OPTION_END);
	if (NULL == g_daemon)
		return -1;

	return 0;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t stop_microhttpd(void)
{

	MHD_stop_daemon (g_daemon);

	return 0;
}

