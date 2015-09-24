//: ----------------------------------------------------------------------------
//: hlx_server example:
//: compile with:
//:   g++ hlx_server_ex.cc -lhlxcore -lssl -lcrypto -lpthread -o hlx_server_ex
//: ----------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <hlo/hlx_server.h>

class bananas_getter: public ns_hlx::default_http_request_handler
{
public:
        // GET
        int32_t do_get(const ns_hlx::url_param_map_t &a_url_param_map,
                       ns_hlx::http_req &a_request,
                       ns_hlx::http_resp &ao_response)
        {
                char l_len_str[64];
                uint32_t l_body_len = strlen("Hello World");
                sprintf(l_len_str, "%u", l_body_len);
                ao_response.write_status(ns_hlx::HTTP_STATUS_OK);
                ao_response.write_header("Content-Type", "html/txt");
                ao_response.write_header("Content-Length", l_len_str);
                ao_response.write_body("Hello World", l_body_len);
                return 0;
        }
};

int main(void)
{
        ns_hlx::hlx_server *l_hlx_server = new ns_hlx::hlx_server();
        l_hlx_server->set_port(12345);
        l_hlx_server->add_endpoint("/bananas", new bananas_getter());
        l_hlx_server->set_num_threads(0);
        l_hlx_server->run();
}


