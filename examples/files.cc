//: ----------------------------------------------------------------------------
//: file server example:
//: compile with:
//:   g++ files.cc -lhlxcore -lssl -lcrypto -lpthread -ludns -o files
//: ----------------------------------------------------------------------------
#include <hlx/hlx.h>
#include <string.h>
//#include <google/profiler.h>

class file_getter: public ns_hlx::default_rqst_h
{
        // GET
        ns_hlx::h_resp_t do_get(ns_hlx::hlx &a_hlx,
                                ns_hlx::hconn &a_hconn,
                                ns_hlx::rqst &a_rqst,
                                const ns_hlx::url_pmap_t &a_url_pmap)
        {
                //printf("GET_PATH: %s\n", a_rqst.get_path().c_str());
                return get_file(a_hlx, a_hconn, a_rqst, a_rqst.get_uri_path());
        }
};

int main(void)
{
        ns_hlx::lsnr *l_lsnr = new ns_hlx::lsnr(12345, ns_hlx::SCHEME_TCP);
        file_getter *l_file_getter = new file_getter();
        l_lsnr->register_endpoint("/*", l_file_getter);
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
