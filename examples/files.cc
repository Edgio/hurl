//: ----------------------------------------------------------------------------
//: file server example:
//: compile with:
//:   g++ files.cc -lhlxcore -lssl -lcrypto -lpthread -ludns -o files
//: ----------------------------------------------------------------------------
#include <hlx/hlx.h>
#include <hlx/file_h.h>
#include <hlx/lsnr.h>
#include <string.h>
//#include <google/profiler.h>

int main(void)
{
        ns_hlx::lsnr *l_lsnr = new ns_hlx::lsnr(12345, ns_hlx::SCHEME_TCP);
        ns_hlx::file_h *l_file_getter = new ns_hlx::file_h();
        l_lsnr->add_route("/*", l_file_getter);
        ns_hlx::hlx *l_hlx = new ns_hlx::hlx();
        l_hlx->register_lsnr(l_lsnr);
        // Run in foreground w/ threads == 0
        l_hlx->set_num_threads(0);
        //ProfilerStart("tmp.prof");
        l_hlx->run();
        //l_hlx->wait_till_stopped();
        //ProfilerStop();
        if(l_hlx) {delete l_hlx; l_hlx = NULL;}
        return 0;
}
