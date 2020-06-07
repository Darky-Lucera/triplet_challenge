#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <map>
#include <unordered_map>
#include <string>
#include "allocator.h"

#if !defined(_WIN32)
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

//-------------------------------------
#define kUseUnorderedMap
//#define kUsePreProcess
//#define kUsePMR
#define kUseMemPool
//#define kShowTime

//-------------------------------------
#if defined(kUsePMR)
    #if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
        #include <memory_resource>
    #else 
        #undef kUsePMR
    #endif
#endif

//-------------------------------------
#if defined(kShowTime)
    #include <chrono>

    using Timer      = std::chrono::high_resolution_clock;
    using time_point = Timer::time_point;

    #define kTick(varName)  time_point varName = Tick()
#else
    #define kTick(varName)
#endif

//-------------------------------------
using std::string;
using std::pair;

#if defined(kShowTime)
//-------------------------------------
static inline time_point
Tick() {
    return Timer::now();
}

//-------------------------------------
static inline uint64_t
TimeDiff(time_point now, time_point past) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now - past).count();
}
#endif

//-------------------------------------
struct String {
    static uint8_t *    buffer;
    union {
        uint64_t        lenHash;
        struct {
            uint32_t    hash;
            uint32_t    length : 8;
            uint32_t    offset : 24;
        };
    };

    inline uint8_t *    word() const noexcept { return buffer + offset; }
};
uint8_t *    String::buffer {};

//-------------------------------------
struct StringLess {
    inline bool operator()(const String &a, const String &b) const {
        uint64_t aLenHash = a.lenHash & 0xffffffffff;
        uint64_t bLenHash = b.lenHash & 0xffffffffff;

        if (aLenHash < bLenHash)
            return true;

        if (aLenHash > bLenHash)
            return false;

        return memcmp(a.buffer + a.offset, b.buffer + b.offset, a.length) < 0;
    }
};

//-------------------------------------
struct StringEqual {
    inline bool operator()(const String &a, const String &b) const {
        uint64_t aLenHash = a.lenHash & 0xffffffffff;
        uint64_t bLenHash = b.lenHash & 0xffffffffff;

        if (aLenHash != bLenHash)
            return false;

        return memcmp(a.buffer + a.offset, b.buffer + b.offset, a.length) == 0;
    }
};

//-------------------------------------
struct StringHash {
    inline size_t operator()(const String &str) const {
        return str.hash;
    }
};

//-------------------------------------
#if defined(kUseUnorderedMap)
    #if defined(kUsePMR)
        using Map = std::pmr::unordered_map<String, int, StringHash, StringEqual>;
    #elif defined(kUseMemPool)
        using CustomAllocatorPair = CustomAllocator<std::unordered_map<String, int, StringHash, StringEqual>::value_type>;
        using Map = std::unordered_map<String, int, StringHash, StringEqual, CustomAllocatorPair>;
    #else
        using Map = std::unordered_map<String, int, StringHash, StringEqual>;
    #endif
#else
    #if defined(kUsePMR)
        using Map = std::pmr::map<String, int, StringLess>;
    #elif defined(kUseMemPool)
        using CustomAllocatorPair = CustomAllocator<std::map<String, int, StringLess>::value_type>;
        using Map = std::map<String, int, StringLess, CustomAllocatorPair>;
    #else
        using Map = std::map<String, int, StringLess>;
    #endif
#endif

//-------------------------------------
string
Expand(const uint8_t *str) {
    char    buffer[256];
    size_t  pos = 0, count = 0;

    do {
        if (*str != 0) {
            buffer[pos++] = *str;
        }
        else {
            ++count;
            if (count < 3)
                buffer[pos++] = ' ';
            else
                buffer[pos] = 0;
        }
        ++str;
    } while (count < 3);

    return buffer;
}

//-------------------------------------
string
Expand(const String &str) {
    string aux((const char *)str.buffer + str.offset, str.length);

    return aux;
}

