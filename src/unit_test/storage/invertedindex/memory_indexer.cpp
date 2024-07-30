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

#include "unit_test/base_test.h"

#include <iostream>
#include <unistd.h>
import stl;
import analyzer;
import analyzer_pool;

import index_defines;
import posting_list_format;
import column_vector;
import data_type;
import value;
import memory_indexer;
import column_index_reader;
import posting_iterator;
import column_inverter;
import segment_index_entry;
import local_file_system;
import internal_types;
import logical_type;
import column_index_merger;
import third_party;
import inmem_posting_decoder;
import inmem_position_list_decoder;
import inmem_index_segment_reader;
import segment_posting;
import global_resource_usage;
import infinity_context;
import third_party;

using namespace infinity;

class MemoryIndexerTest : public BaseTestParamStr {
public:
    struct ExpectedPosting {
        String term;
        Vector<RowID> doc_ids;
        Vector<u32> tfs;
    };

protected:
    ThreadPool inverting_thread_pool_{4};
    ThreadPool commiting_thread_pool_{4};
    optionflag_t flag_{OPTION_FLAG_ALL};
    SharedPtr<ColumnVector> column_;
    Vector<ExpectedPosting> expected_postings_;
    String config_path_{};

public:
    void SetUp() override {
        CleanupDbDirs();
        config_path_ = GetParam();
        if (config_path_ != BaseTestParamStr::NULL_CONFIG_PATH) {
            std::shared_ptr<std::string> config_path = std::make_shared<std::string>(config_path_);
            infinity::InfinityContext::instance().Init(config_path);
        }

        // https://en.wikipedia.org/wiki/Finite-state_transducer
        const char *paragraphs[] = {
            R"#(A finite-state transducer (FST) is a finite-state machine with two memory tapes, following the terminology for Turing machines: an input tape and an output tape. This contrasts with an ordinary finite-state automaton, which has a single tape. An FST is a type of finite-state automaton (FSA) that maps between two sets of symbols.[1] An FST is more general than an FSA. An FSA defines a formal language by defining a set of accepted strings, while an FST defines a relation between sets of strings.)#",
            R"#(An FST will read a set of strings on the input tape and generates a set of relations on the output tape. An FST can be thought of as a translator or relater between strings in a set.)#",
            R"#(In morphological parsing, an example would be inputting a string of letters into the FST, the FST would then output a string of morphemes.)#",
            R"#(An automaton can be said to recognize a string if we view the content of its tape as input. In other words, the automaton computes a function that maps strings into the set {0,1}. Alternatively, we can say that an automaton generates strings, which means viewing its tape as an output tape. On this view, the automaton generates a formal language, which is a set of strings. The two views of automata are equivalent: the function that the automaton computes is precisely the indicator function of the set of strings it generates. The class of languages generated by finite automata is known as the class of regular languages.)#",
            R"#(The two tapes of a transducer are typically viewed as an input tape and an output tape. On this view, a transducer is said to transduce (i.e., translate) the contents of its input tape to its output tape, by accepting a string on its input tape and generating another string on its output tape. It may do so nondeterministically and it may produce more than one output for each input string. A transducer may also produce no output for a given input string, in which case it is said to reject the input. In general, a transducer computes a relation between two formal languages.)#",
        };

        const SizeT num_paragraph = sizeof(paragraphs) / sizeof(char *);
        column_ = ColumnVector::Make(MakeShared<DataType>(LogicalType::kVarchar));
        column_->Initialize();
        for (SizeT i = 0; i < num_paragraph; ++i) {
            Value v = Value::MakeVarchar(String(paragraphs[i]));
            column_->AppendValue(v);
        }
        expected_postings_ = {{"fst", {0, 1, 2}, {4, 2, 2}}, {"automaton", {0, 3}, {2, 5}}, {"transducer", {0, 4}, {1, 4}}};
    }

    void TearDown() override {
        if (config_path_ != BaseTestParamStr::NULL_CONFIG_PATH) {
            infinity::InfinityContext::instance().UnInit();
        }
    }

