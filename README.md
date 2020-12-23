# hurl
> _HTTP Server Load Test utility written in C++_

## Table of Contents

- [Background](#background)
- [Install](#install)
- [Usage](#usage)
- [Contribute](#contribute)
- [License](#license)

## Background
A few utilities for testing and curling from http servers.

### *hurl* HTTP Server Load Tester
*hurl* is an http server load tester similar to ab/siege/weighttp/wrk with support for tls, http2, multithreading, parallelism, url ranges.  *hurl* is primarily useful for benchmarking http server applications.

* **A little more about URLs Ranges**:
*hurl* has support for range expansion in urls which is useful for testing a server's capability to serve from many files. *hurl* will expand the ranges specified in the wildcards and perform requests in user configurable orders (see the "--mode" option in help).
eg: "http://127.0.0.1:8089/[1-100]/my_[1-9]_file.html".

#### An example
```bash
>hurl "https://google.com" --calls=100 -p100 -f1000
Running 1 threads 100 parallel connections per thread with 100 requests per connection
+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+
| Completed / Requested |    IdlKil |    Errors | kBytes Recvd |   Elapsed |       Req/s |      MB/s |
+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+
|       572 /       665 |         0 |         0 |       334.43 |     1.00s |    1118.00s |     0.33s |
|      1000 /      1000 |         0 |         0 |       257.73 |     1.50s |     668.66s |     0.25s |
| RESULTS:             ALL
| fetches:             1000
| max parallel:        100
| bytes:               3.644410e+05
| seconds:             1.501000
| mean bytes/conn:     364.441000
| fetches/sec:         666.222518
| bytes/sec:           2.427988e+05
| HTTP response codes: 
| 200 -- 1000
```

### *phurl* Parallel Curl
*phurl* is a parallel curling utility useful for pulling a single url from many different hosts. *phurl* supports reading line delimited hosts from stdin, a shell command string, or a file.

#### An example
```bash
>printf "www.google.com\nwww.yahoo.com\nwww.reddit.com\n" | phurl -p2 -t3 -u"https://bloop.com/" -s -T5 -o output.json
Done:        0 Reqd:        0 Pendn:        3 Flight:        0 Error:        0
Done:        0 Reqd:        3 Pendn:        3 Flight:        3 Error:        0
Done:        1 Reqd:        3 Pendn:        2 Flight:        2 Error:        0
Done:        2 Reqd:        3 Pendn:        1 Flight:        1 Error:        0
Done:        3 Reqd:        3 Pendn:        0 Flight:        0 Error:        0
Done:        3 Reqd:        3 Pendn:        0 Flight:        0 Error:        0
****************** SUMMARY ******************** 
| total hosts:                                3
| success:                                    3
| error:                                      0
| error address lookup:                       0
| error connectivity:                         0
| error unknown:                              0
| tls error cert hostname:                    0
| tls error cert self-signed:                 0
| tls error cert expired:                     0
| tls error cert issuer:                      0
| tls error other:                            0
+--------------- ALPN PROTOCOLS --------------- 
| NA                                          3
+--------------- ALPN STRING ------------------ 
| NA                                          3
+--------------- SSL PROTOCOLS ---------------- 
| TLSv1.2                                     3
+--------------- SSL CIPHERS ------------------ 
| ECDHE-RSA-AES128-GCM-SHA256                 3
```

## Install

## OS requirements:
Linux/OS X (kqueue support coming soon-ish)

### Install dependencies:
Library requirements:
* libssl/libcrypto (OpenSSL)

### OS X Build requirements (brew)
```bash
brew install cmake
brew install openssl
```

### Building the tools
```bash
./build_simple.sh
```

And optionally install
```bash
cd ./build
sudo make install
```

## Usage

### `hurl`
`hurl --help`

```sh
Usage: hurl [http[s]://]hostname[:port]/path [options]
Options are:
  -h, --help           Display this help and exit.
  -V, --version        Display the version number and exit.
  
Run Options:
  -4, --ipv4           Resolve name to IPv4 address.
  -6, --ipv6           Resolve name to IPv6 address.
  -w, --no_wildcards   Don't wildcard the url.
  -M, --mode           Request mode -if multipath [random(default) | sequential].
  -d, --data           HTTP body data -supports curl style @ file specifier
  -p, --parallel       Num parallel. Default: 100.
  -f, --fetches        Num fetches.
  -N, --calls          Number of requests per connection (or stream if H2)
  -1, --h1             Force http 1.x
  -t, --threads        Number of parallel threads. Default: 1
  -H, --header         Request headers -can add multiple ie -H<> -H<>...
  -X, --verb           Request command -HTTP verb to use -GET/PUT/etc. Default GET
  -l, --seconds        Run for <N> seconds .
  -A, --rate           Max Request Rate -per sec.
  -T, --timeout        Timeout (seconds).
  -x, --no_stats       Don't collect stats -faster.
  -I, --addr_seq       Sequence over local address range.
  -S, --chunk_size_kb  Chunk size in kB -max bytes to read/write per socket read/write. Default 8 kB
  
TLS Settings:
  -y, --cipher         Cipher --see "openssl ciphers" for list.
  -O, --tls_options    SSL Options string.
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

### `phurl`
`phurl --help`

```sh
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
```

## Contribute

- We welcome issues, questions and pull requests.


## License

This project is licensed under the terms of the Apache 2.0 open source license. Please refer to the `LICENSE-2.0.txt` file for the full terms.

