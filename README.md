# hurl
> _HTTP Server Load Test utility written in C++_

## Table of Contents

- [Background](#background)
- [Install](#install)
- [Usage](#usage)
- [Contribute](#contribute)
- [License](#license)

## Background

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
  -l, --seconds        Run for <N> seconds.
  -s, --silent         Silent mode.
  -A, --rate           Max Request Rate -per sec.
  -T, --timeout        Timeout (seconds).
  -x, --no_stats       Don't collect stats -faster.
  -I, --addr_seq       Sequence over local address range.
  -S, --chunk_size_kb  Chunk size in kB -max bytes to read/write per socket read/write. Default 8 kB
  -F, --rand_xfwd      Generate a random X-Forwarded-For header per request)
  
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

## Contribute

- We welcome issues, questions and pull requests.


## License

This project is licensed under the terms of the Apache 2.0 open source license. Please refer to the `LICENSE-2.0.txt` file for the full terms.

