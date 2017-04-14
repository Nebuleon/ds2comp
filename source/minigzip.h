#include "zlib.h"

int  GzipCompress     OF((const char  *file, unsigned int level));
int  GzipUncompress   OF((const char  *file));
