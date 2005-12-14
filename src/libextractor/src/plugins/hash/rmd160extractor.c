/*
     This file is part of libextractor.
     (C) 2004 Vidyut Samanta and Christian Grothoff
     Copyright © 1999 - Philip Howard

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
     option) any later version.

     libextractor is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with libextractor; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.

     RMD code:
     package	rmd160
     version	1.2.1
     homepage	http://phil.ipal.org/freeware/rmd160/

     author	Philip Howard
     email	phil@ipal.org
     homepage	http://phil.ipal.org/

 */

#include "platform.h"
#include "extractor.h"

#if ULONG_MAX>4294967295U
#if UINT_MAX>4294967295U
#if USHRT_MAX>4294967295U
#if UCHAR_MAX>=4294967295U
typedef unsigned char rmd160uint32;
#else
typedef unsigned short rmd160uint32;
#endif /*UCHAR_MAX*/
#else
typedef unsigned int rmd160uint32;
#endif /*USHRT_MAX*/
#else
typedef unsigned long rmd160uint32;
#endif /*UINT_MAX*/
#else
typedef unsigned long rmd160uint32;
#endif

struct rmd160_object {
    rmd160uint32		data	[16]	;
    rmd160uint32		state	[5]	;
    rmd160uint32		len	[2]	;
    rmd160uint32 *		wptr		;
    rmd160uint32 *		wend		;
    unsigned int		bpos		;
};
typedef struct rmd160_object *	RMD160		;

#define RMD160_OK			0
#define RMD160_ERR_INVALID_ARG		1
#define RMD160_ERR_READ			2


#define RMD160_BUFSIZ		1024

#define RMD160_INIT0		0x67452301UL
#define RMD160_INIT1		0xefcdab89UL
#define RMD160_INIT2		0x98badcfeUL
#define RMD160_INIT3		0x10325476UL
#define RMD160_INIT4		0xc3d2e1f0UL

#define FUNCTION_NAME "rmd160_calc"

static int
_rmd160_calc (
	rmd160uint32 *	state
,
	rmd160uint32 *	data
)
{
    rmd160uint32 x0,x1,x2,x3,x4;
    rmd160uint32 y0,y1,y2,y3,y4;

    x0 = y0 = state[0];
    x1 = y1 = state[1];
    x2 = y2 = state[2];
    x3 = y3 = state[3];
    x4 = y4 = state[4];


#define RL(x,n)		(((x) << (n)) | ((x) >> (32-(n))))

#define F1(x,y,z)	((x) ^ (y) ^ (z))
#define F2(x,y,z)	(((x) & (y)) | (~(x) & (z)))
#define F3(x,y,z)	(((x) | ~(y)) ^ (z))
#define F4(x,y,z)	(((x) & (z)) | ((y) & ~(z)))
#define F5(x,y,z)	((x) ^ ((y) | ~(z)))

#define T1(a,b,c,d,e,x,s) {						\
			(a) += F1((b),(c),(d)) + (x);			\
			(a) = RL((a),(s)) + (e);			\
			(c) = RL((c),10);}
#define T2(a,b,c,d,e,x,s) {						\
			(a) += F2((b),(c),(d)) + (x) + 0x5a827999UL;	\
			(a) = RL((a),(s)) + (e);			\
			(c) = RL((c),10);}
#define T3(a,b,c,d,e,x,s) {						\
			(a) += F3((b),(c),(d)) + (x) + 0x6ed9eba1UL;	\
			(a) = RL((a),(s)) + (e);			\
			(c) = RL((c),10);}
#define T4(a,b,c,d,e,x,s) {						\
			(a) += F4((b),(c),(d)) + (x) + 0x8f1bbcdcUL;	\
			(a) = RL((a),(s)) + (e);			\
			(c) = RL((c),10);}
#define T5(a,b,c,d,e,x,s) {						\
			(a) += F5((b),(c),(d)) + (x) + 0xa953fd4eUL;	\
			(a) = RL((a),(s)) + (e);			\
			(c) = RL((c),10);}

#define S1(a,b,c,d,e,x,s) {						\
			(a) += F1((b),(c),(d)) + (x);			\
			(a) = RL((a),(s)) + (e);			\
			(c) = RL((c),10);}
