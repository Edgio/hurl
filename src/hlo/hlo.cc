//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlo.cc
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
#include "hlo.h"
#include "stats_collector.h"
#include "t_client.h"
#include "util.h"
#include "ssl_util.h"

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlo::set_url(const std::string &a_url)
{
	int32_t l_retval = STATUS_OK;

	m_url = a_url;

	return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlo::set_url_file(const std::string &a_url_file)
{
	int32_t l_retval = STATUS_OK;

	m_url_file = a_url_file;

	return l_retval;

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlo::set_header(const std::string &a_header_key, const std::string &a_header_val)
{
	int32_t l_retval = STATUS_OK;
	m_header_map[a_header_key] = a_header_val;
	return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlo::set_end_fetches(int32_t a_val)
{
	m_end_fetches = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlo::set_max_reqs_per_conn(int64_t a_val)
{
	m_max_reqs_per_conn = a_val;
	if((m_max_reqs_per_conn > 1) || (m_max_reqs_per_conn < 0))
	{
		set_header("Connection", "keep-alive");
	}
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlo::set_rate(int32_t a_val)
{
	m_rate = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlo::set_reqlet_mode(reqlet_mode_t a_mode)
{
	m_reqlet_mode = a_mode;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlo::display_results(double a_elapsed_time,
		uint32_t a_max_parallel,
		bool a_show_breakdown_flag)
{
	stats_collector::get()->display_results(a_elapsed_time, a_max_parallel, a_show_breakdown_flag, m_color);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlo::display_results_http_load_style(double a_elapsed_time,
		uint32_t a_max_parallel,
		bool a_show_breakdown_flag,
		bool a_one_line_flag)
{
	stats_collector::get()->display_results_http_load_style(a_elapsed_time, a_max_parallel, a_show_breakdown_flag, a_one_line_flag);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlo::display_results_line_desc(bool a_color_flag)
{
	stats_collector::get()->display_results_line_desc(a_color_flag);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlo::display_results_line(bool a_color_flag)
{
	stats_collector::get()->display_results_line(a_color_flag);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlo::get_stats_json(char *l_json_buf, uint32_t l_json_buf_max_len)
{return stats_collector::get()->get_stats_json(l_json_buf, l_json_buf_max_len);}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlo::run(void)
{
	int32_t l_retval = STATUS_OK;

	// Check is initialized
	if(!m_is_initd)
	{
		l_retval = init();
		if(STATUS_OK != l_retval)
		{
			NDBG_PRINT("Error: performing init.\n");
			return STATUS_ERROR;
		}
	}


	stats_collector::get()->set_start_time_ms(get_time_ms());

	// Caculate num parallel per thread
	uint32_t l_num_parallel_conn_per_thread = m_start_parallel / m_num_threads;
	if(l_num_parallel_conn_per_thread < 1) l_num_parallel_conn_per_thread = 1;

        uint32_t l_num_fetches_per_thread = m_end_fetches / m_num_threads;
        uint32_t l_remainder_fetches = m_end_fetches % m_num_threads;

	// -------------------------------------------
	// Create t_client list...
	// -------------------------------------------
	for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
	{

		if(m_verbose)
		{
			NDBG_PRINT("Creating...\n");
		}

		// Construct with settings...
		t_client *l_t_client = new t_client(
			m_verbose,
			m_color,
			m_sock_opt_recv_buf_size,
			m_sock_opt_send_buf_size,
			m_sock_opt_no_delay,
			m_cipher_list,
			m_ssl_ctx,
			m_evr_type,
			l_num_parallel_conn_per_thread,
			m_run_time_s,
			m_timeout_s,
			m_max_reqs_per_conn,
			m_url,
			m_url_file,
			m_wildcarding
			);

                if (i_client_idx == (m_num_threads - 1))
                        l_num_fetches_per_thread += l_remainder_fetches;

		l_t_client->set_rate(m_rate);
		l_t_client->set_end_fetches(l_num_fetches_per_thread);
		l_t_client->set_reqlet_mode(m_reqlet_mode);

		for(header_map_t::iterator i_header = m_header_map.begin();i_header != m_header_map.end(); ++i_header)
		{
			l_t_client->set_header(i_header->first, i_header->second);
		}

		m_t_client_list.push_back(l_t_client);

	}

	// Set start time...
	stats_collector::get()->set_start_time_ms(get_time_ms());

	// -------------------------------------------
	// Run...
	// -------------------------------------------
	for(t_client_list_t::iterator i_client = m_t_client_list.begin();
		i_client != m_t_client_list.end();
		++i_client)
	{

		if(m_verbose)
		{
			NDBG_PRINT("Running...\n");
		}
		(*i_client)->run();

	}

	return l_retval;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlo::stop(void)
{
	int32_t l_retval = STATUS_OK;

	for (t_client_list_t::iterator i_t_client = m_t_client_list.begin();
			i_t_client != m_t_client_list.end();
			++i_t_client)
	{
		(*i_t_client)->stop();
	}

	return l_retval;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlo::wait_till_stopped(void)
{
	int32_t l_retval = STATUS_OK;

	// -------------------------------------------
	// Join all threads before exit
	// -------------------------------------------
	for(t_client_list_t::iterator i_client = m_t_client_list.begin();
			i_client != m_t_client_list.end(); ++i_client)
	{

		//if(l_settings.m_verbose)
		//{
		//	NDBG_PRINT("joining...\n");
		//}
		pthread_join(((*i_client)->m_t_run_thread), NULL);

	}

	return l_retval;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool hlo::is_running(void)
{
	for (t_client_list_t::iterator i_client = m_t_client_list.begin();
			i_client != m_t_client_list.end(); ++i_client)
	{
		if((*i_client)->is_running())
			return true;
	}

	return false;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlo::init(void)
{
	// Check if already is initd
	if(m_is_initd)
		return STATUS_OK;

	// SSL init...
	m_ssl_ctx = ssl_init(m_cipher_list);
	if(NULL == m_ssl_ctx) {
		NDBG_PRINT("Error: performing ssl_init with cipher_list: %s\n", m_cipher_list.c_str());
		return STATUS_ERROR;
	}

	// -------------------------------------------
	// Start async resolver
	// -------------------------------------------
	//t_async_resolver::get()->run();

	m_is_initd = true;
	return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlo::hlo(void) :
        m_t_client_list(),
	m_ssl_ctx(NULL),
	m_is_initd(false),
	m_cipher_list(""),
	m_verbose(false),
	m_color(false),
	m_quiet(false),
	m_sock_opt_recv_buf_size(0),
	m_sock_opt_send_buf_size(0),
	m_sock_opt_no_delay(false),
	m_evr_type(EV_EPOLL),
	m_start_parallel(64),
	m_end_fetches(-1),
	m_max_reqs_per_conn(1),
	m_run_time_s(-1),
	m_timeout_s(HLO_DEFAULT_CONN_TIMEOUT_S),
	m_num_threads(1),
	m_url(),
	m_url_file(),
	m_header_map(),
	m_rate(-1),
	m_reqlet_mode(REQLET_MODE_ROUND_ROBIN),
        m_wildcarding(true)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlo::~hlo()
{

	// -------------------------------------------
	// Delete t_client list...
	// -------------------------------------------
	for(t_client_list_t::iterator i_client = m_t_client_list.begin();
			i_client != m_t_client_list.end(); )
	{

		t_client *l_t_client_ptr = *i_client;
		delete l_t_client_ptr;
		m_t_client_list.erase(i_client++);

	}

	// SSL Cleanup
	ssl_kill_locks();

	// TODO Deprecated???
	//EVP_cleanup();

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlo *hlo::get(void)
{
	if (m_singleton_ptr == NULL) {
		//If not yet created, create the singleton instance
		m_singleton_ptr = new hlo();

		// Initialize

	}
	return m_singleton_ptr;
}

//: ----------------------------------------------------------------------------
//: Class variables
//: ----------------------------------------------------------------------------
// the pointer to the singleton for the instance 
hlo *hlo::m_singleton_ptr;


