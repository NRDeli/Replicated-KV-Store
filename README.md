# ReplicatedKVStore — Raft-Style Fault-Tolerant Distributed Key-Value Store  
**Hybrid C++ + Rust Storage Engine**

ReplicatedKVStore is a fault-tolerant distributed key-value datastore implementing Raft-style leader-based replication with a hybrid architecture:

- **C++ layer:** consensus, replication, networking (gRPC)
- **Rust layer:** write-ahead log (WAL), snapshots, compaction, crash recovery

The system demonstrates production-grade distributed storage mechanics including quorum commit, durable WAL replication, snapshotting, streaming state transfer, and follower log repair.

---

# ⚙️ Features

- Leader election with randomized timeouts
- Majority quorum commit (N/2 + 1)
- Per-follower `nextIndex` / `matchIndex`
- WAL-backed durable replication
- Crash recovery via WAL + snapshot replay
- Log backtracking for divergence repair
- Snapshot creation + log compaction
- Streaming InstallSnapshot RPC
- Follower catch-up via snapshot transfer
- Rust memory-safe storage engine
- gRPC inter-node communication
- HTTP metrics endpoint
- Multi-node cluster simulation

---

# 🏯 Architecture Overview

ReplicatedKVStore follows a hybrid architecture where Raft consensus and networking
are implemented in C++, while durability and compaction are handled by a Rust WAL engine
exposed via a C FFI boundary.

## Figure 1 — Hybrid C++ / Rust Architecture
![ High-Level System Architecture](/media/architecture.png)

Client → Leader → Rust WAL → Replication → Followers → Apply to KV Store

Each node consists of:

- **Node (C++)** — Raft coordination & state machine control  
- **KVStore (C++)** — in-memory state machine  
- **WALAdapter (C++)** — FFI bridge to Rust WAL  
- **Rust WAL Engine** — durable log + snapshots  
- **ReplicationManager (C++)** — gRPC replication client  
- **gRPC Services** — KV / Replication / Election  
- **Metrics Server** — HTTP observability  

All cluster communication uses gRPC.  
Metrics are exposed via HTTP on `(port + 1000)`.

---

## Storage Layer (Rust WAL)

The storage engine is implemented in Rust and exposed to C++ via a stable C ABI.

Responsibilities:

- Append-only log persistence
- Crash-atomic entry writes
- Snapshot creation & loading
- Log truncation / compaction
- Sequential replay
- Snapshot-aware recovery

Each WAL entry stores:
(index, term, key_len, value_len, key_bytes, value_bytes)

Durability guarantees:

- Append is atomic
- Partial entries detected & truncated
- Replay deterministic after crash
- Snapshot replaces log prefix

The Rust WAL is the authoritative persistent state.

# 🧱 Node Responsibilities

The C++ Node implements Raft-style coordination:

- Role management (Follower / Candidate / Leader)
- Election timeouts & voting
- WAL append via Rust adapter
- Per-follower replication
- Majority commit advancement
- State machine application
- Snapshot triggering
- Recovery orchestration
- Metrics exposure

Persistent state:

- WAL entries (Rust)
- Snapshot (Rust)

Volatile state:

- `current_term`
- `voted_for`
- `commitIndex`
- `lastApplied`
- `nextIndex[]`
- `matchIndex[]`
- `role`

---

# 🔁 Replication Protocol

Leader-driven log replication.

On client write:

1. Leader receives Put
2. Append entry to Rust WAL
3. Replicate entry to followers
4. Update `matchIndex`
5. Backtrack on mismatch
6. Advance `commitIndex` via quorum
7. Apply committed entry to KVStore

Replication is synchronous per write.
## Figure 2 — Replication & Majority Commit Flow
![Replication & Majority Commit Flow](/media/replication-and-majority-commit-flow.png)

---

# 🧮 Majority Commit Rule

Leader advances commitIndex when:
```
count(matchIndex ≥ N) > cluster_size / 2
```
Search from highest index downward:
```
for N = last_index → commit_index:
    if quorum(matchIndex ≥ N):
        commit_index = N
```

