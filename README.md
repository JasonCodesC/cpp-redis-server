# Mini Redis-ish Server

## Overview
This project is a Redis-style key/value server written in modern C++ with nonblocking TCP, epoll, and RESP parsing. It keeps a simple in-memory store with optional expirations, a command dispatcher (SET/GET/DEL/EXISTS/EXPIRE/TTL/PING/ECHO), and a small Python load generator to measure throughput/latency. Optimizations were guided by perf and timing data.

## System Design
- **Network:** Nonblocking `accept4` + epoll; per-connection read/write buffers; backpressure by toggling `EPOLLOUT` only when writes are queued.
- **Protocol:** Streaming RESP array-of-bulk parser; RESP encoder helpers for status, bulk strings, integers, arrays.
- **Store:** `unordered_map` for keys, optional expirations using `steady_clock`; lazy expiry on access plus sweep hook.
- **Commands:** Dispatcher maps argv → handlers; minimal allocations via `string_view` plumbing.
- **Client load:** Python driver sends a pipelined mix of SET/GET/DEL/EXISTS/PING over TCP to a fixed keyspace, records throughput and p50/p95/p99 latencies.

## File Structure
```
redis_engine
├── makefile                            # builds kvserv
├── src
│   ├── main.cpp                        # epoll loop, accept/read/write wiring
│   ├── commands/dispatcher.            # command handlers
│   ├── db/store.*                      # in-memory KV + expirations
│   ├── net/{socket,epoll,connection}.  # sockets/epoll/per-connection buffers
│   ├── protocol/{resp,resp_parser}.    # RESP encoder/parser
│   └── util/{error,time}.hpp           # helpers
├── client/runner.py                    # load generator (pipelined RESP client)
├── utils/redis.sh                      # build+run server
├── utils/client.sh                     # run client load
└── data, plots                         # experiment outputs
```

## How to Run
From repo root:
```
# server (hardcoded to listen on port 9000)
./utils/redis.sh

# client load (hardcoded host 192.168.37.1, port 9000)
./utils/client.sh
```

## Profiling & Optimizations (V2)
- **on_write (~67% of CPU):** dominated by kernel `__send()`; not much to squeeze here beyond batching and avoiding extra wakeups.
- **on_read (~19%):** half in `recv` (syscall), but inside it:
  - **handle_get (~3.8%):** most time in `db::store::get`, with ~1.6% in `steady_clock::now` and the rest in hash lookup and also saw RESP string encoding overhead. Fixes: only call `now()` when the key has an expiry; make `get` return `std::optional<string_view>` to avoid copies alongisde swapping RESP `to_string` for `to_chars` to stay on-stack.
  - **handle_set (~2.2%):** split between `db::store::set` and `free` from allocations. Fixes: move semantics on inserts and pass `string_view` through dispatcher→store to avoid extra allocations.
  - **parse (~1.8%):** two-pass integer parsing (find terminator then `from_chars`). Fix: single-pass parsing over the buffer rather than the double pass and avoid transient `string_view` construction.
- **CPU pinning:** pin server threads during tests to reduce cpu-migration to 0 (shown via perf stats).

## Results (client-perspective latency/throughput)
Client uses pipelined requests (depth 16) over TCP with a keyspace=200, Averages exclude the single 200M run as that was only run once compared to the others being run three times. Errors were 0 in all runs.

### V1 (baseline)
| Workload | Avg elapsed (s) | Avg throughput (ops/s) | p50 (ms) | p95 (ms) | p99 (ms) |
|:--|--:|--:|--:|--:|--:|
| 2M ops (avg of 3)  | 14.163 | 141,218 | 0.31 | 0.59 | 0.76 |
| 20M ops (avg of 3) | 129.372 | 154,662 | 0.28 | 0.53 | 0.69 |

### V2 (optimized)
| Workload | Avg elapsed (s) | Avg throughput (ops/s) | p50 (ms) | p95 (ms) | p99 (ms) |
|:--|--:|--:|--:|--:|--:|
| 2M ops (avg of 3)  | 13.154 | 152,059 | 0.29 | 0.54 | 0.69 |
| 20M ops (avg of 3) | 128.969 | 155,077 | 0.28 | 0.52 | 0.66 |
| 200M ops (single)  | 1278.375 | 156,449 | 0.28 | 0.52 | 0.66 |

### Speedups and Latency Reductions vs V1 (averages)
| Workload | Throughput speedup | p50 delta | p95 delta | p99 delta |
|:--|--:|--:|--:|--:|
| 2M ops  | 1.08x | 0.02 ms | 0.05 ms | 0.07 ms |
| 20M ops | 1.01x |  0.00 ms | 0.01 ms | 0.03 ms |

## Architecture Recap
- Listener on `AF_INET` TCP, nonblocking; `EPOLLIN` for reads, `EPOLLOUT` toggled when write buffer non-empty.
- Per-connection buffers + RESP parser to extract argv; dispatcher writes RESP replies; backpressure via bounded buffers.
- Store with optional expirations; lazy cleanup on access; TTL/EXPIRE commands mapped directly to deadlines.
- Client load emphasizes realistic pressure: fixed keyspace to maintain hit rate, pipelining to simulate in-flight ops, client-perceived latency as primary SLO.

## Final Thoughts
Built a compact, performant Redis-like server with clear layering and measurable gains from profiling-led tweaks. Future work: richer command set, RESP3, finer-grained server-side metrics, and configurable binding per interface.