#define S2(a,b,c,d,e,x,s) {						\
			(a) += F2((b),(c),(d)) + (x) + 0x7a6d76e9UL;	\
			(a) = RL((a),(s)) + (e);			\
			(c) = RL((c),10);}
#define S3(a,b,c,d,e,x,s) {						\
			(a) += F3((b),(c),(d)) + (x) + 0x6d703ef3UL;	\
			(a) = RL((a),(s)) + (e);			\
			(c) = RL((c),10);}
#define S4(a,b,c,d,e,x,s) {						\
			(a) += F4((b),(c),(d)) + (x) + 0x5c4dd124UL;	\
			(a) = RL((a),(s)) + (e);			\
			(c) = RL((c),10);}
#define S5(a,b,c,d,e,x,s) {						\
			(a) += F5((b),(c),(d)) + (x) + 0x50a28be6UL;	\
			(a) = RL((a),(s)) + (e);			\
			(c) = RL((c),10);}

    T1( x0 , x1 , x2 , x3 , x4 , data[ 0] , 11 );
    T1( x4 , x0 , x1 , x2 , x3 , data[ 1] , 14 );
    T1( x3 , x4 , x0 , x1 , x2 , data[ 2] , 15 );
    T1( x2 , x3 , x4 , x0 , x1 , data[ 3] , 12 );
    T1( x1 , x2 , x3 , x4 , x0 , data[ 4] ,  5 );
    T1( x0 , x1 , x2 , x3 , x4 , data[ 5] ,  8 );
    T1( x4 , x0 , x1 , x2 , x3 , data[ 6] ,  7 );
    T1( x3 , x4 , x0 , x1 , x2 , data[ 7] ,  9 );
    T1( x2 , x3 , x4 , x0 , x1 , data[ 8] , 11 );
    T1( x1 , x2 , x3 , x4 , x0 , data[ 9] , 13 );
    T1( x0 , x1 , x2 , x3 , x4 , data[10] , 14 );
    T1( x4 , x0 , x1 , x2 , x3 , data[11] , 15 );
    T1( x3 , x4 , x0 , x1 , x2 , data[12] ,  6 );
    T1( x2 , x3 , x4 , x0 , x1 , data[13] ,  7 );
    T1( x1 , x2 , x3 , x4 , x0 , data[14] ,  9 );
    T1( x0 , x1 , x2 , x3 , x4 , data[15] ,  8 );

    T2( x4 , x0 , x1 , x2 , x3 , data[ 7] ,  7 );
    T2( x3 , x4 , x0 , x1 , x2 , data[ 4] ,  6 );
    T2( x2 , x3 , x4 , x0 , x1 , data[13] ,  8 );
    T2( x1 , x2 , x3 , x4 , x0 , data[ 1] , 13 );
    T2( x0 , x1 , x2 , x3 , x4 , data[10] , 11 );
    T2( x4 , x0 , x1 , x2 , x3 , data[ 6] ,  9 );
    T2( x3 , x4 , x0 , x1 , x2 , data[15] ,  7 );
    T2( x2 , x3 , x4 , x0 , x1 , data[ 3] , 15 );
    T2( x1 , x2 , x3 , x4 , x0 , data[12] ,  7 );
    T2( x0 , x1 , x2 , x3 , x4 , data[ 0] , 12 );
    T2( x4 , x0 , x1 , x2 , x3 , data[ 9] , 15 );
    T2( x3 , x4 , x0 , x1 , x2 , data[ 5] ,  9 );
    T2( x2 , x3 , x4 , x0 , x1 , data[ 2] , 11 );
    T2( x1 , x2 , x3 , x4 , x0 , data[14] ,  7 );
    T2( x0 , x1 , x2 , x3 , x4 , data[11] , 13 );
    T2( x4 , x0 , x1 , x2 , x3 , data[ 8] , 12 );

    T3( x3 , x4 , x0 , x1 , x2 , data[ 3] , 11 );
    T3( x2 , x3 , x4 , x0 , x1 , data[10] , 13 );
    T3( x1 , x2 , x3 , x4 , x0 , data[14] ,  6 );
    T3( x0 , x1 , x2 , x3 , x4 , data[ 4] ,  7 );
    T3( x4 , x0 , x1 , x2 , x3 , data[ 9] , 14 );
    T3( x3 , x4 , x0 , x1 , x2 , data[15] ,  9 );
    T3( x2 , x3 , x4 , x0 , x1 , data[ 8] , 13 );
    T3( x1 , x2 , x3 , x4 , x0 , data[ 1] , 15 );
    T3( x0 , x1 , x2 , x3 , x4 , data[ 2] , 14 );
    T3( x4 , x0 , x1 , x2 , x3 , data[ 7] ,  8 );
    T3( x3 , x4 , x0 , x1 , x2 , data[ 0] , 13 );
    T3( x2 , x3 , x4 , x0 , x1 , data[ 6] ,  6 );
    T3( x1 , x2 , x3 , x4 , x0 , data[13] ,  5 );
    T3( x0 , x1 , x2 , x3 , x4 , data[11] , 12 );
    T3( x4 , x0 , x1 , x2 , x3 , data[ 5] ,  7 );
    T3( x3 , x4 , x0 , x1 , x2 , data[12] ,  5 );

    T4( x2 , x3 , x4 , x0 , x1 , data[ 1] , 11 );
    T4( x1 , x2 , x3 , x4 , x0 , data[ 9] , 12 );
    T4( x0 , x1 , x2 , x3 , x4 , data[11] , 14 );
    T4( x4 , x0 , x1 , x2 , x3 , data[10] , 15 );
    T4( x3 , x4 , x0 , x1 , x2 , data[ 0] , 14 );
    T4( x2 , x3 , x4 , x0 , x1 , data[ 8] , 15 );
    T4( x1 , x2 , x3 , x4 , x0 , data[12] ,  9 );
    T4( x0 , x1 , x2 , x3 , x4 , data[ 4] ,  8 );
    T4( x4 , x0 , x1 , x2 , x3 , data[13] ,  9 );
    T4( x3 , x4 , x0 , x1 , x2 , data[ 3] , 14 );
    T4( x2 , x3 , x4 , x0 , x1 , data[ 7] ,  5 );
    T4( x1 , x2 , x3 , x4 , x0 , data[15] ,  6 );
    T4( x0 , x1 , x2 , x3 , x4 , data[14] ,  8 );
    T4( x4 , x0 , x1 , x2 , x3 , data[ 5] ,  6 );
    T4( x3 , x4 , x0 , x1 , x2 , data[ 6] ,  5 );
    T4( x2 , x3 , x4 , x0 , x1 , data[ 2] , 12 );

    T5( x1 , x2 , x3 , x4 , x0 , data[ 4] ,  9 );
    T5( x0 , x1 , x2 , x3 , x4 , data[ 0] , 15 );
    T5( x4 , x0 , x1 , x2 , x3 , data[ 5] ,  5 );
    T5( x3 , x4 , x0 , x1 , x2 , data[ 9] , 11 );
    T5( x2 , x3 , x4 , x0 , x1 , data[ 7] ,  6 );
    T5( x1 , x2 , x3 , x4 , x0 , data[12] ,  8 );
    T5( x0 , x1 , x2 , x3 , x4 , data[ 2] , 13 );
    T5( x4 , x0 , x1 , x2 , x3 , data[10] , 12 );
    T5( x3 , x4 , x0 , x1 , x2 , data[14] ,  5 );
    T5( x2 , x3 , x4 , x0 , x1 , data[ 1] , 12 );
    T5( x1 , x2 , x3 , x4 , x0 , data[ 3] , 13 );
    T5( x0 , x1 , x2 , x3 , x4 , data[ 8] , 14 );
    T5( x4 , x0 , x1 , x2 , x3 , data[11] , 11 );
    T5( x3 , x4 , x0 , x1 , x2 , data[ 6] ,  8 );
    T5( x2 , x3 , x4 , x0 , x1 , data[15] ,  5 );
    T5( x1 , x2 , x3 , x4 , x0 , data[13] ,  6 );

    S5( y0 , y1 , y2 , y3 , y4 , data[ 5] ,  8 );
    S5( y4 , y0 , y1 , y2 , y3 , data[14] ,  9 );
    S5( y3 , y4 , y0 , y1 , y2 , data[ 7] ,  9 );
    S5( y2 , y3 , y4 , y0 , y1 , data[ 0] , 11 );
    S5( y1 , y2 , y3 , y4 , y0 , data[ 9] , 13 );
    S5( y0 , y1 , y2 , y3 , y4 , data[ 2] , 15 );
    S5( y4 , y0 , y1 , y2 , y3 , data[11] , 15 );
    S5( y3 , y4 , y0 , y1 , y2 , data[ 4] ,  5 );
    S5( y2 , y3 , y4 , y0 , y1 , data[13] ,  7 );
    S5( y1 , y2 , y3 , y4 , y0 , data[ 6] ,  7 );
    S5( y0 , y1 , y2 , y3 , y4 , data[15] ,  8 );
    S5( y4 , y0 , y1 , y2 , y3 , data[ 8] , 11 );
    S5( y3 , y4 , y0 , y1 , y2 , data[ 1] , 14 );
    S5( y2 , y3 , y4 , y0 , y1 , data[10] , 14 );
    S5( y1 , y2 , y3 , y4 , y0 , data[ 3] , 12 );
    S5( y0 , y1 , y2 , y3 , y4 , data[12] ,  6 );

    S4( y4 , y0 , y1 , y2 , y3 , data[ 6] ,  9 );
    S4( y3 , y4 , y0 , y1 , y2 , data[11] , 13 );
    S4( y2 , y3 , y4 , y0 , y1 , data[ 3] , 15 );
    S4( y1 , y2 , y3 , y4 , y0 , data[ 7] ,  7 );
    S4( y0 , y1 , y2 , y3 , y4 , data[ 0] , 12 );
    S4( y4 , y0 , y1 , y2 , y3 , data[13] ,  8 );
    S4( y3 , y4 , y0 , y1 , y2 , data[ 5] ,  9 );
    S4( y2 , y3 , y4 , y0 , y1 , data[10] , 11 );
    S4( y1 , y2 , y3 , y4 , y0 , data[14] ,  7 );
    S4( y0 , y1 , y2 , y3 , y4 , data[15] ,  7 );
    S4( y4 , y0 , y1 , y2 , y3 , data[ 8] , 12 );
    S4( y3 , y4 , y0 , y1 , y2 , data[12] ,  7 );
    S4( y2 , y3 , y4 , y0 , y1 , data[ 4] ,  6 );
    S4( y1 , y2 , y3 , y4 , y0 , data[ 9] , 15 );
    S4( y0 , y1 , y2 , y3 , y4 , data[ 1] , 13 );
    S4( y4 , y0 , y1 , y2 , y3 , data[ 2] , 11 );

    S3( y3 , y4 , y0 , y1 , y2 , data[15] ,  9 );
    S3( y2 , y3 , y4 , y0 , y1 , data[ 5] ,  7 );
    S3( y1 , y2 , y3 , y4 , y0 , data[ 1] , 15 );
    S3( y0 , y1 , y2 , y3 , y4 , data[ 3] , 11 );
    S3( y4 , y0 , y1 , y2 , y3 , data[ 7] ,  8 );
    S3( y3 , y4 , y0 , y1 , y2 , data[14] ,  6 );
    S3( y2 , y3 , y4 , y0 , y1 , data[ 6] ,  6 );
    S3( y1 , y2 , y3 , y4 , y0 , data[ 9] , 14 );
    S3( y0 , y1 , y2 , y3 , y4 , data[11] , 12 );
    S3( y4 , y0 , y1 , y2 , y3 , data[ 8] , 13 );
    S3( y3 , y4 , y0 , y1 , y2 , data[12] ,  5 );
    S3( y2 , y3 , y4 , y0 , y1 , data[ 2] , 14 );
    S3( y1 , y2 , y3 , y4 , y0 , data[10] , 13 );
    S3( y0 , y1 , y2 , y3 , y4 , data[ 0] , 13 );
    S3( y4 , y0 , y1 , y2 , y3 , data[ 4] ,  7 );
    S3( y3 , y4 , y0 , y1 , y2 , data[13] ,  5 );

    S2( y2 , y3 , y4 , y0 , y1 , data[ 8] , 15 );
    S2( y1 , y2 , y3 , y4 , y0 , data[ 6] ,  5 );
    S2( y0 , y1 , y2 , y3 , y4 , data[ 4] ,  8 );
    S2( y4 , y0 , y1 , y2 , y3 , data[ 1] , 11 );
    S2( y3 , y4 , y0 , y1 , y2 , data[ 3] , 14 );
    S2( y2 , y3 , y4 , y0 , y1 , data[11] , 14 );
    S2( y1 , y2 , y3 , y4 , y0 , data[15] ,  6 );
    S2( y0 , y1 , y2 , y3 , y4 , data[ 0] , 14 );
    S2( y4 , y0 , y1 , y2 , y3 , data[ 5] ,  6 );
    S2( y3 , y4 , y0 , y1 , y2 , data[12] ,  9 );
    S2( y2 , y3 , y4 , y0 , y1 , data[ 2] , 12 );
    S2( y1 , y2 , y3 , y4 , y0 , data[13] ,  9 );
    S2( y0 , y1 , y2 , y3 , y4 , data[ 9] , 12 );
    S2( y4 , y0 , y1 , y2 , y3 , data[ 7] ,  5 );
    S2( y3 , y4 , y0 , y1 , y2 , data[10] , 15 );
    S2( y2 , y3 , y4 , y0 , y1 , data[14] ,  8 );

    S1( y1 , y2 , y3 , y4 , y0 , data[12] ,  8 );
    S1( y0 , y1 , y2 , y3 , y4 , data[15] ,  5 );
    S1( y4 , y0 , y1 , y2 , y3 , data[10] , 12 );
    S1( y3 , y4 , y0 , y1 , y2 , data[ 4] ,  9 );
    S1( y2 , y3 , y4 , y0 , y1 , data[ 1] , 12 );
    S1( y1 , y2 , y3 , y4 , y0 , data[ 5] ,  5 );
    S1( y0 , y1 , y2 , y3 , y4 , data[ 8] , 14 );
    S1( y4 , y0 , y1 , y2 , y3 , data[ 7] ,  6 );
    S1( y3 , y4 , y0 , y1 , y2 , data[ 6] ,  8 );
    S1( y2 , y3 , y4 , y0 , y1 , data[ 2] , 13 );
    S1( y1 , y2 , y3 , y4 , y0 , data[13] ,  6 );
    S1( y0 , y1 , y2 , y3 , y4 , data[14] ,  5 );
    S1( y4 , y0 , y1 , y2 , y3 , data[ 0] , 15 );
    S1( y3 , y4 , y0 , y1 , y2 , data[ 3] , 13 );
    S1( y2 , y3 , y4 , y0 , y1 , data[ 9] , 11 );
    S1( y1 , y2 , y3 , y4 , y0 , data[11] , 11 );

    y3 += x2 + state[1];
    state[1] = state[2] + x3 + y4;
    state[2] = state[3] + x4 + y0;
    state[3] = state[4] + x0 + y1;
    state[4] = state[0] + x1 + y2;
    state[0] = y3;

    return RMD160_OK;
}

