//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Edgecast, Inc.
//: All Rights Reserved
//:
//: \file:    t_async_resolver.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    02/07/2014
//:
//: ----------------------------------------------------------------------------
#ifndef _T_ASYNC_RESOLVER_H
#define _T_ASYNC_RESOLVER_H


//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "reqlet.h"
#include "trcdebug.h"
#include "so_cache.h"

#include <pthread.h>
#include <signal.h>
#include <pcre.h>

#include <list>

// Unbound...
#include <unbound.h>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
// Resolve callback function...
typedef int32_t (*resolve_cb_t)(void* a_cb_obj);

typedef struct lookup_struct
{

        reqlet *m_reqlet;
        resolve_cb_t m_resolve_cb;
        int64_t m_start_time_ms;
        int64_t m_intransit_id;

        lookup_struct(reqlet *a_reqlet,
                                 resolve_cb_t a_resolve_cb) :
                m_reqlet(a_reqlet),
                m_resolve_cb(a_resolve_cb),
                m_start_time_ms(-1),
                m_intransit_id(-1)
        {};

private:
        //: ------------------------------------------------
        //: Private methods
        //: ------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(lookup_struct)

} lookup_t;

typedef std::list <lookup_t *> lookup_list_t;

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class t_async_resolver
{
public:
        //: ------------------------------------------------
        //: Consts
        //: ------------------------------------------------
        static const uint64_t SC_MAX_PARALLEL_LOSTATUS_OKUPS = 100;
        static const uint64_t SC_LOSTATUS_OKUP_TIMEOUT_MS = 1000;

        typedef enum {

                LOSTATUS_OKUP_STATUS_STATUS_ERROR = -1,
                LOSTATUS_OKUP_STATUS_ADDED = 0,
                LOSTATUS_OKUP_STATUS_EARLY = 1,

                LOSTATUS_OKUP_STATUS_NONE = 2

        } sc_lookup_status_t;

        //: ------------------------------------------------
        //: Public methods
        //: ------------------------------------------------
        ~t_async_resolver();
        //void read_sip_file(const std::string &a_sip_file);

        // run
        int run(void);

        // run thread...
        void *t_run(void *a_nothing);

        void stop(void) { m_stopped = true; }
        bool is_running(void) { return !m_stopped; }

        sc_lookup_status_t try_fast_lookup(reqlet *a_reqlet,
                        resolve_cb_t a_resolve_cb);

        sc_lookup_status_t add_lookup(reqlet *a_reqlet,
                        resolve_cb_t a_resolve_cb);

        //: ------------------------------------------------
        //: Class methods
        //: ------------------------------------------------
    // Get the singleton instance
    static t_async_resolver *get(void);

        //: ------------------------------------------------
        //: Public members
        //: ------------------------------------------------

        // Needs to be public for now -to join externally
        pthread_t m_t_run_thread;
        pthread_mutex_t m_list_mutex;
        so_cache <lookup_t *> m_lookup_intransit;
        uint64_t m_lookup_list_queued_num;
        uint64_t m_lookup_num;


private:
        //: ------------------------------------------------
        //: Private methods
        //: ------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(t_async_resolver)

        t_async_resolver();

        //Helper for pthreads
        static void *t_run_static(void *a_context)
        {
                return reinterpret_cast<t_async_resolver *>(a_context)->t_run(NULL);
        }

        //: ------------------------------------------------
        //: Private members
        //: ------------------------------------------------
        sig_atomic_t m_stopped;


        // Lookup lists
        lookup_list_t m_lookup_list_queued;

        // IP Address string checking
        pcre *m_ip_address_re_compiled;
        pcre_extra *m_ip_address_pcre_extra;


        //: ------------------------------------------------
        //: Unbound support...
        //: ------------------------------------------------
        struct ub_ctx* m_unbound_ctx;

        //: ------------------------------------------------
        //: Class members
        //: ------------------------------------------------
        // the pointer to the singleton for the instance of analysisd
        static t_async_resolver *m_singleton_ptr;


};

#endif



