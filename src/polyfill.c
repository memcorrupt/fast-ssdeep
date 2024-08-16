#include <errno.h>
#include <stdio.h>

//MSVC has no implementations for fseeko/ftello
#ifdef _MSC_VER

typedef size_t off_t;

int fseeko(FILE *stream, off_t offset, int whence){
    errno = ENOTSUP;
    return -1;
}

off_t ftello(FILE *stream){
    errno = ENOTSUP;
    return -1;
}

#endif