#undef FUNCTION_NAME

#define FUNCTION_NAME "rmd160_append"

static int
rmd160_append (
	RMD160			arg_obj
,
	size_t			arg_len
,
	const unsigned char *	arg_data
)
{
    size_t		alen	;

    rmd160uint32 *	wend	;
    rmd160uint32 *	wptr	;

    unsigned int	bpos	;


    if ( ! arg_obj ) return RMD160_ERR_INVALID_ARG;

    if ( arg_len == 0 ) return RMD160_OK;
    if ( ! arg_data ) return RMD160_ERR_INVALID_ARG;

    alen = arg_len;
    wend = arg_obj->wend;
    wptr = arg_obj->wptr;
    bpos = arg_obj->bpos;

    if ( bpos ) {
	register rmd160uint32 w;
	w = * wptr;
	if ( bpos == 1 ) {
	    w |= ( (rmd160uint32) ( 0xff & * ( arg_data ++ ) ) ) << 8;
	    -- alen;
	    ++ bpos;
	}
	if ( bpos == 2 && alen ) {
	    w |= ( (rmd160uint32) ( 0xff & * ( arg_data ++ ) ) ) << 16;
	    -- alen;
	    ++ bpos;
	}
	if ( bpos == 3 && alen ) {
	    w |= ( (rmd160uint32) ( 0xff & * ( arg_data ++ ) ) ) << 24;
	    -- alen;
	    ++ bpos;
	}
	* wptr = w;
	if ( ! alen ) {
	    arg_obj->wptr = wptr;
	    arg_obj->bpos = bpos;
	    if ( ( arg_obj->len[0] = arg_len + arg_obj->len[0] ) < arg_len ) {
		arg_obj->len[1] ++;
	    }
	    return RMD160_OK;
	}
	bpos = 0;
	++ wptr;
    }

    for (;;) {
	while ( alen >= 4 && wptr < wend ) {

#if defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && __BYTE_ORDER == __LITTLE_ENDIAN
	    * wptr = * (const rmd160uint32 *) arg_data;
#else
	    * wptr =
		  ( (rmd160uint32) ( 0xff & arg_data[ 0 ] ) )       |
		  ( (rmd160uint32) ( 0xff & arg_data[ 1 ] ) ) <<  8 |
		  ( (rmd160uint32) ( 0xff & arg_data[ 2 ] ) ) << 16 |
		  ( (rmd160uint32) ( 0xff & arg_data[ 3 ] ) ) << 24;
#endif

	    ++ wptr;
	    arg_data += 4;
	    alen -= 4;
	}
	if ( wptr < wend ) break;
	_rmd160_calc( arg_obj->state , arg_obj->data );
	wptr = arg_obj->data;
    }

    if ( alen ) {
	rmd160uint32 w;
	w = ( (rmd160uint32) ( 0xff & * ( arg_data ++ ) ) );
	if ( alen >= 2 ) {
	    w |= ( (rmd160uint32) ( 0xff & * ( arg_data ++ ) ) ) << 8;
	    if ( alen >= 3 ) {
		w |= ( (rmd160uint32) ( 0xff & * ( arg_data ++ ) ) ) << 16;
	    }
	}
	bpos = alen;
	* wptr = w;
    }

    arg_obj->wptr = wptr;
    arg_obj->bpos = bpos;
    if ( ( arg_obj->len[0] = arg_len + arg_obj->len[0] ) < arg_len ) {
	arg_obj->len[1] ++;
    }
    return RMD160_OK;

}