Invariants:
- `commitIndex ≤ lastIndex`
- Entries applied only after commit
- Majority quorum ensures safety

---

# 🔄 Log Backtracking

If follower replication fails:

- Leader decrements `nextIndex[f]`
- Retries replication
- Rust WAL truncates conflicting suffix
- Follower log converges

This repairs divergence and guarantees eventual convergence.

---

# 💾 Snapshot & Log Compaction

Snapshots are created from committed state:
```
snapshot = serialize(KVStore at commitIndex)
```
Rust WAL:

- Persists snapshot file
- Records snapshot index
- Truncates log below snapshot
- Rewrites remaining entries

Followers behind snapshot receive snapshot via streaming RPC.

---

# 📡 Streaming InstallSnapshot

Large snapshots are transferred in chunks:

Leader → stream InstallSnapshotChunk → Follower

Each chunk contains:

- data bytes
- last_index
- last_term
- done flag

Follower:

1. Reassembles snapshot
2. Replaces KV state
3. Compacts WAL
4. Updates indices

This enables fast follower catch-up.

---

# 🔄 Recovery Model

On startup:

1. Load snapshot (Rust)
2. Restore KVStore
3. Replay WAL entries after snapshot
4. Restore lastIndex
5. Set commitIndex
6. Set lastApplied

Guarantees:

- Committed writes survive crash
- Snapshot + WAL define state
- Deterministic reconstruction

---

# 🔐 Safety Invariants

The system enforces:

1. At most one leader per term  
2. Entry committed only after majority replication  
3. Committed entries applied in order  
4. No uncommitted entry applied  
5. commitIndex ≤ lastIndex  
6. WAL append-only (except compaction)  
7. Snapshot replaces only committed prefix  

These ensure deterministic replicated state machine behavior.

---

# ⚡ Failure Handling

Leader crash:

- Followers timeout
- New election
- New leader continues from WAL

Follower crash:

- WAL + snapshot recovery
- Leader backtracks or sends snapshot

Divergence:

- nextIndex decrement
- WAL truncate
- Re-replicate

Lagging follower:

- Snapshot transfer
- Resume replication

---

# 📊 Metrics

Exposed via HTTP:
- raft_role
- raft_term
- raft_commit_index
- raft_last_applied
- raft_log_size
- raft_elections_total
- raft_replication_failures_total

Example:
```
curl localhost:51051
```

Role:

- 0 = FOLLOWER
- 1 = CANDIDATE
- 2 = LEADER

---

# ⚠️ Raft Simplifications

This is Raft-style but not full Raft.

Implemented:

- Leader election
- Majority commit
- Log backtracking
- Snapshot + InstallSnapshot
- Durable WAL
- Follower catch-up

Not implemented:

- Persistent term/vote
- prevLogTerm validation
- Membership changes
- Linearizable reads
- Network partition tests

Focus: core replicated storage mechanics.

---

# 🧪 Running a Cluster

## Build
```
rm -rf build
cmake -S . -B build
cmake --build build -j
```
## Start 3 nodes
```
./build/server 50051
./build/server 50052
./build/server 50053
```
## Check leader
```
curl localhost:51051
curl localhost:51052
curl localhost:51053
```
Exactly one:
```
raft_role 2
```
---

# 📚 Distributed Systems Concepts

- Replicated state machines
- Leader election
- Majority quorum commit
- WAL durability
- Snapshotting & compaction
- Streaming state transfer
- Log repair
- Crash recovery
- Consensus replication
- gRPC distributed communication

---

# 🎯 Resume Value

ReplicatedKVStore demonstrates:

- Raft-style consensus implementation
- Hybrid C++/Rust systems integration
- Durable WAL & snapshot engine
- Distributed replication protocols
- Follower repair & compaction
- Crash-safe storage design
- gRPC distributed architecture
- Observability & metrics
- Systems-level debugging

---
# Known Limitations

- Term/vote not persisted
- Partial Raft conflict logic
- No membership changes
- No partition tolerance testing
- No read linearizability

The project targets storage-layer correctness and replication mechanics rather than full Raft compliance.
