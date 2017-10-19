  hurl: HTTP Server Load Test in C++
=========

## What are they
A few utilities for testing and curling from http servers.

## *hurl* HTTP Server Load Tester
*hurl* is an http server load tester similar to ab/siege/weighttp/wrk with support for tls, http2, multithreading, parallelism, url ranges.  *hurl* is primarily useful for benchmarking http server applications.

* **A little more about URLs Ranges**:
*hurl* has support for range expansion in urls which is useful for testing a server's capability to serve from many files. *hurl* will expand the ranges specified in the wildcards and perform requests in user configurable orders (see the "--mode" option in help).
eg: "http://127.0.0.1:8089/[1-100]/my_[1-9]_file.html".

####An example
```bash
hurl "http://127.0.0.1/index.html" --num_calls=100 -p100 -f100000 -c
```

####Options
```bash
Usage: hurl [http[s]://]hostname[:port]/path [options]
Options are:
  -h, --help           Display this help and exit.
  -V, --version        Display the version number and exit.
  
Run Options:
  -w, --no_wildcards   Don't wildcard the url.
  -M, --mode           Request mode -if multipath [random(default) | sequential].
  -d, --data           HTTP body data -supports curl style @ file specifier
  -p, --parallel       Num parallel. Default: 100.
  -f, --fetches        Num fetches.
  -N, --calls          Number of requests per connection (or stream if H2)
  -t, --threads        Number of parallel threads. Default: 1
  -H, --header         Request headers -can add multiple ie -H<> -H<>...
  -X, --verb           Request command -HTTP verb to use -GET/PUT/etc. Default GET
  -l, --seconds        Run for <N> seconds .
  -A, --rate           Max Request Rate -per sec.
  -T, --timeout        Timeout (seconds).
  -x, --no_stats       Don't collect stats -faster.
  -I, --addr_seq       Sequence over local address range.
  
Display Options:
  -v, --verbose        Verbose logging
  -c, --no_color       Turn off colors
  -C, --responses      Display http(s) response codes instead of request statistics
  -L, --responses_per  Display http(s) response codes per interval instead of request statistics
  -U, --update         Update output every N ms. Default 500ms.
  
Results Options:
  -j, --json           Display results in json
  -o, --output         Output results to file <FILE> -default to stdout
  
Debug Options:
  -r, --trace          Turn on tracing (error/warn/debug/verbose/all)
  
Note: If running long jobs consider enabling tcp_tw_reuse -eg:
echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse
```

## *phurl* Parallel Curl
*phurl* is a parallel curling utility useful for pulling a single url from many different hosts. *phurl* supports reading line delimited hosts from stdin, a shell command string, or a file.

####An example
```bash
printf "www.google.com\nwww.yahoo.com\nwww.reddit.com\n" | phurl -p2 -t3 -u"https://bloop.com/" -s -c -T5
```

####Options
```bash
Usage: phurl -u [http[s]://]hostname[:port]/path [options]
Options are:
  -h, --help           Display this help and exit.
  -V, --version        Display the version number and exit.
  
URL Options -or without parameter
  -u, --url            URL -REQUIRED (unless running cli: see --cli option).
  -d, --data           HTTP body data -supports curl style @ file specifier
  
Hostname Input Options -also STDIN:
  -f, --host_file      Host name file.
  -J, --host_json      Host listing json format.
  -x, --execute        Script to execute to get host names.
  
Settings:
  -p, --parallel       Num parallel.
  -t, --threads        Number of parallel threads.
  -H, --header         Request headers -can add multiple ie -H<> -H<>...
  -X, --verb           Request command -HTTP verb to use -GET/PUT/etc
  -T, --timeout        Timeout (seconds).
  -n, --no_async_dns   Use getaddrinfo to resolve.
  -k, --no_cache       Don't use addr info cache.
  -A, --ai_cache       Path to Address Info Cache (DNS lookup cache).
  -C, --connect_only   Only connect -do not send request.
  -Q, --complete_time  Cancel requests after N seconds.
  -W, --complete_ratio Cancel requests after % complete (0.0-->100.0).
  
TLS Settings:
  -y, --cipher         Cipher --see "openssl ciphers" for list.
  -O, --tls_options    SSL Options string.
  -K, --tls_verify     Verify server certificate.
  -N, --tls_sni        Use SSL SNI.
  -B, --tls_self_ok    Allow self-signed certificates.
  -M, --tls_no_host    Skip host name checking.
  -F, --tls_ca_file    SSL CA File.
  -L, --tls_ca_path    SSL CA Path.
  
Print Options:
  -v, --verbose        Verbose logging
  -c, --no_color       Turn off colors
  -m, --show_summary   Show summary output
  
Output Options: -defaults to line delimited
  -o, --output         File to write output to. Defaults to stdout
  -l, --line_delimited Output <HOST> <RESPONSE BODY> per line
  -j, --json           JSON { <HOST>: "body": <RESPONSE> ...
  -P, --pretty         Pretty output
  
  
Note: If running large jobs consider enabling tcp_tw_reuse -eg:
echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse
```

## Building

##OS requirements:
Linux/OS X (kqueue support coming soon-ish)

###Install dependencies:
Library requirements:
* libssl/libcrypto (OpenSSL)

###OS X Build requirements (brew)
```bash
brew install cmake
brew install openssl
```

###Building the tools
```bash
./build_simple.sh
```

And optionally install
```bash
cd ./build
sudo make install
```