#undef FUNCTION_NAME

#define FUNCTION_NAME "rmd160_destroy"

static int
rmd160_destroy (
	RMD160		ptr
)
{
    if ( ! ptr ) {
	return RMD160_ERR_INVALID_ARG;
    }
    free( ptr );
    return RMD160_OK;
}

#undef FUNCTION_NAME

#define FUNCTION_NAME "rmd160_copy"

static RMD160
rmd160_copy (
        RMD160          target_p
,
        RMD160          source_p
)
{
    if ( ! target_p ) {
        if ( ! ( target_p = (struct rmd160_object*)malloc( sizeof (struct rmd160_object) ) ) ) {
            return NULL;
        }
    }

    if ( source_p ) {
        target_p->state[ 0 ] = source_p->state[ 0 ];
        target_p->state[ 1 ] = source_p->state[ 1 ];
        target_p->state[ 2 ] = source_p->state[ 2 ];
        target_p->state[ 3 ] = source_p->state[ 3 ];
        target_p->state[ 4 ] = source_p->state[ 4 ];
        {
            int i;
            for ( i = 0 ; i < 16 ; ++ i ) {
                target_p->data[ i ] = source_p->data[ i ];
            }
        }
        target_p->len[ 0 ] = source_p->len[ 0 ];
        target_p->len[ 1 ] = source_p->len[ 1 ];
        target_p->bpos = source_p->bpos;
        target_p->wptr = source_p->wptr - source_p->data + target_p->data;
        target_p->wend = 16 + target_p->data;
    }
    else {
        target_p->state[ 0 ] = RMD160_INIT0;
        target_p->state[ 1 ] = RMD160_INIT1;
        target_p->state[ 2 ] = RMD160_INIT2;
        target_p->state[ 3 ] = RMD160_INIT3;
        target_p->state[ 4 ] = RMD160_INIT4;
        {
            int i;
            for ( i = 0 ; i < 16 ; ++ i ) {
                target_p->data[ i ] = 0U;
            }
        }
        target_p->len[ 0 ] = 0U;
        target_p->len[ 1 ] = 0U;
        target_p->bpos = 0;
        target_p->wptr = target_p->data;
        target_p->wend = 16 + target_p->data;
   }

    return target_p;
}