    void Check(ColumnIndexReader &reader) {
        for (SizeT i = 0; i < expected_postings_.size(); ++i) {
            const ExpectedPosting &expected = expected_postings_[i];
            const String &term = expected.term;

            UniquePtr<PostingIterator> post_iter(reader.Lookup(term));
            ASSERT_TRUE(post_iter != nullptr);

            RowID doc_id = INVALID_ROWID;
            for (SizeT j = 0; j < expected.doc_ids.size(); ++j) {
                doc_id = post_iter->SeekDoc(expected.doc_ids[j]);
                ASSERT_EQ(doc_id, expected.doc_ids[j]);
                u32 tf = post_iter->GetCurrentTF();
                ASSERT_EQ(tf, expected.tfs[j]);
                pos_t cur_pos = 0;
                pos_t res_pos = INVALID_POSITION;
                do {
                    post_iter->SeekPosition(cur_pos, res_pos);
                    cur_pos = res_pos + 1;
                } while (res_pos != INVALID_POSITION);
            }
            if (doc_id != INVALID_ROWID) {
                doc_id = post_iter->SeekDoc(doc_id + 1);
                ASSERT_EQ(doc_id, INVALID_ROWID);
            }
        }
    }
};

INSTANTIATE_TEST_SUITE_P(
    TestWithDifferentParams,
    MemoryIndexerTest,
    ::testing::Values(
        BaseTestParamStr::NULL_CONFIG_PATH,
        BaseTestParamStr::CONFIG_PATH
    )
);

TEST_P(MemoryIndexerTest, Insert) {
    // prepare fake segment index entry
    auto fake_segment_index_entry_1 = SegmentIndexEntry::CreateFakeEntry(GetFullDataDir());
    MemoryIndexer indexer1(GetFullDataDir(), "chunk1", RowID(0U, 0U), flag_, "standard");
    indexer1.Insert(column_, 0, 1);
    indexer1.Insert(column_, 1, 3);
    indexer1.Dump();

    auto indexer2 = MakeUnique<MemoryIndexer>(GetFullDataDir(), "chunk2", RowID(0U, 4U), flag_, "standard");
    indexer2->Insert(column_, 4, 1);
    while (indexer2->GetInflightTasks() > 0) {
        sleep(1);
        indexer2->CommitSync();
    }

    fake_segment_index_entry_1->AddFtChunkIndexEntry("chunk1", RowID(0U, 0U).ToUint64(), 4U);
    fake_segment_index_entry_1->SetMemoryIndexer(std::move(indexer2));
    Map<SegmentID, SharedPtr<SegmentIndexEntry>> index_by_segment = {{0, fake_segment_index_entry_1}};
    ColumnIndexReader reader;
    reader.Open(flag_, GetFullDataDir(), std::move(index_by_segment));
    Check(reader);
}

TEST_P(MemoryIndexerTest, test2) {
    auto fake_segment_index_entry_1 = SegmentIndexEntry::CreateFakeEntry(GetFullDataDir());
    MemoryIndexer indexer1(GetFullDataDir(), "chunk1", RowID(0U, 0U), flag_, "standard");
    indexer1.Insert(column_, 0, 2, true);
    indexer1.Insert(column_, 2, 2, true);
    indexer1.Insert(column_, 4, 1, true);
    indexer1.Dump(true);
    fake_segment_index_entry_1->AddFtChunkIndexEntry("chunk1", RowID(0U, 0U).ToUint64(), 5U);

    Map<SegmentID, SharedPtr<SegmentIndexEntry>> index_by_segment = {{1, fake_segment_index_entry_1}};

    ColumnIndexReader reader;
    reader.Open(flag_, GetFullDataDir(), std::move(index_by_segment));
    Check(reader);
}

