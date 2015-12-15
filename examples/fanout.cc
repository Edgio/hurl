//: ----------------------------------------------------------------------------
//: fanout example:
//: compile with:
//:   g++ fanout.cc -lhlxcore -lssl -lcrypto -lpthread -o fanout
//: ----------------------------------------------------------------------------
#include <hlx/hlx.h>
#include <hlx/phurl_h.h>
//#include <google/profiler.h>

class quitter: public ns_hlx::default_rqst_h
{
public:
        // GET
        ns_hlx::h_resp_t do_get(ns_hlx::hlx &a_hlx,
                                ns_hlx::hconn &a_hconn,
                                ns_hlx::rqst &a_rqst,
                                const ns_hlx::url_pmap_t &a_url_pmap)
        {
                a_hlx.stop();
                return ns_hlx::H_RESP_DONE;
        }
};

int main(void)
{
        ns_hlx::lsnr *l_lsnr = new ns_hlx::lsnr(12345, ns_hlx::SCHEME_TCP);
        ns_hlx::phurl_h *l_phurl_h = new ns_hlx::phurl_h();

        // ---------------------------------------
        // Setup parallel hurl endpoint
        // ---------------------------------------
        // Add hosts
        l_phurl_h->add_host("www.yahoo.com", 443);
        l_phurl_h->add_host("www.google.com", 443);
        l_phurl_h->add_host("www.reddit.com", 443);

        // Setup subr template
        ns_hlx::subr &l_subr = l_phurl_h->get_subr_template();
        l_subr.set_port(443);
        l_subr.set_scheme(ns_hlx::SCHEME_TLS);

        // Add endpoints
        l_lsnr->add_endpoint("/phurl", l_phurl_h);
        l_lsnr->add_endpoint("/quit", new quitter());

        ns_hlx::hlx *l_hlx = new ns_hlx::hlx();
        l_hlx->add_lsnr(l_lsnr);
        l_hlx->set_num_threads(0);
        l_hlx->set_use_persistent_pool(true);
        l_hlx->set_num_parallel(32);
        //ProfilerStart("tmp.prof");
        l_hlx->run();
        delete l_hlx;
        //ProfilerStop();
}
