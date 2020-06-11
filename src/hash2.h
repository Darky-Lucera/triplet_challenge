#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <cstring>
#include <cstdlib>

//-------------------------------------
struct String {
    static inline uint8_t *    buffer {};

    uint32_t            offset;
    union {
        uint64_t        lenHash;
        struct {
            uint32_t    hash;
            uint32_t    length : 16;
            uint32_t    count  : 16;
        };
    };

    inline uint8_t *word() const noexcept { return buffer + offset; }
};

//-------------------------------------
class CustomHashMap {
public:
    //---------------------------------
    CustomHashMap() noexcept {
        Allocate();
    }

    //---------------------------------
    ~CustomHashMap() noexcept {
        _aligned_free(mBuffer);
    }

    //---------------------------------
    void insert(String str) noexcept {
        if (++mNumNodes >= mResizeThreshold) {
            Grow();
        }
        InsertHelper(str);
    }

    //---------------------------------
    String *find(const String &str) noexcept {
        uint32_t pos = GetPosition(str.hash);
        uint32_t distance = 0;
        uint32_t nodeHash, nodeDistance;

        for (;;) {
            nodeHash = mBuffer[pos].hash;
            if (nodeHash == 0) {
                return nullptr;
            }
            else {
                nodeDistance = GetDistance(nodeHash, pos);
                if (distance > nodeDistance) {
                    return nullptr;
                }
                else if (((mBuffer[pos].lenHash ^ str.lenHash) & 0xffff'ffff'ffff) == 0 && Equal(mBuffer[pos], str)) {
                    return &mBuffer[pos];
                }
            }

            pos = (pos + 1) & mMask;
            ++distance;
        }
    }

    //---------------------------------
    const String *find(const String &str) const noexcept    { return const_cast<CustomHashMap *>(this)->find(str); }

    //---------------------------------
    void mergeNode(String str) noexcept {
        uint32_t pos       = GetPosition(str.hash);
        uint32_t distance  = 0;
        uint32_t nodeHash, nodeDistance;

        for (;;) {
            String &current = mBuffer[pos];
            nodeHash = current.hash;
            if (nodeHash == 0) {
                if (++mNumNodes >= mResizeThreshold) {
                    Grow();
                }
                current = str;
                return;
            }
            else {
                nodeDistance = GetDistance(nodeHash, pos);
                if (distance > nodeDistance) {
                    std::swap(str, current);
                    distance = nodeDistance;
                }
                else if (((current.lenHash ^ str.lenHash) & 0xffff'ffff'ffff) == 0 && Equal(current, str)) {
                    ++current.count;
                    return;
                }
            }

            pos = (pos + 1) & mMask;
            ++distance;
        }
    }

    //---------------------------------
    size_t size() const noexcept        { return mNumNodes;     }

    //---------------------------------
    size_t capacity() const noexcept    { return mTotalNodes;   }

    //---------------------------------
    const String *data() const noexcept { return mBuffer;       }

    //---------------------------------
    String *data() noexcept             { return mBuffer;       }

public:
    // http://www.orthogonal.com.au/computers/hashstrings/
    //---------------------------------
    static inline uint32_t Hash(const String &str) noexcept {
        enum { fnvPrime       = 16777619u   };
        enum { fnvOffsetBasis = 2166136261u };

        uint8_t *buffer = str.word();
        size_t  length  = str.length;

        uint32_t hash;
        uint32_t hash1 = buffer[length - 1] * fnvPrime;
        uint32_t hash2 = buffer[length - 2] * fnvPrime;
        uint32_t hash3 = buffer[length - 3] * fnvPrime;
        uint32_t hash4 = buffer[length - 4] * fnvPrime;
        length -= 4;

        while (ptrdiff_t(length) > 3) {
            hash1 ^= buffer[length - 1];
            hash2 ^= buffer[length - 2];
            hash3 ^= buffer[length - 3];
            hash4 ^= buffer[length - 4];
            hash1 *= fnvPrime;
            hash2 *= fnvPrime;
            hash3 *= fnvPrime;
            hash4 *= fnvPrime;
            length -= 4;
        }
        if (length > 2) {
            hash1 ^= buffer[length - 3];
            hash1 *= fnvPrime;
        }
        if (length > 1) {
            hash2 ^= buffer[length - 2];
            hash2 *= fnvPrime;
        }
        if (length > 0) {
            hash3 ^= buffer[length - 1];
            hash3 *= fnvPrime;
        }

        hash  = (hash1 ^ hash2) ^ (hash3 ^ hash4);
        hash |= hash == 0;  // Avoid 0 as hash

        return hash;
    }

    //---------------------------------
    static inline bool Equal(const String &a, const String &b) noexcept {
        return memcmp(a.buffer + a.offset, b.buffer + b.offset, a.length) == 0;
    }

protected:
    //---------------------------------
    inline uint32_t GetPosition(uint32_t hash) const noexcept {
        return hash & mMask;
    }

    //---------------------------------
    inline uint32_t GetDistance(uint32_t hash, uint32_t pos) const noexcept {
        return (mTotalNodes + pos - GetPosition(hash)) & mMask;
    }

    //---------------------------------
    void Allocate() noexcept {
        mBuffer = reinterpret_cast<String *>(_aligned_malloc(mTotalNodes * sizeof(String), __alignof(String)));

        memset(mBuffer, 0, mTotalNodes * sizeof(String));

        mResizeThreshold = (mTotalNodes * kLoadFactor) / 100;
        mMask = uint32_t(mTotalNodes) - 1;
    }

    //---------------------------------
    void Grow() noexcept {
        String *oldBuffer     = mBuffer;
        size_t  oldTotalNodes = mTotalNodes;

        mTotalNodes *= 2;
        Allocate();

        for (size_t i = 0; i < oldTotalNodes; ++i) {
            String &node = oldBuffer[i];
            if (node.hash != 0) {
                InsertHelper(node);
            }
        }

        _aligned_free(oldBuffer);
    }

    //---------------------------------
    inline void InsertHelper(String &key) noexcept {
        uint32_t hash      = key.hash;
        uint32_t pos       = GetPosition(hash);
        uint32_t distance  = 0;
        uint32_t nodeHash, nodeDistance;

        for (;;) {
            nodeHash = mBuffer[pos].hash;
            if (nodeHash == 0) {
                mBuffer[pos] = key;
                return;
            }

            nodeDistance = GetDistance(nodeHash, pos);
            if (distance > nodeDistance) {
                std::swap(key, mBuffer[pos]);
                hash = key.hash;
                distance = nodeDistance;
            }

            pos = (pos + 1) & mMask;
            ++distance;
        }
    }

public:
    enum { kInitialSize = 0x40000 };
    enum { kLoadFactor  = 90 };

public:
    String *__restrict mBuffer  { };
    size_t   mNumNodes          { };
    size_t   mTotalNodes        { kInitialSize };
    size_t   mResizeThreshold;
    uint32_t mMask;
};