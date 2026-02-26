# ReplicatedKVStore — Raft-Inspired Fault-Tolerant Distributed Key-Value Store

ReplicatedKVStore is a fault-tolerant distributed key-value datastore implemented in C++ using gRPC.
It demonstrates core distributed systems principles including leader election, majority quorum commit, log-based state machine replication, write-ahead logging (WAL) durability, crash recovery, and log backtracking.

The system follows a Raft-inspired consensus design (simplified implementation) and operates as a replicated state machine.
---

# ⚙️ Features

- Leader election with randomized timeouts
- Majority quorum commit (N/2 + 1)
- Per-follower nextIndex tracking
- Per-follower matchIndex tracking
- Log backtracking for follower repair
- Write-Ahead Log (WAL) durability
- Crash recovery via WAL replay
- Separation of commitIndex and lastApplied
- gRPC-based inter-node communication
- HTTP metrics endpoint for observability
- Real multi-node cluster simulation

This implementation focuses on core consensus mechanics while intentionally simplifying certain advanced Raft features.
---

# 🏯 Architecture Overview

ReplicatedKVStore follows a replicated state machine architecture.

Client → Leader → WAL Append → Replication → Followers → Apply to KV Store

Each node consists of:
- Node — Raft coordination layer
- KVStore — In-memory state machine
- WriteAheadLog (WAL) — Durability layer
- ReplicationManager — gRPC replication client
- ElectionService — Vote RPC handling
- KVService — Client Put/Get RPC
- Metrics Server — HTTP observability endpoint

All cluster communication uses gRPC.
Metrics are exposed separately via HTTP (port + 1000).

---

## Figure 1: High-Level System Architecture
![ High-Level System Architecture](/media/architecture.png)



# 🧱 Components

## Node
Core coordination engine implementing Raft-inspired logic.

Responsibilities:
- Manage node role (Follower / Candidate / Leader)
- Handle election timeouts
- Send and process vote RPCs
- Append operations to WAL
- Replicate log entries to followers
- Track nextIndex[] and matchIndex[]
- Advance commitIndex via quorum
- Apply committed entries to state machine
- Recover state on startup
- Expose internal metrics

---
## Node State

Each node maintains the following core state:

Persistent:
- Log entries (index, term, key, value)

Volatile:
- current_term
- voted_for
- commitIndex
- lastApplied
- nextIndex[] per follower
- matchIndex[] per follower
- role (Follower / Candidate / Leader)

These variables enforce replication safety and quorum commit correctness. Note: current_term and voted_for are currently volatile and reset on restart.

---

## KVStore
In‑memory key‑value database.

- HashMap‑based storage
- O(1) reads and writes
- Serves as the replicated state machine backend
State changes occur only after entries are committed.

---

## WriteAheadLog (WAL)
Durability layer.

- Append-only file
- Stores serialized log entries (index, term, key, value)
- Ensures writes are persisted before replication
- Replayed on crash recovery

Durability model:
- Append before replication
- Deterministic replay on restart

---

## ReplicationManager
Leader–Follower replication logic.

Leader:
- Sends replication RPCs to followers
- Tracks nextIndex per follower
- Tracks matchIndex per follower
- Retries replication on mismatch
- Supports basic log backtracking

Follower:
- Appends entries to WAL
- Updates local log index
- Applies entries only after commit notification

Consistency model: **Leader-based majority quorum replication**

---

## gRPC Services

### 1. KV Service
Client-facing API:
- Put(key, value)
- Get(key)

Writes are accepted only by leader.

### 2. Replication Service
Leader → Follower replication RPC.
Used to:
- Ship log entries
- Advance follower state
- Propagate commit index

### 3. Election Service
Implements RequestVote RPC.

Used during election phase:
- Candidate requests votes
- Followers grant or deny votes
- Majority determines leader

Election Safety:

- At most one leader can be elected per term due to majority quorum voting.
- Term numbers strictly increase across elections.
- A candidate must receive > N/2 votes to transition to LEADER.
- Followers reject stale-term vote requests.

### Figure 2: Leader Election Sequence
![Leader Election Sequence](/media/leader-election-sequence.png)

### 4. Metrics Server
Lightweight HTTP server implemented via POSIX sockets.

Exposes internal Raft state:
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

Role mapping:
- 0 = FOLLOWER
- 1 = CANDIDATE
- 2 = LEADER

---

# 🔁 Replication Protocol

Leader‑driven log replication.