TEST_P(MemoryIndexerTest, SpillLoadTest) {
    auto fake_segment_index_entry_1 = SegmentIndexEntry::CreateFakeEntry(GetFullDataDir());
    auto indexer1 = MakeUnique<MemoryIndexer>(GetFullDataDir(), "chunk1", RowID(0U, 0U), flag_, "standard");
    indexer1->Insert(column_, 0, 2);
    indexer1->Insert(column_, 2, 2);
    indexer1->Insert(column_, 4, 1);
    while (indexer1->GetInflightTasks() > 0) {
        sleep(1);
        indexer1->CommitSync();
    }

    indexer1->Dump(false, true);
    UniquePtr<MemoryIndexer> loaded_indexer = MakeUnique<MemoryIndexer>(GetFullDataDir(), "chunk1", RowID(0U, 0U), flag_, "standard");

    loaded_indexer->Load();
    SharedPtr<InMemIndexSegmentReader> segment_reader = MakeShared<InMemIndexSegmentReader>(loaded_indexer.get());
    for (SizeT i = 0; i < expected_postings_.size(); ++i) {
        const ExpectedPosting &expected = expected_postings_[i];
        const String &term = expected.term;
        SegmentPosting seg_posting;
        SharedPtr<Vector<SegmentPosting>> seg_postings = MakeShared<Vector<SegmentPosting>>();

        auto ret = segment_reader->GetSegmentPosting(term, seg_posting);
        if (ret) {
            seg_postings->push_back(seg_posting);
        }

        auto posting_iter = MakeUnique<PostingIterator>(flag_);
        u32 state_pool_size = 0;
        posting_iter->Init(seg_postings, state_pool_size);
        RowID doc_id = INVALID_ROWID;
        for (SizeT j = 0; j < expected.doc_ids.size(); ++j) {
            doc_id = posting_iter->SeekDoc(expected.doc_ids[j]);
            ASSERT_EQ(doc_id, expected.doc_ids[j]);
            u32 tf = posting_iter->GetCurrentTF();
            ASSERT_EQ(tf, expected.tfs[j]);
        }
    }
}

TEST_P(MemoryIndexerTest, SeekPosition) {
    // "A B C" repeats 7 times
    String paragraph(R"#(A B C A B C A B C A B C A B C A B C A B C)#");
    auto column = ColumnVector::Make(MakeShared<DataType>(LogicalType::kVarchar));
    column->Initialize();
    Value v = Value::MakeVarchar(paragraph);
    for (SizeT i = 0; i < 8192; i++) {
        column->AppendValue(v);
    }

    auto fake_segment_index_entry_1 = SegmentIndexEntry::CreateFakeEntry(GetFullDataDir());
    MemoryIndexer indexer1(GetFullDataDir(), "chunk1", RowID(0U, 0U), flag_, "standard");
    indexer1.Insert(column, 0, 8192);
    while (indexer1.GetInflightTasks() > 0) {
        sleep(1);
        indexer1.CommitSync();
    }

    SharedPtr<InMemIndexSegmentReader> segment_reader = MakeShared<InMemIndexSegmentReader>(&indexer1);
    const String term("a");
    SegmentPosting seg_posting;
    SharedPtr<Vector<SegmentPosting>> seg_postings = MakeShared<Vector<SegmentPosting>>();
    auto ret = segment_reader->GetSegmentPosting(term, seg_posting);
    if (ret) {
        seg_postings->push_back(seg_posting);
    }

    auto posting_iter = MakeUnique<PostingIterator>(flag_);
    u32 state_pool_size = 0;
    posting_iter->Init(seg_postings, state_pool_size);
    RowID doc_id = INVALID_ROWID;
    Vector<SizeT> doc_ids = {0, 1, 2, 5, 127, 128, 512, 1024, 2048, 4096, 8191};
    for (SizeT i = 0; i < doc_ids.size(); ++i) {
        doc_id = RowID::FromUint64(doc_ids[i]);
        doc_id = posting_iter->SeekDoc(doc_id);
        ASSERT_EQ(doc_id, doc_ids[i]);
        u32 tf = posting_iter->GetCurrentTF();
        ASSERT_EQ(tf, 7);
        pos_t target_pos = 0;
        pos_t act_pos = 0;
        for (SizeT j = 0; j < 7; ++j) {
            posting_iter->SeekPosition(target_pos, act_pos);
            ASSERT_EQ(act_pos, 3 * j);
            target_pos = act_pos + 1;
        }
        posting_iter->SeekPosition(act_pos + 1, act_pos);
        ASSERT_EQ(act_pos, INVALID_POSITION);
    }
}
