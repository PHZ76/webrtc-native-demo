#ifndef _XOP_LOG_H
#define _XOP_LOG_H

#include <cstdio>

//#ifdef _DEBUG
#define LOG(format, ...)  	\
{								\
    fprintf(stderr, "[DEBUG] [%s:%d] " format "", \
        __FUNCTION__ , __LINE__, ##__VA_ARGS__);     \
}
//#else
//#define LOG(format, ...)  	
//#endif 


#endif
