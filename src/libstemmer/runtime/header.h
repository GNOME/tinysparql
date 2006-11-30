/* 
 * Copyright (c) 2001, Dr Martin Porter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of the <ORGANIZATION> nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */



#include <limits.h>

#include "api.h"

#define MAXINT INT_MAX
#define MININT INT_MIN

#define HEAD 2*sizeof(int)

#define SIZE(p)        ((int *)(p))[-1]
#define SET_SIZE(p, n) ((int *)(p))[-1] = n
#define CAPACITY(p)    ((int *)(p))[-2]

struct among
{   int s_size;     /* number of chars in string */
    symbol * s;       /* search string */
    int substring_i;/* index to longest matching substring */
    int result;     /* result of the lookup */
    int (* function)(struct SN_env *);
};

extern symbol * create_s(void);
extern void lose_s(symbol * p);

extern int skip_utf8(const symbol * p, int c, int lb, int l, int n);

extern int in_grouping_U(struct SN_env * z, unsigned char * s, int min, int max);
extern int in_grouping_b_U(struct SN_env * z, unsigned char * s, int min, int max);
extern int out_grouping_U(struct SN_env * z, unsigned char * s, int min, int max);
extern int out_grouping_b_U(struct SN_env * z, unsigned char * s, int min, int max);

extern int in_grouping(struct SN_env * z, unsigned char * s, int min, int max);
extern int in_grouping_b(struct SN_env * z, unsigned char * s, int min, int max);
extern int out_grouping(struct SN_env * z, unsigned char * s, int min, int max);
extern int out_grouping_b(struct SN_env * z, unsigned char * s, int min, int max);

extern int eq_s(struct SN_env * z, int s_size, symbol * s);
extern int eq_s_b(struct SN_env * z, int s_size, symbol * s);
extern int eq_v(struct SN_env * z, symbol * p);
extern int eq_v_b(struct SN_env * z, symbol * p);

extern int find_among(struct SN_env * z, struct among * v, int v_size);
extern int find_among_b(struct SN_env * z, struct among * v, int v_size);

extern int replace_s(struct SN_env * z, int c_bra, int c_ket, int s_size, const symbol * s, int * adjustment);
extern int slice_from_s(struct SN_env * z, int s_size, symbol * s);
extern int slice_from_v(struct SN_env * z, symbol * p);
extern int slice_del(struct SN_env * z);

extern int insert_s(struct SN_env * z, int bra, int ket, int s_size, symbol * s);
extern int insert_v(struct SN_env * z, int bra, int ket, symbol * p);

extern symbol * slice_to(struct SN_env * z, symbol * p);
extern symbol * assign_to(struct SN_env * z, symbol * p);

extern void debug(struct SN_env * z, int number, int line_count);