On client write:
1. Leader receives Put request
2. Append entry to WAL
3. Send replication RPC to each follower
4. Update matchIndex on success
5. Decrement nextIndex on failure (backtracking)
6. Advance commitIndex when quorum reached
7. Apply committed entries to KVStore

Replication is synchronous per write.

## Figure 3 — Replication & Majority Commit
![Replication & Majority Commit](/media/replication-and-majority-commit-flow.png)

---

# 🧮 Majority Commit Algorithm

The leader advances commitIndex when:
Number of nodes with matchIndex >= N > cluster_size / 2

Commit Rule:
```
For N from last_index down to commit_index:
    if count(matchIndex >= N) > cluster_size/2:
        commit_index = N
```
Key invariants:
- commitIndex ≤ highest replicated index
- Entries applied only after commit
- Majority quorum ensures safety

Separation of:
- commitIndex (replicated)
- lastApplied (state machine progress)

ensures correct ordering and crash safety.

---

# 🔄 Log Backtracking

If replication to a follower fails:
- Decrement that follower’s nextIndex
- Retry replication with earlier log entry

This ensures:
- Lagging followers converge
- Log divergence is repaired
- Eventual consistency within cluster

This is a simplified variant of Raft log repair.

# 💾 Persistence Model

All writes are persisted before replication.

Crash recovery process:
1. Read WAL file
2. Reconstruct in-memory log
3. Replay operations sequentially
4. Restore KV state
5. Restore commitIndex and lastApplied

Guarantees:
- Committed writes survive crashes
- Deterministic state reconstruction
- Durable majority replication

# 🦾 Consistency Model

ReplicatedKVStore provides:

- Leader-based majority quorum writes
- Ordered log replication
- Deterministic state machine application
- Single-leader write authority

Under normal operation (no partitions), committed writes are consistent across the cluster.

This implementation approximates Raft-style safety but does not implement full linearizability guarantees under all failure scenarios.


# 🔐 Safety Invariants

The system enforces the following invariants:

1. At most one leader per term.
2. An entry is committed only if replicated on majority.
3. Committed entries are applied in order.
4. No uncommitted entry is applied to the state machine.
5. commitIndex ≤ last replicated index.
6. Log entries are append-only.

These invariants ensure deterministic and safe state machine replication.

# ⚡ Failure Handling

Leader Crash:
- Followers detect election timeout.
- New election is triggered.
- New leader elected via majority vote.

Follower Crash:
- Leader continues replication.
- Upon restart, follower replays WAL.
- nextIndex backtracking repairs divergence.

Crash During Replication:
- Only majority-replicated entries are committed.
- Uncommitted entries are not applied.

# ⚠️ Simplifications (Raft-inspired model)

This implementation is intentionally simplified.

Partially Implemented:
- Log conflict resolution without full prevLogIndex/prevLogTerm validation
- Simplified heartbeat logic
- Minimal HTTP metrics server (not full Prometheus exporter)

Not Implemented:
- Snapshotting / log compaction
- InstallSnapshot RPC
- Persistent current_term and voted_for
- Dynamic cluster membership
- Full prevLogTerm conflict overwrite logic
- Network partition simulation

The project focuses on core consensus mechanics rather than full production Raft compliance.

# 🧪 Running a Cluster

## Build 
```
rm -rf build
mkdir build
cd build
cmake ..
make
```

## Start 3 Nodes
Terminal 1:
```
./build/server 50051
```
Terminal 2:
```
./build/server 50052
```
Terminal 3:
```
./build/server 50053
```

## Check Leader
```
curl localhost:51051
curl localhost:51052
curl localhost:51053
```

Exactly one node should report 
```
raft_role 2
```
---

## Test Failover
1. Identify leader
2. Kill leader process
3. Wait 2 seconds
4. Check metrics again
A new leader will be elected automatically.

# 📚 Distributed Systems Concepts Demonstrated
- Replicated state machines
- Leader election
- Majority quorum commit
- Log replication
- Log backtracking
- Write-ahead logging
- Crash recovery
- RPC-based distributed communication
- Failure detection via timeout
- Observability integration

# 🎯 Resume Value
ReplicatedKVStore demonstrates:
- Distributed systems architecture design
- Implementation of Raft-inspired consensus mechanics
- Majority quorum commit logic
- Fault tolerance and crash recovery
- Write-ahead logging durability
- Concurrency and atomic state management
- gRPC-based inter-node communication
- Systems-level debugging and observability design

# Known Limitations

This implementation does not guarantee safety under network partitions
due to lack of persistent term/vote storage and full conflict resolution.