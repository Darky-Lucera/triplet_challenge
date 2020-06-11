#include "hash2.h"

//-------------------------------------
//#define kUsePreProcess
//#define kUseMMap

//-------------------------------------
#if defined(kUseMMap) && !defined(_WIN32)
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

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
    uint8_t *read = (uint8_t *) buffer;
    uint8_t *write = (uint8_t *) buffer;

    // Clean start
    while (size > 0 && gLUT[*read] == 0) {
        ++read;
        --size;
    }

    size_t newSize = 0;
    while (size > 0) {
        uint8_t c = gLUT[*read];
        if (c != 0) {
            write[newSize++] = c;
            ++read;
            --size;
        }
        else {
            write[newSize++] = ' ';
            while (size > 0 && gLUT[*read] == 0) {
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

    if (argc != 2) {
        fprintf(stderr, "Use: %s file.txt\n", argv[0]);
        return -1;
    }

#if defined(_WIN32) || !defined(kUseMMap)
    FILE *pf = nullptr;

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
    size = sb.st_size;
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

    CustomHashMap words;

    const uint8_t *const end = buffer + size;
    String      triplet{ };            // 157858 triplets, 212199 words
    size_t      wordPosCount = 0;
    uint32_t    wordPos[2]{};

    triplet.count  = 1;
    triplet.buffer = buffer;

    // Get the two first words
    while (buffer < end && wordPosCount < 2) {
    #if defined(kUsePreProcess)
        if (*buffer != ' ') {
            ++triplet.length;
        }
        else {
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
            wordPos[wordPosCount++] = triplet.length;
            while (buffer < end && gLUT[*buffer] == 0) {
                ++buffer;
            }
        }
    #endif
    }

    // Get triplets
    while (buffer < end) {
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

            triplet.hash = CustomHashMap::Hash(triplet);
            words.mergeNode(triplet);

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

    String *elems = words.data();
    String *max1 = &elems[0];
    String *max2 = &elems[0];
    String *max3 = &elems[0];
    for (size_t i = 1; i < words.capacity(); ++i) {
        if ((elems[i].hash & 0x7fff'ffff) != 0) {
            if (elems[i].count > max1->count) {
                max3 = max2;
                max2 = max1;
                max1 = &elems[i];
            }
            else if (elems[i].count > max2->count) {
                max3 = max2;
                max2 = &elems[i];
            }
            else if (elems[i].count > max3->count) {
                max3 = &elems[i];
            }
        }
    }

    max1->word()[max1->length] = 0;
    max2->word()[max2->length] = 0;
    max3->word()[max3->length] = 0;
    printf("%s - %d\n%s - %d\n%s - %d\n", max1->word(), max1->count, max2->word(), max2->count, max3->word(), max3->count);

#if defined(kUseMMap) && !defined(_WIN32)
    if (fd != -1)
        close(fd);

    if (buffer != MAP_FAILED)
        munmap(buffer, sb.st_size);
#endif

    return 0;
}