// http://www.orthogonal.com.au/computers/hashstrings/
//-------------------------------------
static inline uint32_t 
fnvHash32(const uint8_t *str, size_t length) {
    enum { fnvPrime       = 16777619u   };
    enum { fnvOffsetBasis = 2166136261u };
    //uint32_t hash = fnvOffsetBasis;
    //
    //while (length-- > 0) {
    //    hash ^= str[length];
    //    hash *= fnvPrime;
    //}

    //return hash;

    uint32_t hash1 = str[length - 0] * fnvPrime;
    uint32_t hash2 = str[length - 1] * fnvPrime;
    uint32_t hash3 = str[length - 2] * fnvPrime;
    uint32_t hash4 = str[length - 3] * fnvPrime;
    length -= 4;

    while (ptrdiff_t(length) > 3) {
        hash1 ^= str[length - 0];
        hash2 ^= str[length - 1];
        hash3 ^= str[length - 2];
        hash4 ^= str[length - 3];
        hash1 *= fnvPrime;
        hash2 *= fnvPrime;
        hash3 *= fnvPrime;
        hash4 *= fnvPrime;
        length -= 4;
    }
    if(length > 2) {
        hash1 ^= str[length - 2];
        hash1 *= fnvPrime;
    }
    if (length > 1) {
        hash2 ^= str[length - 1];
        hash2 *= fnvPrime;
    }
    if (length > 0) {
        hash3 ^= str[length - 0];
        hash3 *= fnvPrime;
    }

    return (hash1 ^ hash2) ^ (hash3 ^ hash4);
}

//-------------------------------------
static uint8_t gLUT[256] = {
     0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
     0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
     0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 39 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
     0 , 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',  0 ,  0 ,  0 ,  0 ,  0 ,
     0 , 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',  0 ,  0 ,  0 ,  0 ,  0 ,
     0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
     0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
     0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
     0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
     0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
     0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
     0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
     0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
};

//-------------------------------------
size_t 
PreprocessBuffer(uint8_t *buffer, size_t size) {
    uint8_t *read  = (uint8_t *) buffer;
    uint8_t *write = (uint8_t *) buffer;

    // Clean start
    while (size > 0 && gLUT[*read] == 0) {
        ++read;
        --size;
    }

    size_t newSize = 0;
    while(size > 0) {
        uint8_t c = gLUT[*read];
        if(c != 0) {
            write[newSize++] = c;
            ++read;
            --size;
        }
        else {
            write[newSize++] = ' ';
            while(size > 0 && gLUT[*read] == 0) {
                ++read;
                --size;
            }
        }
    }

    return newSize;
}

//-------------------------------------
int
main(int argc, char *argv[]) {
    size_t  size;
    uint8_t *buffer;

    kTick(timestamp);

    if (argc != 2) {
        fprintf(stderr, "Use: %s file.txt\n", argv[0]);
        return -1;
    }

#if defined(_WIN32)
    FILE    *pf = nullptr;

    pf = fopen(argv[1], "rb");
    if (pf == nullptr) {
        fprintf(stderr, "Can't open file '%s'\n", argv[1]);
        return -1;
    }
    fseek(pf, 0, SEEK_END);
    size = ftell(pf);
    fseek(pf, 0, SEEK_SET);
    buffer = (uint8_t *) malloc(size + 1);
    fread(buffer, size, 1, pf);
    fclose(pf);
#else
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Can't open file '%s'\n", argv[1]);
        return -1;
    }

    // Get the size
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        fprintf(stderr, "Can't get stat of file '%s'\n", argv[1]);
        return -1;
    }

    // map the whole file into virtual memory. Let's delegate the implementation of reading in chunks to the OS
    buffer = (uint8_t *) mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    size   = sb.st_size;
#endif

    kTick(readTime);

#if defined(kUsePMR)
    std::pmr::monotonic_buffer_resource mbr;
#elif defined(kUseMemPool)
    CustomPool          pool(1024 * 1024 * 13);
    CustomAllocatorPair ator(&pool);
#endif

#if defined(kUsePMR)
    Map words(&mbr);
