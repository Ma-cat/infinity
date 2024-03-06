// Copyright(C) 2023 InfiniFlow, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

module;

export module db_meta;

import stl;

import buffer_manager;
import third_party;
import status;
import extra_ddl_info;
import db_entry;
import base_entry;
import txn_manager;

import entry_list;

import meta_entry_interface;
import cleanup_scanner;

namespace infinity {

struct Catalog;
class AddDBMetaOp;

export struct DBMeta : public MetaInterface {
    using EntryT = DBEntry;

    friend struct Catalog;

public:
    using MetaOp = AddDBMetaOp;

public:
    explicit DBMeta(const SharedPtr<String> &data_dir, SharedPtr<String> db_name) : db_name_(std::move(db_name)), data_dir_(data_dir) {}

    static UniquePtr<DBMeta> NewDBMeta(const SharedPtr<String> &data_dir, const SharedPtr<String> &db_name);

    SharedPtr<String> ToString();

    nlohmann::json Serialize(TxnTimeStamp max_commit_ts, bool is_full_checkpoint);

    static UniquePtr<DBMeta> Deserialize(const nlohmann::json &db_meta_json, BufferManager *buffer_mgr);

    SharedPtr<String> db_name() const { return db_name_; }

    SharedPtr<String> data_dir() const { return data_dir_; }

private:
    Tuple<DBEntry *, Status> CreateNewEntry(std::shared_lock<std::shared_mutex> &&r_lock,
                                            TransactionID txn_id,
                                            TxnTimeStamp begin_ts,
                                            TxnManager *txn_mgr,
                                            ConflictType conflict_type = ConflictType::kError);

    Tuple<DBEntry *, Status> DropNewEntry(std::shared_lock<std::shared_mutex> &&r_lock,
                                          TransactionID txn_id,
                                          TxnTimeStamp begin_ts,
                                          TxnManager *txn_mgr,
                                          ConflictType conflict_type = ConflictType::kError);

    void DeleteNewEntry(TransactionID txn_id);

    Tuple<DBEntry *, Status> GetEntry(std::shared_lock<std::shared_mutex> &&r_lock, TransactionID txn_id, TxnTimeStamp begin_ts) {
        return db_entry_list_.GetEntry(std::move(r_lock), txn_id, begin_ts);
    }

    Tuple<DBEntry *, Status> GetEntryNolock(TransactionID txn_id, TxnTimeStamp begin_ts) { return db_entry_list_.GetEntryNolock(txn_id, begin_ts); }

    Tuple<DBEntry *, Status> GetEntryReplay(TransactionID txn_id, TxnTimeStamp begin_ts) { return db_entry_list_.GetEntryReplay(txn_id, begin_ts); }

private:
    SharedPtr<String> db_name_{};
    SharedPtr<String> data_dir_{};

public:
    void Cleanup() override;

    bool PickCleanup(CleanupScanner *scanner) override;

    bool Empty() override { return db_entry_list_.Empty(); }

private:
    EntryList<DBEntry> db_entry_list_{};

    // TODO: remove
    std::shared_mutex &rw_locker() { return db_entry_list_.rw_locker_; }

public:
    // TODO: remove
    List<SharedPtr<DBEntry>> &db_entry_list() { return db_entry_list_.entry_list_; }
};

} // namespace infinity
