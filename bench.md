# Throughput and tail latency

Both runs used `wrk` on 26 October 2026, default log level `info`, against `127.0.0.1:8001`, for 30 seconds each. `127.0.0.1` was used so `wrk` did not try IPv6 first. The first run is the thread-pool server. The second is the same server after the kqueue loop replaced the workers.

The page size did not change. `/` is still about 330 bytes on the wire, and `file.jpeg` is still about 26 KB.

## Thread pool

### What we ran

```bash
wrk -t4 -c1  -d30s --latency http://127.0.0.1:8001/
wrk -t4 -c8  -d30s --latency http://127.0.0.1:8001/
wrk -t4 -c64 -d30s --latency http://127.0.0.1:8001/
wrk -t4 -c8  -d30s --latency http://127.0.0.1:8001/file.jpeg
wrk -t1 -c1  -d30s --latency http://127.0.0.1:8001/
```

`-t4 -c1` did not start. `wrk` requires at least as many connections as threads. The single-connection floor was rerun with `-t1 -c1`. The first JPEG attempt failed with `Connection refused` because the server was not listening. The second JPEG run is the one recorded below.

### Raw output

```text
wrk -t4 -c1  -d30s --latency http://127.0.0.1:8001/
number of connections must be >= threads

Running 30s test @ http://127.0.0.1:8001/
  4 threads and 8 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   439.59us    1.21ms  45.62ms   97.55%
    Req/Sec     6.46k     1.03k    7.94k    69.08%
  Latency Distribution
     50%  265.00us
     75%  359.00us
     90%  557.00us
     99%    3.86ms
  771108 requests in 30.01s, 244.15MB read
Requests/sec:  25694.12
Transfer/sec:      8.14MB

Running 30s test @ http://127.0.0.1:8001/
  4 threads and 64 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   322.28us  378.69us  38.62ms   96.58%
    Req/Sec    12.42k     6.60k   22.79k    55.50%
  Latency Distribution
     50%  261.00us
     75%  341.00us
     90%  485.00us
     99%    1.13ms
  741814 requests in 30.04s, 234.87MB read
Requests/sec:  24693.37
Transfer/sec:      7.82MB

wrk -t4 -c8 -d30s --latency http://127.0.0.1:8001/file.jpeg
unable to connect to 127.0.0.1:8001 Connection refused

Running 30s test @ http://127.0.0.1:8001/file.jpeg
  4 threads and 8 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    10.13ms   37.65ms 243.79ms   93.46%
    Req/Sec     5.47k     1.37k   10.08k    68.17%
  Latency Distribution
     50%  297.00us
     75%  414.00us
     90%    2.43ms
     99%  206.83ms
  651992 requests in 30.03s, 16.10GB read
Requests/sec:  21707.75
Transfer/sec:    549.02MB

wrk -t1 -c1 -d30s --latency http://127.0.0.1:8001/
Running 30s test @ http://127.0.0.1:8001/
  1 threads and 1 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   139.20us  578.36us  26.52ms   98.36%
    Req/Sec    11.05k     1.84k   13.18k    77.08%
  Latency Distribution
     50%   75.00us
     75%   98.00us
     90%  156.00us
     99%    1.34ms
  330895 requests in 30.10s, 104.77MB read
Requests/sec:  10991.96
Transfer/sec:      3.48MB
```

### What we obtained

| Target | Threads | Connections | Requests/sec | Transfer/sec | 50% | 99% |
| --- | --- | --- | --- | --- | --- | --- |
| `/` | 1 | 1 | 10,992 | 3.48 MB | 75 µs | 1.34 ms |
| `/` | 4 | 8 | 25,694 | 8.14 MB | 265 µs | 3.86 ms |
| `/` | 4 | 64 | 24,693 | 7.82 MB | 261 µs | 1.13 ms |
| `/file.jpeg` | 4 | 8 | 21,708 | 549 MB | 297 µs | 207 ms |

`/` at 8 connections moved 244 MB in 30 seconds, about 330 bytes per response including headers and chunk framing. `file.jpeg` moved 16.10 GB, about 26 KB per response.

### What we can infer

One connection is the floor: about 11,000 requests/sec, with half of the responses done in 75 µs.

Eight connections do a little more than twice that work, about 25,700 requests/sec, not eight times as much. The typical response slows from 75 µs to about 265 µs once the 8 workers are shared.

Sixty-four connections do not raise throughput. Requests/sec fall slightly, to about 24,700. The 99th percentile for `/` stays in the 1–4 ms range across these runs, and it moved between samples, so that tail is noisy for this small page. Eight connections already saturate `/`.

`file.jpeg` is a different limit. Request rate drops to about 21,700/sec while transfer rises to 549 MB/sec. Half of those responses are still near 300 µs, but the 99th percentile is 207 ms. The 10 ms average is pulled up by that tail. The long tail belongs to the larger file, not to `/`.

## Kqueue, same commands

The server was rebuilt and left listening. `LOG_LEVEL` stayed unset. The four runs were issued together, so each 30-second test followed the previous one.

```bash
wrk -t1 -c1  -d30s --latency http://127.0.0.1:8001/
wrk -t4 -c8  -d30s --latency http://127.0.0.1:8001/
wrk -t4 -c64 -d30s --latency http://127.0.0.1:8001/
wrk -t4 -c8  -d30s --latency http://127.0.0.1:8001/file.jpeg
```