#elif defined(kUseMemPool)
    Map words(ator);
#else
    Map words;
#endif

#if defined(kUsePreProcess)
    size = PreprocessBuffer(buffer, size);
#else
    // Clean start
    while (size > 0 && gLUT[*buffer] == 0) {
        ++buffer;
        --size;
    }
#endif

    kTick(preprocess);

    const uint8_t * const end = buffer + size;
    String      triplet { };            // 157858 triplets
    size_t      wordCount = 0;          // 212199 words
    size_t      wordPosCount = 0;
    uint32_t    wordPos[2]{};

    triplet.buffer = buffer;

    // Get the two first words
    while (buffer < end && wordPosCount < 2) {
#if defined(kUsePreProcess)
        if(*buffer != ' ') {
            ++triplet.length;
        }
        else {
            ++wordCount;
            ++triplet.length;
            wordPos[wordPosCount++] = triplet.length;
        }
        ++buffer;
#else
        uint8_t c = gLUT[*buffer];
        if (c != 0) {
            triplet.word()[triplet.length++] = c;
            ++buffer;
        }
        else {
            triplet.word()[triplet.length++] = ' ';
            ++wordCount;
            wordPos[wordPosCount++] = triplet.length;
            while (buffer < end && gLUT[*buffer] == 0) {
                ++buffer;
            }
        }
#endif
    }

    // Get triplets
    while(buffer < end) {
#if defined(kUsePreProcess)
        if (*buffer != ' ') {
            ++triplet.length;
        }
        else {
#else
        uint8_t c = gLUT[*buffer];
        if (c != 0) {
            triplet.word()[triplet.length++] = c;
            ++buffer;
        }
        else {
            triplet.word()[triplet.length] = ' ';
            while (buffer < end && gLUT[*buffer] == 0) {
                ++buffer;
            }
#endif
            ++wordCount;
            //printf("%s\n", Expand(triplet).c_str());

            triplet.hash = fnvHash32((uint8_t *) triplet.word(), triplet.length);
            auto it = words.find(triplet);
            if (it == words.end()) {
                words.emplace(triplet, 1);
            }
            else {
                ++it->second;
            }

            triplet.offset += wordPos[0];
            uint32_t w0 = wordPos[1] - wordPos[0];
            uint32_t w1 = triplet.length - wordPos[0] + 1;
            triplet.length -= wordPos[0] - 1;
            wordPos[0] = w0;
            wordPos[1] = w1;
        }
#if defined(kUsePreProcess)
        ++buffer;
#endif
    }

    pair<String, int> max1{}, max2{}, max3{};
    for (const auto &p : words) {
        if (p.second > max1.second) {
            max3 = max2;
            max2 = max1;
            max1 = p;
        }
        else if (p.second > max2.second) {
            max3 = max2;
            max2 = p;
        }
        else if (p.second > max3.second) {
            max3 = p;
        }
    }

#if !defined(_WIN32)
    if (fd != -1)
        close(fd);

    if (buffer != MAP_FAILED)
        munmap(buffer, sb.st_size);
#endif

    kTick(process);

#if defined(kShowTime)
    printf("Read:       %.2f ms\n", TimeDiff(readTime, timestamp) / 1e6);
    printf("Preprocess: %.2f ms (%.2f ms)\n", TimeDiff(preprocess, timestamp) / 1e6, TimeDiff(preprocess, readTime) / 1e6);
    printf("Process:    %.2f ms (%.2f ms)\n", TimeDiff(process, timestamp) / 1e6, TimeDiff(process, preprocess) / 1e6);
    printf("Word Count: %lld\n", wordCount);
    printf("Triplet Count: %lld\n", words.size());
#endif

    max1.first.word()[max1.first.length] = 0;
    max2.first.word()[max2.first.length] = 0;
    max3.first.word()[max3.first.length] = 0;
    printf("%s - %d\n%s - %d\n%s - %d\n", max1.first.word(), max1.second, max2.first.word(), max2.second, max3.first.word(), max3.second);

    return 0;
}
