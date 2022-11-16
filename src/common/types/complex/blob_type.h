//
// Created by JinHai on 2022/10/30.
//

#pragma once

#include "common/types/varlen_type.h"

namespace infinity {

struct BlobType {
public:
    ptr_t ptr {nullptr};
    u64 size {0};  // 4GB will be the limitation.

public:
    inline
    BlobType() = default;

    // The blob_ptr will also be freed by BlobType's destructor.
    inline
    BlobType(ptr_t blob_ptr, u64 blob_size)
        : ptr(blob_ptr), size(blob_size)
        {}

    inline
    ~BlobType() {
        Reset();
    }

    BlobType(const BlobType& other);
    BlobType(BlobType&& other) noexcept;
    BlobType& operator=(const BlobType& other);
    BlobType& operator=(BlobType&& other) noexcept;

public:
    void
    Copy(ptr_t blob_ptr, u64 blob_size);

    void
    Move(ptr_t blob_ptr, u64 blob_size);

    inline void
    Reset() {
        if(size != 0) {
            size = 0;
            delete[] ptr;
            ptr = nullptr;
        }
    }
};

}
