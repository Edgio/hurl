//: ----------------------------------------------------------------------------
//: cgi example:
//: compile with:
//:   g++ cgi.cc -lhlxcore -lssl -lcrypto -lpthread -o cgi
//: ----------------------------------------------------------------------------
#include <hlx/srvr.h>
#include <hlx/cgi_h.h>
#include <hlx/lsnr.h>
#include <hlx/trace.h>
#include <string.h>
//#include <google/profiler.h>

int main(void)
{
        ns_hlx::trc_log_level_set(ns_hlx::TRC_LOG_LEVEL_NONE);
        ns_hlx::lsnr *l_lsnr = new ns_hlx::lsnr(12345, ns_hlx::SCHEME_TCP);
        ns_hlx::cgi_h *l_cgi_h = new ns_hlx::cgi_h();
        l_cgi_h->set_root("./cgi-bin");
        l_cgi_h->set_timeout_ms(2000);
        l_lsnr->add_route("/*", l_cgi_h);
        ns_hlx::srvr *l_srvr = new ns_hlx::srvr();
        l_srvr->register_lsnr(l_lsnr);
        // Run in foreground w/ threads == 0
        l_srvr->set_num_threads(0);
        //ProfilerStart("tmp.prof");
        l_srvr->run();
        //l_srvr->wait_till_stopped();
        //ProfilerStop();
        if(l_srvr) {delete l_srvr; l_srvr = NULL;}
        return 0;
}
