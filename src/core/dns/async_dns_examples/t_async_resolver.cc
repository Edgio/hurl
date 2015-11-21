//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Edgecast, Inc.
//: All Rights Reserved
//:
//: \file:    main.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    02/07/2014
//:
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "t_async_resolver.h"
#include "trcdebug.h"

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define IPV4_ADDR_REGEX "\\b\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\b"
// Note matches 999.999.999.999 -check inet_pton result...


//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
static void resolve_cb(void* mydata, int err, struct ub_result* result);


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_async_resolver::t_async_resolver():
	m_t_run_thread(),
	m_lookup_intransit(SC_MAX_PARALLEL_LOSTATUS_OKUPS),
	m_lookup_num(0),
	m_stopped(true)
{

	pthread_mutex_init(&m_list_mutex, NULL);

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_async_resolver::~t_async_resolver()
{

	// Free up the regular expression.
	pcre_free(m_ip_address_re_compiled);

	// Free up the EXTRA PCRE value (may be NULL at this point)
	if(m_ip_address_pcre_extra != NULL)
	{
		pcre_free(m_ip_address_pcre_extra);
	}

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_async_resolver::run(void)
{

	if(!m_stopped)
	{
		TRC_TRACE_PRINT("STATUS_ERROR: Already running\n");
		return STATUS_ERROR;
	}

	// Set to running
	m_stopped = false;

	//TRC_TRACE_PRINT("Setup regex for IPV4 match\n");


	// TODO Move to constructor...
	const char *l_pcre_error_str;
	int l_pcre_error_offset;

	// Setup ip address regexes
	// Check if regex for ipaddr string eg: "127.0.01"
	m_ip_address_re_compiled = pcre_compile(IPV4_ADDR_REGEX, 0, &l_pcre_error_str, &l_pcre_error_offset, NULL);

	// pcre_compile returns NULL on error, and sets
	// pcreErrorOffset & pcreErrorStr
	if(m_ip_address_re_compiled == NULL)
	{
		TRC_TRACE_PRINT("STATUS_ERROR: Could not compile '%s': %s\n", IPV4_ADDR_REGEX, l_pcre_error_str);
		return STATUS_ERROR;
	}

	// Optimize the regex
	m_ip_address_pcre_extra = pcre_study(m_ip_address_re_compiled, 0, &l_pcre_error_str);

	// -----------------------------------------------------
	// pcre_study() returns NULL for both errors and when it
	// can not optimize the regex.  The last argument is how
	// one checks for errors (it is NULL if everything works,
	// and points to an error string otherwise.
	// -----------------------------------------------------
	if(l_pcre_error_str != NULL) {
		TRC_TRACE_PRINT("STATUS_ERROR: Could not study '%s': %s\n", IPV4_ADDR_REGEX, l_pcre_error_str);
		return STATUS_ERROR;
	}


	int32_t l_pthread_error = STATUS_OK;
	l_pthread_error = pthread_create(&m_t_run_thread,
			NULL,
			t_run_static,
			this);
	if (l_pthread_error != 0)
	{
		// failed to create thread

		//TRC_ERROR("DAEMON: Error creating thread for handling daemon signals %s\n.", strerror(l_pthread_error));
		return STATUS_ERROR;

	}

	return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_async_resolver::t_run(void *a_nothing)
{

	// ---------------------------------------------------------------
	// TODO PORT BEGIN
	// ---------------------------------------------------------------
	struct ub_ctx* ctx;
	int retval;

	//TRC_TRACE_PRINT("HERE\n");

	/* create context */
	ctx = ub_ctx_create();
	if (!ctx)
	{
		TRC_TRACE_PRINT("STATUS_ERROR: could not create unbound context\n");
		return NULL;
	}
	if(!ctx) {
		TRC_TRACE_PRINT("STATUS_ERROR: could not create unbound context\n");
		return NULL;
	}
	/* read /etc/resolv.conf for DNS proxy settings (from DHCP) */
	if( (retval=ub_ctx_resolvconf(ctx, (char *)"/etc/resolv.conf")) != 0) {
		TRC_TRACE_PRINT("STATUS_ERROR reading resolv.conf: %s. errno says: %s\n",
			ub_strerror(retval), strerror(errno));
		return NULL;
	}
	/* read /etc/hosts for locally supplied host addresses */
	if( (retval=ub_ctx_hosts(ctx, (char *)"/etc/hosts")) != 0) {
		TRC_TRACE_PRINT("STATUS_ERROR reading hosts: %s. errno says: %s\n",
			ub_strerror(retval), strerror(errno));
		return NULL;
	}

	// ---------------------------------------------------------------
	// TODO PORT END
	// ---------------------------------------------------------------

	//: ----------------------------------------------------
	//: Main loop.
	//: ----------------------------------------------------
	//TRC_TRACE_PRINT("starting main loop\n");
	//TRC_TRACE_PRINT("%sLOSTATUS_OKUP_ADD%s: starting main loop m_stopped: %d\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, m_stopped);

	while(!m_stopped)
	{

		//TRC_TRACE_PRINT("%sLOSTATUS_OKUP_ADD%s: m_lookup_list_queued.size(): %Zu intxt_size: %Zu\n",
		//		ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF,
		//		m_lookup_list_queued.size(),
		//		m_lookup_intransit.size());

		// ---------------------------------------------------------------
		// TODO PORT BEGIN
		// ---------------------------------------------------------------

		// Pop off up to limit...
		while((m_lookup_intransit.size() < SC_MAX_PARALLEL_LOSTATUS_OKUPS) &&
				(m_lookup_list_queued.size()))
		{

			//TRC_TRACE_PRINT("%sLOSTATUS_OKUP_ADD%s: Adding lookup: %"PRIu64" \n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, m_lookup_num);

			pthread_mutex_lock(&m_list_mutex);

			lookup_t *l_lookup = NULL;
			if(m_lookup_list_queued.size())
			{
				l_lookup = m_lookup_list_queued.front();
				m_lookup_list_queued.pop_front();
				m_lookup_list_queued_num = m_lookup_list_queued.size();
				l_lookup->m_intransit_id = m_lookup_num;
				m_lookup_intransit.insert(m_lookup_num, l_lookup);
				++m_lookup_num;
			}
			pthread_mutex_unlock(&m_list_mutex);

			if(l_lookup)
			{
				/* asynchronous query for webserver */
				retval = ub_resolve_async(ctx,
										  (char *)l_lookup->m_reqlet->m_url.m_host.c_str(),
										  1 /* TYPE A (IPv4 address) */,
										  1 /* CLASS IN (internet) */,
										  (void*) l_lookup,
										  resolve_cb,
										  NULL);
				if (retval != 0)
				{
					TRC_TRACE_PRINT("STATUS_ERROR: resolve error: %s\n", ub_strerror(retval));
					return NULL;
				}
			}
		}

		if(m_lookup_intransit.size() > 0)
		{
			retval = ub_process(ctx);
			if (retval != 0)
			{
				TRC_TRACE_PRINT("STATUS_ERROR: resolve error: %s\n", ub_strerror(retval));
				return NULL;
			}
		}

		// Sleep for bit waiting for new lookups to process
		usleep(100000);

		// ---------------------------------------------------------------
		// TODO PORT END
		// ---------------------------------------------------------------

	}


	// ---------------------------------------------------------------
	// TODO PORT BEGIN
	// ---------------------------------------------------------------
	retval = ub_process(ctx);
	if (retval != 0)
	{
		TRC_TRACE_PRINT("STATUS_ERROR: resolve error: %s\n", ub_strerror(retval));
		return NULL;
	}
	printf("done\n");

	ub_ctx_delete(ctx);
	// ---------------------------------------------------------------
	// TODO PORT END
	// ---------------------------------------------------------------

	m_stopped = true;

	return NULL;

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_async_resolver::sc_lookup_status_t t_async_resolver::try_fast_lookup(reqlet *a_reqlet,
		resolve_cb_t a_resolve_cb)
{

	// TODO Move to constructor...
	int l_pcre_exec_ret;
	int l_pcre_sub_str_vec[30];

    // Try to find the regex in aLineToMatch, and report results.
	l_pcre_exec_ret = pcre_exec(m_ip_address_re_compiled,
                            m_ip_address_pcre_extra,
                            a_reqlet->m_url.m_host.c_str(),
                            a_reqlet->m_url.m_host.length(), // length of string
                            0,               // Start looking at this point
                            0,               // OPTIONS
                            l_pcre_sub_str_vec,
                            30);             // Length of subStrVec


    // Report what happened in the pcre_exec call..
    //printf("pcre_exec return: %d\n", l_pcre_exec_ret);
    if((PCRE_ERROR_NOMATCH != l_pcre_exec_ret)
    		&& (l_pcre_exec_ret < 0))
    {
    	// Something bad happened..
		switch(l_pcre_exec_ret)
		{
			//case PCRE_ERROR_NOMATCH      : printf("String did not match the pattern\n");        break;
			case PCRE_ERROR_NULL         : TRC_TRACE_PRINT("STATUS_ERROR: Something was null\n");                      break;
			case PCRE_ERROR_BADOPTION    : TRC_TRACE_PRINT("STATUS_ERROR: A bad option was passed\n");                 break;
			case PCRE_ERROR_BADMAGIC     : TRC_TRACE_PRINT("STATUS_ERROR: Magic number bad (compiled re corrupt?)\n"); break;
			case PCRE_ERROR_UNKNOWN_NODE : TRC_TRACE_PRINT("STATUS_ERROR: Something kooky in the compiled re\n");      break;
			case PCRE_ERROR_NOMEMORY     : TRC_TRACE_PRINT("STATUS_ERROR: Ran out of memory\n");                       break;
			default                      : TRC_TRACE_PRINT("STATUS_ERROR: Unknown error\n");                           break;
		} // end switch
		return LOSTATUS_OKUP_STATUS_ERROR;
    }
    else if(PCRE_ERROR_NOMATCH != l_pcre_exec_ret)
    {
    	//TRC_TRACE_PRINT("Result: We have a match for string: %s\n", a_reqlet->m_url.m_host.c_str());

		// store this IP address in sa:
		int l_status = 0;
		l_status = a_reqlet->slow_resolve();
		if(STATUS_OK != l_status)
		{
			TRC_TRACE_PRINT("STATUS_ERROR: performing slow_resolve.\n");
			return LOSTATUS_OKUP_STATUS_ERROR;
		}
		else
		{
			if(NULL != a_resolve_cb)
			{
				// Callback...
				int l_status = 0;
				l_status = a_resolve_cb(a_reqlet);
				if(STATUS_OK != l_status)
				{
					TRC_TRACE_PRINT("STATUS_ERROR: invoking callback\n");
					return LOSTATUS_OKUP_STATUS_ERROR;
				}

				// We're done...
				//TRC_TRACE_PRINT("MATCH for a_reqlet: %p.\n", a_reqlet);
				//a_reqlet->show_host_info();

				return LOSTATUS_OKUP_STATUS_EARLY;

			}
		}
    }

    // Try a cache db lookup
    // TODO

	return LOSTATUS_OKUP_STATUS_NONE;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_async_resolver::sc_lookup_status_t t_async_resolver::add_lookup(reqlet *a_reqlet,
		resolve_cb_t a_resolve_cb)
{

	//TRC_TRACE_PRINT("%sLOSTATUS_OKUP_RESOLVE%s: ....\n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF);

	//TRC_TRACE_PRINT("LOSTATUS_OKUP for a_reqlet: %p.\n", a_reqlet);

	sc_lookup_status_t l_fast_lookup_status;

	// Try fast lookup first
	l_fast_lookup_status = try_fast_lookup(a_reqlet, a_resolve_cb);
	if(LOSTATUS_OKUP_STATUS_NONE != l_fast_lookup_status)
	{
		// Got something other than none return...
		return l_fast_lookup_status;

	}

	// Create new lookup
	lookup_t *l_lookup = new lookup_t(a_reqlet, a_resolve_cb);

	pthread_mutex_lock(&m_list_mutex);
	m_lookup_list_queued.push_back(l_lookup);
	m_lookup_list_queued_num = m_lookup_list_queued.size();
	pthread_mutex_unlock(&m_list_mutex);

	return LOSTATUS_OKUP_STATUS_ADDED;

}

//: ----------------------------------------------------------------------------
//: \details: examine the result structure in detail
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void examine_result(char* query, struct ub_result* result)
{
	int i;
	int num;

	printf("+ -------------------------------------------------+\n");
	printf(": query:: %s\n", query);
	printf(": qname:  %s\n", result->qname);
	printf(": qtype:  %d\n", result->qtype);
	printf(": qclass: %d\n", result->qclass);
	if(result->canonname)
	printf(": canonical_name: %s\n", result->canonname);
	else
	printf(": canonical name: <none>\n");

	if(result->havedata)
	printf(": has data\n");
	else
	printf(": has no data\n");

	if(result->nxdomain)
	printf(": nxdomain (name does not exist)\n");
	else
	printf(": not an nxdomain (name exists)\n");

	if(result->secure)
	printf(": validated to be secure\n");
	else
	printf(": not validated as secure\n");

	if(result->bogus)
	printf(": a security failure! (bogus)\n");
	else
	printf(": not a security failure (not bogus)\n");

	printf(": DNS rcode: %d\n", result->rcode);

	num = 0;
	for(i=0; result->data[i]; i++) {
	printf(": result data element %d has length %d\n", i, result->len[i]);
	printf(": result data element %d is: %s\n", i, inet_ntoa(*(struct in_addr*)result->data[i]));
	num++;
	}
	printf(": result has %d data element(s)\n", num);
	printf("+ -------------------------------------------------+\n");

}


//: ----------------------------------------------------------------------------
//: \details: called when resolution is completed
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void resolve_cb(void* mydata, int err, struct ub_result* result)
{

	if (err != 0)
	{
		TRC_TRACE_PRINT("STATUS_ERROR: resolve error: %s\n", ub_strerror(err));
		return;
	}
	/* show first result */
	if (result->havedata)
	{

		//++g_addr_resolved;
		//printf(": address[%8d] of %s is %s\n",
		//		0,
		//		//g_addr_resolved,
		//		result->qname,
		//		inet_ntoa(*(struct in_addr*) result->data[0]));

		//examine_result(result->qname, result);


		//if(g_addr_resolved > 3500) exit(0);

		lookup_t *l_lookup = (lookup_t *)mydata;
		t_async_resolver *l_tasr = t_async_resolver::get();

		//TRC_TRACE_PRINT("Resolved host: %s\n", l_lookup->m_reqlet->m_url.m_host.c_str());

		// -------------------------------------------
		// Remove from cache
		// -------------------------------------------
		pthread_mutex_lock(&(l_tasr->m_list_mutex));
		// Remove from cache
		l_tasr->m_lookup_intransit.remove(l_lookup->m_intransit_id);
		pthread_mutex_unlock(&(l_tasr->m_list_mutex));

		// Set the data
		// TODO Just use first result???
		//ns_hlx::mem_display((const uint8_t *)result->data[0], result->len[0]);

		memcpy(&(l_lookup->m_reqlet->m_host_info.m_sa.sin_addr), result->data[0], result->len[0]);
		//l_lookup->m_reqlet->m_host_info.m_sa_len = result->len[0];

		// Callback...
		l_lookup->m_resolve_cb(l_lookup->m_reqlet);
		// TODO Check for errors...

		// Delete the lookup
		delete l_lookup;


		//int* done = (int*) mydata;
		//*done = 1;

	}


	ub_resolve_free(result);
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_async_resolver *t_async_resolver::get(void)
{
	if (m_singleton_ptr == NULL) {
		//If not yet created, create the singleton instance
		m_singleton_ptr = new t_async_resolver();
	}
	return m_singleton_ptr;
}

//: ----------------------------------------------------------------------------
//: Class variables
//: ----------------------------------------------------------------------------

// the pointer to the singleton for the instance of analysisd
t_async_resolver *t_async_resolver::m_singleton_ptr;
