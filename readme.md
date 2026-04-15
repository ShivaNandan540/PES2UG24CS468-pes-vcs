# PES Version Control System (PES-VCS)

## 📌 Overview
This project implements a simplified version control system similar to Git.  
It supports object storage, tree structures, staging (index), and commit history.

---

## ⚙️ Features Implemented

### 🔹 Phase 1: Object Storage
- Blob creation and storage
- SHA-256 hashing using OpenSSL
- Deduplication of objects
- Stored in `.pes/objects/`

---

### 🔹 Phase 2: Tree Objects
- Directory structure representation
- Tree serialization and parsing
- Deterministic tree creation

---

### 🔹 Phase 3: Index (Staging Area)
- Tracks staged files
- Functions:
  - `index_load`
  - `index_save`
  - `index_add`
  - `index_status`
- Stored in `.pes/index`

---

### 🔹 Phase 4: Commits
- Snapshot of staged files
- Parent-child commit linking
- Author + timestamp tracking
- HEAD reference updates

---

## 🚀 Commands Supported

```bash
./pes init
./pes add <file>
./pes status
./pes commit -m "message"