While eight Python sockets held one keep-alive response and then sat idle, a ninth request was:

```bash
curl -w "waited %{time_total}s\n" -o /dev/null http://127.0.0.1:8001/
```

### Raw output

```text
Running 30s test @ http://127.0.0.1:8001/
  1 threads and 1 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.07ms   11.36ms 136.44ms   91.40%
    Req/Sec     3.33k     1.63k    6.20k    61.94%
  Latency Distribution
     50%  187.00us
     75%    0.93ms
     90%   12.70ms
     99%   56.66ms
  98168 requests in 30.03s, 31.08MB read
Requests/sec:   3268.53
Transfer/sec:      1.03MB

Running 30s test @ http://127.0.0.1:8001/
  4 threads and 8 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.61ms    7.63ms 112.68ms   92.04%
    Req/Sec     1.27k   419.39     2.01k    65.09%
  Latency Distribution
     50%    1.17ms
     75%    2.03ms
     90%    7.17ms
     99%   39.44ms
  151271 requests in 30.10s, 47.90MB read
Requests/sec:   5026.35
Transfer/sec:      1.59MB

Running 30s test @ http://127.0.0.1:8001/
  4 threads and 64 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    63.58ms  197.62ms   1.90s    95.36%
    Req/Sec     0.88k   456.62     2.72k    65.03%
  Latency Distribution
     50%   14.75ms
     75%   29.01ms
     90%   81.96ms
     99%    1.24s
  97364 requests in 30.09s, 30.83MB read
Requests/sec:   3235.59
Transfer/sec:      1.02MB

Running 30s test @ http://127.0.0.1:8001/file.jpeg
  4 threads and 8 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     7.26ms   21.59ms 311.71ms   93.81%
    Req/Sec     0.92k   333.48     1.84k    64.10%
  Latency Distribution
     50%    1.58ms
     75%    2.85ms
     90%   16.38ms
     99%  122.09ms
  109372 requests in 30.08s, 2.70GB read
Requests/sec:   3636.18

waited 0.001070s
```

The JPEG block in the paste ends at `Requests/sec`. `2.70GB` read in `30.08s` is about 92 MB/s. The ninth `curl` received 225 bytes, the same `index.html` body, and printed `waited 0.001070s` while the eight sockets were still held.

### What we obtained

| Target | Threads | Connections | Requests/sec | Transfer/sec | 50% | 99% |
| --- | --- | --- | --- | --- | --- | --- |
| `/` | 1 | 1 | 3,269 | 1.03 MB | 187 µs | 56.66 ms |
| `/` | 4 | 8 | 5,026 | 1.59 MB | 1.17 ms | 39.44 ms |
| `/` | 4 | 64 | 3,236 | 1.02 MB | 14.75 ms | 1.24 s |
| `/file.jpeg` | 4 | 8 | 3,636 | ~92 MB | 1.58 ms | 122 ms |

| Target | Connections | Thread pool | kqueue |
| --- | --- | --- | --- |
| `/` | 1 | 10,992 req/s, p50 75 µs, p99 1.34 ms | 3,269 req/s, p50 187 µs, p99 56.66 ms |
| `/` | 8 | 25,694 req/s, p50 265 µs, p99 3.86 ms | 5,026 req/s, p50 1.17 ms, p99 39.44 ms |
| `/` | 64 | 24,693 req/s, p50 261 µs, p99 1.13 ms | 3,236 req/s, p50 14.75 ms, p99 1.24 s |
| `/file.jpeg` | 8 | 21,708 req/s, 549 MB/s, p99 207 ms | 3,636 req/s, ~92 MB/s, p99 122 ms |

### What we can infer

The idle check is the change kqueue was added for. Eight connections stayed open after one response, and the ninth finished in 0.001 s. On the thread pool that ninth request waited until one of the eight workers left `recv`.

`wrk` does not show that win. It sends the next request immediately, so every connection is busy. On that workload the one loop thread is slower than eight workers.

One connection drops from about 11,000 req/s to about 3,300. The median only moves from 75 µs to 187 µs. The average of 4.07 ms is the tail: the 99th percentile is 57 ms and the slowest sample is 136 ms.

Eight connections rise only from 3,269 to 5,026 req/s. The pool roughly doubled over that same step, to about 25,700, because eight workers could read and send together. This loop handles one ready socket through the end of the response before it looks at the next.

Sixty-four connections fall back to about 3,200 req/s, and the 99th percentile becomes 1.24 s. The pool never served more than eight sockets at once, so its 64-connection run stayed near its 8-connection run. kqueue accepts all 64. A `send` that fills the kernel buffer then waits inside `poll` in `Socket::send`, and every other ready client waits behind that call.

`file.jpeg` at 8 connections falls from 21,700 req/s and 549 MB/s to 3,600 req/s and about 92 MB/s. The 99th percentile shortens from 207 ms to 122 ms. One request is not slower. Fewer of them are in flight, because one thread reads and sends the file.

The busy-server loss is that single thread, plus `poll` inside `send` holding it. Registering the socket for write with `EVFILT_WRITE`, and returning to `kevent` when the send buffer is full, is the next measurement to take with these same commands.
