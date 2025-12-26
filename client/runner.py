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
from datetime import datetime
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Optional

# Hardcoded client/server settings (no CLI flags needed).
HOST = "192.168.37.128"
PORT = 9000
TOTAL_OPS = 20_000_000
CONCURRENCY = 3
KEYSPACE_SIZE = 1000
VALUE_LEN = 16
SET_RATIO = 0.4
GET_RATIO = 0.4
DEL_RATIO = 0.1
EXISTS_RATIO = 0.05
PIPELINE_DEPTH = 16
READ_BUF_SIZE = 4096
DATA_DIR_NAME = "data"


def encode_command(*parts: str) -> bytes:
  """Build a RESP Array of Bulk Strings."""
  out = [f"*{len(parts)}\r\n".encode()]
  for p in parts:
    out.append(f"${len(p)}\r\n".encode())
    out.append(p.encode())
    out.append(b"\r\n")
  return b"".join(out)


class RespReader:
  def __init__(self, sock: socket.socket, bufsize: int = READ_BUF_SIZE) -> None:
    self.sock = sock
    self.bufsize = bufsize
    self.buffer = bytearray()

  def _fill(self, n: int) -> None:
    while len(self.buffer) < n:
      chunk = self.sock.recv(self.bufsize)
      if not chunk:
        raise ConnectionError("socket closed while reading")
      self.buffer.extend(chunk)

  def _read_exact(self, n: int) -> bytes:
    self._fill(n)
    data = bytes(self.buffer[:n])
    del self.buffer[:n]
    return data

  def _read_line(self) -> bytes:
    while True:
      idx = self.buffer.find(b"\r\n")
      if idx != -1:
        line = bytes(self.buffer[:idx])
        del self.buffer[:idx + 2]
        return line
      chunk = self.sock.recv(self.bufsize)
      if not chunk:
        raise ConnectionError("socket closed while reading line")
      self.buffer.extend(chunk)

  def read_resp(self):
    """Minimal RESP reader for simple/bulk/int responses."""
    prefix = self._read_exact(1)
    if prefix == b"+" or prefix == b"-":
      line = self._read_line()
      return line.decode()
    if prefix == b":":
      line = self._read_line()
      return int(line)
    if prefix == b"$":
      length_line = self._read_line()
      length = int(length_line)
      if length == -1:
        return None
      data = self._read_exact(length)
      self._read_exact(2)  # trailing CRLF
      return data
    if prefix == b"*":
      # Simple array handling for future expansion; not heavily used here.
      length_line = self._read_line()
      length = int(length_line)
      if length == -1:
        return None
      return [self.read_resp() for _ in range(length)]

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
  try:
    sock = socket.create_connection((HOST, PORT))
  except OSError:
    result.errors += 1
    return
  reader = RespReader(sock)
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
        reader.read_resp()
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

  if not check_server():
    return

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
  write_latencies(latencies)

  print(f"Server: {HOST}:{PORT}")
  print(f"Completed ops: {all_ops}, errors: {all_errors}, elapsed: {elapsed:.3f}s")
  if elapsed > 0:
    print(f"Throughput: {all_ops / elapsed:.1f} ops/sec")
  if latencies:
    for label, val in [("p50", pct(latencies, 50)), ("p95", pct(latencies, 95)), ("p99", pct(latencies, 99))]:
      if val is not None:
        print(f"{label}: {val:.2f} ms")

def check_server() -> bool:
  try:
    with socket.create_connection((HOST, PORT), timeout=2):
      return True
  except OSError as exc:
    print(f"Cannot connect to server at {HOST}:{PORT}: {exc}")
    return False

def write_latencies(latencies: List[float]) -> None:
  root = Path(__file__).resolve().parent.parent
  data_dir = root / DATA_DIR_NAME
  data_dir.mkdir(parents=True, exist_ok=True)
  stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
  path = data_dir / f"latencies-{stamp}.txt"
  with path.open("w", encoding="utf-8") as f:
    for value in latencies:
      f.write(f"{value:.6f}\n")


if __name__ == "__main__":
  random.seed(42)
  run_load(None)