#undef FUNCTION_NAME



#define FUNCTION_NAME "rmd160_sum_words"

static rmd160uint32 *
rmd160_sum_words (
        RMD160          arg_handle
,
        rmd160uint32 *  arg_result_p
)
{
    struct rmd160_object        work    ;


    if ( ! arg_handle ) return NULL;

    if ( ! arg_result_p && ! ( arg_result_p = (rmd160uint32*)malloc( 5 * sizeof (rmd160uint32) ) ) ) {
        return NULL;
    }

    rmd160_copy( & work , arg_handle );

    {
        rmd160uint32 * p;
        p = work.wptr;
        if ( work.bpos ) ++ p;
        while ( p < work.wend ) * ( p ++ ) = 0U;
    }
    * ( work.wptr ) |= ( (rmd160uint32) 0x80 ) << ( work.bpos << 3 );

    if ( ( work.wend - work.wptr ) <= 2 ) {
        _rmd160_calc( work.state , work.data );

        memset( work.data , 0U , 14 * sizeof (rmd160uint32) );
    }

    work.data[14] = work.len[0] << 3;
    work.data[15] = ( work.len[1] << 3 ) | ( work.len[0] >> 29 );

    _rmd160_calc( work.state , work.data );

    memcpy( arg_result_p , work.state , 5 * sizeof (rmd160uint32) );

    return arg_result_p;

}

