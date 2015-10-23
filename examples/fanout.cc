//: ----------------------------------------------------------------------------
//: hlx_server example:
//: compile with:
//:   g++ hlx_server_ex.cc -lhlxcore -lssl -lcrypto -lpthread -o hlx_server_ex
//: ----------------------------------------------------------------------------
#include <hlx/hlx.h>
#include <string.h>

class fanout_getter: public ns_hlx::default_http_request_handler
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
                ns_hlx::subreq &l_subreq = a_hlx.create_subreq("fanout_subreq");
                l_subreq.init_with_url("https://blorp/");
                ns_hlx::server_list_t l_server_list;
                l_server_list.push_back("www.yahoo.com");
                l_server_list.push_back("www.google.com");
                l_server_list.push_back("www.reddit.com");
                l_subreq.set_server_list(l_server_list);
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
                //printf("completion_cb: \n");
                //a_resp.show_headers();
                ns_hlx::nconn *l_req_nconn = a_subreq.get_req_conn();
                if(!l_req_nconn)
                {
                        printf("Error getting subrequest requester connection.\n");
                        return -1;
                }

                char l_len_str[64];
                sprintf(l_len_str, "%lu", a_subreq_resp.get_body().length());
                ns_hlx::http_resp &l_resp = *(a_subreq.get_req_resp());
                l_resp.write_status(ns_hlx::HTTP_STATUS_OK);
                l_resp.write_header("Content-Length", l_len_str);
                std::string l_all_resp = a_subreq.dump_all_responses(false,
                                                                     false,
                                                                     ns_hlx::subreq::OUTPUT_LINE_DELIMITED,
                                                                     ns_hlx::subreq::PART_HOST
                                                                   | ns_hlx::subreq::PART_SERVER
                                                                   | ns_hlx::subreq::PART_STATUS_CODE
                                                                   | ns_hlx::subreq::PART_HEADERS
                                                                   | ns_hlx::subreq::PART_BODY);
                l_resp.write_body(l_all_resp.c_str(), l_all_resp.length());
                return 0;
        }
};

int main(void)
{
        ns_hlx::listener *l_listener = new ns_hlx::listener(12345, ns_hlx::SCHEME_TCP);
        l_listener->add_endpoint("/fanout", new fanout_getter());
        ns_hlx::hlx *l_hlx = new ns_hlx::hlx();
        l_hlx->add_listener(l_listener);
        l_hlx->set_num_threads(0);
        l_hlx->set_use_persistent_pool(true);
        l_hlx->run();
}
