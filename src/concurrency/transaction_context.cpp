//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// transaction_context.cpp
//
// Identification: src/concurrency/transaction_context.cpp
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "concurrency/transaction_context.h"

#include <sstream>

#include "common/logger.h"
#include "common/platform.h"
#include "common/macros.h"
#include "trigger/trigger.h"

#include <chrono>
#include <thread>
#include <iomanip>

namespace peloton {
namespace concurrency {

/*
 * TransactionContext state transition:
 *                r           r/ro            u/r/ro
 *              +--<--+     +---<--+        +---<--+
 *           r  |     |     |      |        |      |     d
 *  (init)-->-- +-> Read  --+-> Read Own ---+--> Update ---> Delete (final)
 *                    |   ro             u  |
 *                    |                     |
 *                    +----->--------->-----+
 *                              u
 *              r/ro/u
 *            +---<---+
 *         i  |       |     d
 *  (init)-->-+---> Insert ---> Ins_Del (final)
 *
 *    r : read
 *    ro: read_own
 *    u : update
 *    d : delete
 *    i : insert
 */

TransactionContext::TransactionContext(const size_t thread_id,
                         const IsolationLevelType isolation,
                         const cid_t &read_id) {
  Init(thread_id, isolation, read_id);
}

TransactionContext::TransactionContext(const size_t thread_id,
                         const IsolationLevelType isolation,
                         const cid_t &read_id, const cid_t &commit_id) {
  Init(thread_id, isolation, read_id, commit_id);
}

TransactionContext::~TransactionContext() {}

void TransactionContext::Init(const size_t thread_id,
                       const IsolationLevelType isolation, const cid_t &read_id,
                       const cid_t &commit_id) {
  read_id_ = read_id;

  // commit id can be set at a transaction's commit phase.
  commit_id_ = commit_id;

  // set txn_id to commit_id.
  txn_id_ = commit_id_;

  epoch_id_ = read_id_ >> 32;

  thread_id_ = thread_id;

  isolation_level_ = isolation;

  is_written_ = false;

  insert_count_ = 0;

  gc_set_.reset(new GCSet());
  gc_object_set_.reset(new GCObjectSet());

  on_commit_triggers_.reset();
}

RWType TransactionContext::GetRWType(const ItemPointer &location) {
  auto it = rw_set_.find(location);
  if (it == rw_set_.end()) {
    return RWType::INVALID;
  }
  return it->second;
}

void TransactionContext::RecordRead(const ItemPointer &location) {
  switch (GetRWType(location)) {
    case RWType::INVALID: {
      rw_set_[location] = RWType::READ;
      break;
    }
    case RWType::READ:
    case RWType::READ_OWN:
    case RWType::UPDATE:
    case RWType::INSERT: {
      break;
    }
    case RWType::DELETE:
    case RWType::INS_DEL: {
      PL_ASSERT(false && "Bad RWType");
    }
  }
}

void TransactionContext::RecordReadOwn(const ItemPointer &location) {
  switch (GetRWType(location)) {
    case RWType::INVALID:
    case RWType::READ: {
      rw_set_[location] = RWType::READ_OWN;
      break;
    }
    case RWType::READ_OWN:
    case RWType::UPDATE:
    case RWType::INSERT: {
      break;
    }
    case RWType::DELETE:
    case RWType::INS_DEL: {
      PL_ASSERT(false && "Bad RWType");
    }
  }
}

void TransactionContext::RecordUpdate(const ItemPointer &location) {
  switch (GetRWType(location)) {
    case RWType::INVALID: {
      rw_set_[location] = RWType::UPDATE;
      break;
    }
    case RWType::READ:
    case RWType::READ_OWN: {
      rw_set_[location] = RWType::UPDATE;
      is_written_ = true;
      break;
    }
    case RWType::UPDATE:
    case RWType::INSERT: {
      break;
    }
    case RWType::DELETE:
    case RWType::INS_DEL: {
      PL_ASSERT(false && "Bad RWType");
    }
  }
}

void TransactionContext::RecordInsert(const ItemPointer &location) {
  switch (GetRWType(location)) {
    case RWType::INVALID: {
      rw_set_[location] = RWType::INSERT;
      ++insert_count_;
      break;
    }
    default: {
      PL_ASSERT(false && "Bad RWType");
    }
  }
}

bool TransactionContext::RecordDelete(const ItemPointer &location) {
  switch (GetRWType(location)) {
    case RWType::INVALID: {
      rw_set_[location] = RWType::DELETE;
      return false;
    }
    case RWType::READ:
    case RWType::READ_OWN: {
      rw_set_[location] = RWType::DELETE;
      is_written_ = true;
      return false;
    }
    case RWType::UPDATE: {
      rw_set_[location] = RWType::DELETE;
      return false;
    }
    case RWType::INSERT: {
      rw_set_[location] = RWType::INS_DEL;
      --insert_count_;
      return true;
    }
    case RWType::DELETE:
    case RWType::INS_DEL: {
      PL_ASSERT(false && "Bad RWType");
      return false;
    }
  }
  return false;
}

const std::string TransactionContext::GetInfo() const {
  std::ostringstream os;

  os << " Txn :: @" << this << " ID : " << std::setw(4) << txn_id_
     << " Read ID : " << std::setw(4) << read_id_
     << " Commit ID : " << std::setw(4) << commit_id_
     << " Result : " << result_;

  return os.str();
}

void TransactionContext::AddOnCommitTrigger(trigger::TriggerData &trigger_data) {
  if (on_commit_triggers_ == nullptr) {
    on_commit_triggers_.reset(new trigger::TriggerSet());
  }
  on_commit_triggers_->push_back(trigger_data);
}

void TransactionContext::ExecOnCommitTriggers() {
  if (on_commit_triggers_ != nullptr) {
    on_commit_triggers_->ExecTriggers();
  }
}

}  // namespace concurrency
}  // namespace peloton
