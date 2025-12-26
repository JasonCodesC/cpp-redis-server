

todo

speed up

cpu pin





V1:

2 mill ops:

Run 1:

Completed ops: 1999998, errors: 0, elapsed: 14.166s
Throughput: 141186.2 ops/sec
p50: 0.31 ms
p95: 0.59 ms
p99: 0.77 ms

Run 2:

Completed ops: 1999998, errors: 0, elapsed: 14.106s
Throughput: 141787.9 ops/sec
p50: 0.30 ms
p95: 0.59 ms
p99: 0.77 ms

Run 3:

Completed ops: 1999998, errors: 0, elapsed: 14.217s
Throughput: 140680.7 ops/sec
p50: 0.31 ms
p95: 0.58 ms
p99: 0.75 ms

20 mill ops:

Run 1:

Completed ops: 19999998, errors: 0, elapsed: 133.265s
Throughput: 150077.2 ops/sec
p50: 0.29 ms
p95: 0.55 ms
p99: 0.72 ms

Run 2:

Completed ops: 19999998, errors: 0, elapsed: 127.645s
Throughput: 156684.1 ops/sec
p50: 0.28 ms
p95: 0.53 ms
p99: 0.68 ms

Run 3:

Completed ops: 19999998, errors: 0, elapsed: 127.207s
Throughput: 157223.9 ops/sec
p50: 0.28 ms
p95: 0.52 ms
p99: 0.67 ms

V2:

First optimization CPU pinning

Second used perf record and report and found that on_write was very slow with its children using 67% (lol) of CPU time. Within this all but 0.4% was __send() which we can't really improve bc this is a syscall. 

From there I saw on_read was very slow with its children taking up 18.89% CPU time, half of which was recv (another syscall). But within that ----> 

handle_get is taking up 3.75% cpu time with db::store::get taking up the majority of that and 1.55% of that is steady_clock::now and the other is a hash table. The other part of handle get thats slow is append_string. To improve this I changed from calling steady_clock::now() on every GET and only call it now if it is in expires. I also now changed Store::get to return std::optional<std::string_view> rather than std::optional<std::string> to avoid copies. And to speed up append string I replaced std::to_string with std::to_chars so we can avoid expensive heap allocation and just keep stuff on the stack.

handle_set is taking up 2.2% of the cpu and half of that is due to db::store::set while the other half is free (a syscall). To cut down on both of these we can use std::pmr::unordered_map and string which lets us use a custom allocator using a memory pool rather than new and delete along with using std::move instead of copying. 
Swapped the store to use std::pmr::unordered_map + std::pmr::string with an unsynchronized pool, plus std::move on inserts to reduce allocation/copy overhead in SET. Also shifted Store APIs to std::string_view so the dispatcher can pass RESP args without extra string copies.

the last notable time sink is parse, which is taking up 1.81% of cpu time and that is dominated by calls of parse_integer. This is because we are doing two passes, one to find the terminator and then from_chars() to scan the digits again. We can change this to a single pass to cut down even more. We can also avoid creating a string_view object and parse directly from the buffer data.



fix perf stat permissions




