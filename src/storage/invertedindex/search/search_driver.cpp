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

#include <cassert>
#include <iostream>
#include <sstream>
#include <utility>

#include "query_node.h"
#include "search_driver.h"
#include "search_parser.h"
#include "search_scanner.h"

import stl;
import term;
import infinity_exception;
import status;
import logger;
import third_party;

namespace infinity {

std::pair<std::string, float> ParseField(const std::string_view &field) {
    size_t cap_idx = field.find_first_of('^', 0);
    if (cap_idx == std::string::npos) {
        return std::make_pair(std::string(field), 1.0F);
    } else {
        std::string field_name(field.substr(0, cap_idx));
        std::string_view field_boost = field.substr(cap_idx + 1);
        float boost = std::stof(std::string(field_boost));
        return std::make_pair(std::move(field_name), boost);
    }
}

void ParseFields(const std::string &fields_str, std::vector<std::pair<std::string, float>> &fields) {
    fields.clear();
    if (fields_str.empty()) {
        return;
    }
    size_t begin_idx = 0;
    size_t len = fields_str.length();
    while (begin_idx < len) {
        size_t comma_idx = fields_str.find_first_of(',', begin_idx);
        if (comma_idx == std::string::npos) {
            auto field = ParseField(fields_str.substr(begin_idx));
            fields.emplace_back(std::move(field));
            break;
        } else {
            auto field = ParseField(fields_str.substr(begin_idx, comma_idx - begin_idx));
            fields.emplace_back(std::move(field));
            begin_idx = comma_idx + 1;
        }
    }
}

std::unique_ptr<QueryNode> SearchDriver::ParseSingleWithFields(const std::string &fields_str, const std::string &query) const {
    std::unique_ptr<QueryNode> parsed_query_tree;
    std::vector<std::pair<std::string, float>> fields;
    ParseFields(fields_str, fields);
    if (fields.empty()) {
        parsed_query_tree = ParseSingle(query);
    } else if (fields.size() == 1) {
        parsed_query_tree = ParseSingle(query, &(fields[0].first));
        if (parsed_query_tree) {
            parsed_query_tree->MultiplyWeight(fields[0].second);
        }
    } else {
        std::vector<std::unique_ptr<QueryNode>> or_children;
        for (auto &[default_field, boost] : fields) {
            auto sub_result = ParseSingle(query, &default_field);
            if (sub_result) {
                sub_result->MultiplyWeight(boost);
                or_children.emplace_back(std::move(sub_result));
            }
        }
        if (or_children.size() == 1) {
            parsed_query_tree = std::move(or_children[0]);
        } else if (!or_children.empty()) {
            parsed_query_tree = std::make_unique<OrQueryNode>();
            static_cast<OrQueryNode *>(parsed_query_tree.get())->children_ = std::move(or_children);
        }
    }
#ifdef INFINITY_DEBUG
    {
        OStringStream oss;
        oss << "Query tree:\n";
        if (parsed_query_tree) {
            parsed_query_tree->PrintTree(oss);
        } else {
            oss << "Empty query tree!\n";
        }
        LOG_INFO(std::move(oss).str());
    }
#endif
    return parsed_query_tree;
}

std::unique_ptr<QueryNode> SearchDriver::ParseSingle(const std::string &query, const std::string *default_field_ptr) const {
    std::istringstream iss(query);
    if (!iss.good()) {
        return nullptr;
    }
    if (!default_field_ptr) {
        default_field_ptr = &default_field_;
    }
    std::unique_ptr<SearchScanner> scanner;
    std::unique_ptr<SearchParser> parser;
    std::unique_ptr<QueryNode> result;
    try {
        scanner = std::make_unique<SearchScanner>(&iss);
        parser = std::make_unique<SearchParser>(*scanner, *this, *default_field_ptr, result);
    } catch (std::bad_alloc &ba) {
        std::cerr << "Failed to allocate: (" << ba.what() << "), exiting!!\n";
        return nullptr;
    }
    constexpr int accept = 0;
    if (parser->parse() != accept) {
        return nullptr;
    }
    return result;
}

std::unique_ptr<QueryNode> SearchDriver::AnalyzeAndBuildQueryNode(const std::string &field, std::string &&text) const {
    fmt::print("SearchDriver::AnalyzeAndBuildQueryNode, field = {}, text = {}\n", field, text);
    if (text.empty()) {
        RecoverableError(Status::SyntaxError("Empty query text"));
        return nullptr;
    }
    TermList terms;
    // 1. analyze
    bool analyzed = false;
    if (!field.empty()) {
        if (auto it = field2analyzer_.find(field); it != field2analyzer_.end()) {
            if (const std::string &analyzer_name = it->second; !analyzer_name.empty()) {
                auto analyzer_func = reinterpret_cast<void (*)(const std::string &, std::string &&, TermList &)>(analyze_func_);
                analyzer_func(analyzer_name, std::move(text), terms);
                analyzed = true;
            }
        }
    }
    for (auto& term : terms) {
        fmt::print("term = {}\n", term.Text());
    }
    // 2. build query node
    if (!analyzed) {
        auto result = std::make_unique<TermQueryNode>();
        result->term_ = std::move(text);
        result->column_ = field;
        return result;
    } else if (terms.empty()) {
        RecoverableError(Status::SyntaxError("Empty terms after analyzing"));
        return nullptr;
    } else if (terms.size() == 1) {
        fmt::print("Create Term Query Node\n");
        auto result = std::make_unique<TermQueryNode>();
        result->term_ = std::move(terms.front().text_);
        result->column_ = field;
        return result;
    } else {
        /*
        fmt::print("Create Or Query Node\n");
        auto result = std::make_unique<OrQueryNode>();
        for (auto &term : terms) {
            auto subquery = std::make_unique<TermQueryNode>();
            subquery->term_ = std::move(term.text_);
            subquery->column_ = field;
            result->Add(std::move(subquery));
        }
        return result;
        */
        fmt::print("Create Phrase Query Node\n");
        auto result = std::make_unique<PhraseQueryNode>();
        for (auto term : terms) {
            result->AddTerm(term.Text());
        }
        result->column_ = field;
        return result;
    }
}

} // namespace infinity
