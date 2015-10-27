//: ----------------------------------------------------------------------------
//: hlx_server example:
//: compile with:
//:   g++ hlx_server_ex.cc -lhlxcore -lssl -lcrypto -lpthread -o hlx_server_ex
//: ----------------------------------------------------------------------------
#include <hlx/hlx.h>
#include <string.h>

class twootter_getter: public ns_hlx::default_http_request_handler
{
public:
        // GET
        int32_t do_get(ns_hlx::hlx &a_hlx,
                       ns_hlx::nconn &a_nconn,
                       ns_hlx::http_req &a_request,
                       const ns_hlx::url_param_map_t &a_url_param_map,
                       ns_hlx::http_resp &ao_resp)
        {
                // https://api.twitter.com/1.1/statuses/lookup.json
                ns_hlx::subreq &l_subreq = a_hlx.create_subreq("twootter_subreq");
                l_subreq.init_with_url("https://api.twitter.com/1.1/statuses/lookup.json");
                //l_subreq.init_with_url("http://127.0.0.1:8089/TEST_FILE");
                l_subreq.set_completion_cb(completion_cb);
                l_subreq.set_header("User-Agent", "hlx");
                l_subreq.set_header("Accept", "*/*");
                l_subreq.set_req_conn(&a_nconn);
                l_subreq.set_req_resp(&ao_resp);
                l_subreq.set_keepalive(true);
                l_subreq.set_num_reqs_per_conn(100);
                a_hlx.add_subreq(l_subreq);
                return 0;
        }

        // Completion
        static int32_t completion_cb(ns_hlx::nconn &a_nconn,
                                     ns_hlx::subreq &a_subreq,
                                     ns_hlx::http_resp &a_subreq_resp)
        {
                ns_hlx::nconn *l_req_nconn = a_subreq.get_req_conn();
                if(!l_req_nconn)
                {
                        printf("Error getting subrequest requester connection.\n");
                        return -1;
                }

                char l_len_str[64];
                char *l_buf = NULL;
                uint32_t l_len;
                a_subreq_resp.get_body(&l_buf, l_len);
                sprintf(l_len_str, "%u", l_len);
                ns_hlx::http_resp &l_resp = *(a_subreq.get_req_resp());
                l_resp.write_status(ns_hlx::HTTP_STATUS_OK);
                l_resp.write_header("Content-Length", l_len_str);
                l_resp.write_body(l_buf, l_len);
                if(l_buf)
                {
                        free(l_buf);
                        l_buf = NULL;
                }
                return 0;
        }
};

int main(void)
{
        ns_hlx::listener *l_listener = new ns_hlx::listener(12345, ns_hlx::SCHEME_TCP);
        l_listener->add_endpoint("/twootter", new twootter_getter());
        ns_hlx::hlx *l_hlx = new ns_hlx::hlx();
        l_hlx->add_listener(l_listener);
        l_hlx->set_num_threads(0);
        //l_hlx->set_verbose(true);
        //l_hlx->set_color(true);
        l_hlx->set_use_persistent_pool(true);
        l_hlx->run();
}
