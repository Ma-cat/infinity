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

#include <vector>
module and_iterator;

import stl;
import doc_iterator;

namespace infinity {

AndIterator::AndIterator(Vector<UniquePtr<DocIterator>> iterators) {
    children_ = std::move(iterators);
    sorted_iterators_.reserve(children_.size());
    for (u32 i = 0; i < children_.size(); ++i) {
        sorted_iterators_.push_back(children_[i].get());
    }
    std::sort(sorted_iterators_.begin(), sorted_iterators_.end(), [](const auto lhs, const auto rhs) { return lhs->GetDF() < rhs->GetDF(); });
    // initialize doc_id_ to first doc
    DoSeek(0);
}

AndIterator::~AndIterator() {}

void AndIterator::DoSeek(docid_t doc_id) {
    auto ib = sorted_iterators_.begin(), ie = sorted_iterators_.end();
    while (ib != ie) {
        (*ib)->Seek(doc_id);
        if (docid_t doc = (*ib)->Doc(); doc != doc_id) {
            // not match, restart from the first iterator, since first iterator has fewer docs
            doc_id = doc;
            ib = sorted_iterators_.begin();
        } else {
            ++ib;
        }
    }
    doc_id_ = doc_id;
}

u32 AndIterator::GetDF() const {
    u32 min_df = std::numeric_limits<u32>::max();
    for (u32 i = 0; i < children_.size(); ++i) {
        min_df = std::min(min_df, children_[i]->GetDF());
    }
    return min_df;
}
} // namespace infinity
