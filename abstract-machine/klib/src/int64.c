#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define _YUGA_LITTLE_ENDIAN 1
#define _YUGA_BIG_ENDIAN    0
#else
#define _YUGA_LITTLE_ENDIAN 1
#define _YUGA_BIG_ENDIAN    0
#endif

typedef unsigned int su_int;
typedef int si_int;
typedef unsigned long long du_int;
typedef long long di_int;

typedef union {
    du_int all;
    struct {
#if _YUGA_LITTLE_ENDIAN
        su_int low;
        su_int high;
#else
        su_int high;
        su_int low;
#endif
    } s;
} udwords;

du_int __udivmoddi4(du_int a, du_int b, du_int* rem) {
    const unsigned n_uword_bits = sizeof(su_int) * 8;
    const unsigned n_udword_bits = sizeof(du_int) * 8;
    udwords n; n.all = a;
    udwords d; d.all = b;
    udwords q;
    udwords r;
    unsigned sr;

    if (n.s.high == 0) {
        if (d.s.high == 0) {
            if (rem) *rem = (du_int)(n.s.low % d.s.low);
            udwords res;
            res.s.low = n.s.low / d.s.low;
            res.s.high = 0;
            return res.all;
        }
        if (rem) *rem = (du_int)n.s.low;
        return 0;
    }

    if (d.s.low == 0) {
        if (d.s.high == 0) {
            if (rem) *rem = (du_int)(n.s.high % d.s.low);
            udwords res;
            res.s.low = n.s.high / d.s.low;
            res.s.high = 0;
            return res.all;
        }
        if (n.s.low == 0) {
            if (rem) {
                r.s.high = n.s.high % d.s.high;
                r.s.low = 0;
                *rem = r.all;
            }
            udwords res;
            res.s.low = n.s.high / d.s.high;
            res.s.high = 0;
            return res.all;
        }
        if ((d.s.high & (d.s.high - 1)) == 0) {
            if (rem) {
                r.s.low = n.s.low;
                r.s.high = n.s.high & (d.s.high - 1);
                *rem = r.all;
            }
            udwords res;
            res.s.low = n.s.high >> __builtin_ctz(d.s.high);
            res.s.high = 0;
            return res.all;
        }
        sr = __builtin_clz(d.s.high) - __builtin_clz(n.s.high);
        if (sr > n_uword_bits - 2) {
            if (rem) *rem = n.all;
            return 0;
        }
        ++sr;
        q.s.low = 0;
        q.s.high = n.s.low << (n_uword_bits - sr);
        r.s.high = n.s.high >> sr;
        r.s.low = (n.s.high << (n_uword_bits - sr)) | (n.s.low >> sr);
    } else {
        if (d.s.high == 0) {
            if ((d.s.low & (d.s.low - 1)) == 0) {
                if (rem) *rem = (du_int)(n.s.low & (d.s.low - 1));
                if (d.s.low == 1) return n.all;
                sr = __builtin_ctz(d.s.low);
                q.s.high = n.s.high >> sr;
                q.s.low = (n.s.high << (n_uword_bits - sr)) | (n.s.low >> sr);
                return q.all;
            }
            sr = 1 + n_uword_bits + __builtin_clz(d.s.low) - __builtin_clz(n.s.high);
            if (sr == n_uword_bits) {
                q.s.low = 0; q.s.high = n.s.low;
                r.s.high = 0; r.s.low = n.s.high;
            } else if (sr < n_uword_bits) {
                q.s.low = 0; q.s.high = n.s.low << (n_uword_bits - sr);
                r.s.high = n.s.high >> sr;
                r.s.low = (n.s.high << (n_uword_bits - sr)) | (n.s.low >> sr);
            } else {
                q.s.low = n.s.low << (n_udword_bits - sr);
                q.s.high = (n.s.high << (n_udword_bits - sr)) | (n.s.low >> (sr - n_uword_bits));
                r.s.high = 0; r.s.low = n.s.high >> (sr - n_uword_bits);
            }
        } else {
            sr = __builtin_clz(d.s.high) - __builtin_clz(n.s.high);
            if (sr > n_uword_bits - 1) {
                if (rem) *rem = n.all;
                return 0;
            }
            ++sr;
            q.s.low = 0;
            if (sr == n_uword_bits) {
                q.s.high = n.s.low;
                r.s.high = 0; r.s.low = n.s.high;
            } else {
                q.s.high = n.s.low << (n_uword_bits - sr);
                r.s.high = n.s.high >> sr;
                r.s.low = (n.s.high << (n_uword_bits - sr)) | (n.s.low >> sr);
            }
        }
    }

    su_int carry = 0;
    for (; sr > 0; --sr) {
        r.s.high = (r.s.high << 1) | (r.s.low  >> (n_uword_bits - 1));
        r.s.low  = (r.s.low  << 1) | (q.s.high >> (n_uword_bits - 1));
        q.s.high = (q.s.high << 1) | (q.s.low  >> (n_uword_bits - 1));
        q.s.low  = (q.s.low  << 1) | carry;
        const di_int s = (di_int)(d.all - r.all - 1) >> (n_udword_bits - 1);
        carry = s & 1;
        r.all -= d.all & s;
    }
    q.all = (q.all << 1) | carry;
    if (rem) *rem = r.all;
    return q.all;
}

du_int __udivdi3(du_int a, du_int b) {
    volatile du_int q = __udivmoddi4(a, b, 0);
    return q;
}

du_int __umoddi3(du_int a, du_int b) { du_int r; __udivmoddi4(a, b, &r); return r; }
di_int __divdi3(di_int a, di_int b) {
    di_int s_a = a >> 63;
    di_int s_b = b >> 63;
    a = (a ^ s_a) - s_a;
    b = (b ^ s_b) - s_b;
    s_a ^= s_b;
    return (__udivmoddi4(a, b, 0) ^ s_a) - s_a;
}
di_int __moddi3(di_int a, di_int b) {
    di_int s = b >> 63;
    b = (b ^ s) - s;
    s = a >> 63;
    a = (a ^ s) - s;
    du_int r;
    __udivmoddi4(a, b, &r);
    return ((di_int)r ^ s) - s;
}