#undef FUNCTION_NAME



#define FUNCTION_NAME "rmd160_sum_bytes"

static unsigned char *
rmd160_sum_bytes (
        RMD160                  arg_handle
,
        unsigned char *         arg_result_p
)
{
    rmd160uint32        temp    [5]     ;

    rmd160uint32 *      ptemp           ;

    unsigned char *     result_p        ;


    if ( ! ( result_p = arg_result_p ) ) {
        if ( ! ( result_p = (unsigned char*)malloc( 20 ) ) ) return NULL;
    }

    if ( ! rmd160_sum_words( arg_handle , temp ) ) {
        if ( ! arg_result_p ) free( result_p );
        return NULL;
    }

    ptemp = temp;
    {
        int i;
        for ( i = 0 ; i < 5 ; ++ i ) {
            register rmd160uint32 w;
            * ( arg_result_p ++ ) = 0xff & ( w = * ptemp );
            * ( arg_result_p ++ ) = 0xff & ( w >>  8 );
            * ( arg_result_p ++ ) = 0xff & ( w >> 16 );
            * ( arg_result_p ++ ) = 0xff & ( w >> 24 );
            ++ ptemp;
        }
    }

    return arg_result_p;

}

#undef FUNCTION_NAME


static struct EXTRACTOR_Keywords * addKeyword(EXTRACTOR_KeywordList *oldhead,
					      const char *phrase,
					      EXTRACTOR_KeywordType type) {

   EXTRACTOR_KeywordList * keyword;
   keyword = (EXTRACTOR_KeywordList*) malloc(sizeof(EXTRACTOR_KeywordList));
   keyword->next = oldhead;
   keyword->keyword = strdup(phrase);
   keyword->keywordType = type;
   return keyword;
}

