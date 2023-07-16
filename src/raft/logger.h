#pragma once

#include <cstdio>
#include <cstring>

#define DEBUG

#define __FILENAME__ (strrchr(__FILE__, '/') + 1)

#ifdef DEBUG
#  define LOG(format, ...)                \
    printf("[%s][%s][%d]: " format "\n",  \
           __FILENAME__,                  \
           __FUNCTION__,                  \
           __LINE__,                      \
           ##__VA_ARGS__)
#endif