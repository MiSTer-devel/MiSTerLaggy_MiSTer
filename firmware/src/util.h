#if !defined(UTIL_H)
#define UTIL_H 1

#include <stdint.h>

static inline void memsetb(void *ptr, uint8_t c, size_t len)
{
    uint8_t *p = (uint8_t *)ptr;
    while( len )
    {
        *p = c;
        p++;
        len--;
    }
}

static inline void memsetw(void *ptr, uint16_t c, size_t len)
{
    uint16_t *p = (uint16_t *)ptr;
    while( len )
    {
        *p = c;
        p++;
        len--;
    }
}

static inline void memset(void *ptr, int c, size_t len)
{
    memsetb(ptr, c, len);
}

static inline void memcpyb(void *a, const void *b, size_t len)
{
    uint8_t *p_a = (uint8_t *)a;
    uint8_t *p_b = (uint8_t *)b;

    while( len )
    {
        *p_a = *p_b;
        p_a++;
        p_b++;
        len--;
    }
}

static inline void memcpyw(void *a, const void *b, size_t len)
{
    uint16_t *p_a = (uint16_t *)a;
    uint16_t *p_b = (uint16_t *)b;

    while( len )
    {
        *p_a = *p_b;
        p_a++;
        p_b++;
        len--;
    }
}

static inline void memcpy(void *a, const void *b, size_t len)
{
    memcpyb(a, b, len);
}

static inline int strlen(const char *s)
{
    const char *p = s;
    while(*p) { p++; }
    return p - s;
}

static inline void strcpy(char *dst, const char *src)
{
    const char *s = src;
    char *d = dst;
    while(*s)
    {
        *d = *s;
        s++;
        d++;
    }
    *d = '\0';
}

#define ARRAY_COUNT(x) (sizeof((x)) / sizeof((x)[0]))

#endif