#define DIGEST_BITS 160
#define DIGEST_HEX_BYTES (DIGEST_BITS / 4)
#define DIGEST_BIN_BYTES (DIGEST_BITS / 8)
#define MAX_DIGEST_BIN_BYTES DIGEST_BIN_BYTES
#define rmd160_init(t) rmd160_copy((t),NULL)
#define rmd160_new() rmd160_copy(NULL,NULL)


struct EXTRACTOR_Keywords * libextractor_hash_rmd160_extract(const char * filename,
							     char * data,
							     size_t size,
							     struct EXTRACTOR_Keywords * prev) {
  unsigned char bin_buffer[MAX_DIGEST_BIN_BYTES];
  char hash[8 * MAX_DIGEST_BIN_BYTES];
  char buf[16];
  RMD160 ptr;
  int i;

  ptr = rmd160_new();
  rmd160_append(ptr, size, data);
  rmd160_sum_bytes(ptr, bin_buffer);
  rmd160_destroy(ptr);
  hash[0] = '\0';
  for (i=0;i<DIGEST_HEX_BYTES / 2; i++) {
    snprintf(buf,
	     16,
	     "%02x",
	     bin_buffer[i]);
    strcat(hash, buf);
  }
  prev = addKeyword(prev,
		    hash,
		    EXTRACTOR_HASH_RMD160);
  return prev;
}
