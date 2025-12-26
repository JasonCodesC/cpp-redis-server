#!/usr/bin/env python3
"""
Lightweight client/load generator for the mini-redis server.

Sends a mix of SET/GET/DEL/EXISTS/PING commands with a bounded keyspace to keep
hit rates reasonable. Collects basic latency/throughput metrics.
"""

import random
import socket
import string
import threading
import time
from dataclasses import dataclass, field
from typing import List, Optional

# Hardcoded client/server settings (no CLI flags needed).
HOST = "192.168.37.1"
PORT = 9000
TOTAL_OPS = 200_000
CONCURRENCY = 50
KEYSPACE_SIZE = 200
VALUE_LEN = 16
SET_RATIO = 0.4
GET_RATIO = 0.4
DEL_RATIO = 0.1
EXISTS_RATIO = 0.05
PIPELINE_DEPTH = 16


def encode_command(*parts: str) -> bytes:
  """Build a RESP Array of Bulk Strings."""
  out = [f"*{len(parts)}\r\n".encode()]
  for p in parts:
    out.append(f"${len(p)}\r\n".encode())
    out.append(p.encode())
    out.append(b"\r\n")
  return b"".join(out)


def recv_exact(sock: socket.socket, n: int) -> bytes:
  data = bytearray()
  while len(data) < n:
    chunk = sock.recv(n - len(data))
    if not chunk:
      raise ConnectionError("socket closed while reading")
    data.extend(chunk)
  return bytes(data)


def read_line(sock: socket.socket) -> bytes:
  buf = bytearray()
  while True:
    ch = sock.recv(1)
    if not ch:
      raise ConnectionError("socket closed while reading line")
    buf.extend(ch)
    if len(buf) >= 2 and buf[-2:] == b"\r\n":
      return bytes(buf[:-2])


def read_resp(sock: socket.socket):
  """Minimal RESP reader for simple/bulk/int responses."""
  prefix = sock.recv(1)
  if not prefix:
    raise ConnectionError("socket closed before response prefix")

  if prefix == b"+" or prefix == b"-":
    line = read_line(sock)
    return line.decode()
  if prefix == b":":
    line = read_line(sock)
    return int(line)
  if prefix == b"$":
    length_line = read_line(sock)
    length = int(length_line)
    if length == -1:
      return None
    data = recv_exact(sock, length)
    recv_exact(sock, 2)  # trailing CRLF
    return data
  if prefix == b"*":
    # Simple array handling for future expansion; not heavily used here.
    length_line = read_line(sock)
    length = int(length_line)
    if length == -1:
      return None
    return [read_resp(sock) for _ in range(length)]

  raise ValueError(f"unknown RESP prefix {prefix!r}")


def random_value(length: int) -> str:
  return "".join(random.choice(string.ascii_lowercase) for _ in range(length))


def pick_key(keyspace: List[str]) -> str:
  return random.choice(keyspace)


@dataclass
class WorkerConfig:
  ops: int
  keyspace: List[str]
  value_len: int
  set_ratio: float
  get_ratio: float
  del_ratio: float
  exists_ratio: float
  pipeline: int


@dataclass
class WorkerResult:
  latencies_ms: List[float] = field(default_factory=list)
  errors: int = 0
  ops: int = 0


def worker_thread(cfg: WorkerConfig, result: WorkerResult):
  sock = socket.create_connection((HOST, PORT))
  pending: List[float] = []
  sent = 0
  completed = 0
  try:
    while completed < cfg.ops:
      # Fill pipeline up to depth or until all ops sent.
      while len(pending) < cfg.pipeline and sent < cfg.ops:
        r = random.random()
        if r < cfg.set_ratio:
          key = pick_key(cfg.keyspace)
          val = random_value(cfg.value_len)
          cmd = encode_command("SET", key, val)
        elif r < cfg.set_ratio + cfg.get_ratio:
          key = pick_key(cfg.keyspace)
          cmd = encode_command("GET", key)
        elif r < cfg.set_ratio + cfg.get_ratio + cfg.del_ratio:
          key = pick_key(cfg.keyspace)
          cmd = encode_command("DEL", key)
        elif r < cfg.set_ratio + cfg.get_ratio + cfg.del_ratio + cfg.exists_ratio:
          key = pick_key(cfg.keyspace)
          cmd = encode_command("EXISTS", key)
        else:
          cmd = encode_command("PING")

        start = time.perf_counter()
        sock.sendall(cmd)
        pending.append(start)
        sent += 1

      # Drain one response per loop to keep ordering.
      try:
        read_resp(sock)
        start_time = pending.pop(0)
      except Exception:
        result.errors += 1
        pending.clear()
        break
      else:
        completed += 1
        result.ops += 1
        result.latencies_ms.append((time.perf_counter() - start_time) * 1000.0)
  finally:
    sock.close()


def pct(values: List[float], p: float) -> Optional[float]:
  if not values:
    return None
  idx = int(p / 100.0 * (len(values) - 1))
  return sorted(values)[idx]


def run_load(args):
  keyspace = [f"key:{i}" for i in range(KEYSPACE_SIZE)]
  per_worker = TOTAL_OPS // CONCURRENCY
  cfg = WorkerConfig(
      ops=per_worker,
      keyspace=keyspace,
      value_len=VALUE_LEN,
      set_ratio=SET_RATIO,
      get_ratio=GET_RATIO,
      del_ratio=DEL_RATIO,
      exists_ratio=EXISTS_RATIO,
      pipeline=PIPELINE_DEPTH,
  )

  threads = []
  results = [WorkerResult() for _ in range(CONCURRENCY)]
  start = time.perf_counter()
  for i in range(CONCURRENCY):
    t = threading.Thread(target=worker_thread, args=(cfg, results[i]), daemon=True)
    threads.append(t)
    t.start()
  for t in threads:
    t.join()
  elapsed = time.perf_counter() - start

  all_ops = sum(r.ops for r in results)
  all_errors = sum(r.errors for r in results)
  latencies = [l for r in results for l in r.latencies_ms]

  print(f"Server: {HOST}:{PORT}")
  print(f"Completed ops: {all_ops}, errors: {all_errors}, elapsed: {elapsed:.3f}s")
  if elapsed > 0:
    print(f"Throughput: {all_ops / elapsed:.1f} ops/sec")
  if latencies:
    for label, val in [("p50", pct(latencies, 50)), ("p95", pct(latencies, 95)), ("p99", pct(latencies, 99))]:
      if val is not None:
        print(f"{label}: {val:.2f} ms")


if __name__ == "__main__":
  random.seed(42)
  run_load(None)
