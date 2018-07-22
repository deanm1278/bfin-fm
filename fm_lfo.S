#ifndef __ASSEMBLY__
#define __ASSEMBLY__
#endif

#include <bf706_device.h>
#include "audioFX_config.h"

.extern __fm_sine;
.type __fm_sine,STT_OBJECT;

.extern __fm_sine_log;
.type __fm_sine_log,STT_OBJECT;

/*
 * void _lfo_q15(_lfo *l, q15 *buf);
 */
.align 2
.global __lfo_q15;
.type __lfo_q15, STT_FUNC;
__lfo_q15:
	[--SP] = ( R7:2, P5:2 );

	P3 = R1; //buf
	P2 = 2 (X);

	P5 = R0;
	P4 = R0; //_lfo
	R7 = [P4]; //_lfo->current
	I1 = R7;
	P4 += 4;
	R1 = [P4]; //_lfo->cfreq
	P4 += 4;
	R0 = [P4]; //_lfo->err
	P4 += 4;
	R6 = [P4]; //_lfo->depth

	R7.H = __fm_sine;
	R7.L = __fm_sine;
	B1 = R7;
	L1 = 8192; //sizeof(__fm_sine)

	R7 = FM_LOOKUP_INC;

	R4 = AUDIO_BUFSIZE;
	R5 = FM_LOOKUP_MOD;
	LC0 = R4;
	LOOP lfo15loop LC0;
	LOOP_BEGIN lfo15loop;
		(R3:R2) = ((A1:0) = R7 * R1) (TFU);

		R2 = PACK(R3.L, R2.H);

		R2 = R2 + R0; //add previous error
		R0 = R2 & R5;
		R2 >>= 19; //shift out the error
		R2 <<= 1;

		M1 = R2;
		R4 = [I1++M1];

		//compress to half the range
		R4 = R4.L (X);
		R4 >>>= 1;
		R3 = 16383;
		R4.L = R4.L + R3.L (S);

		//multiply by depth
		R4.L = R4.L * R6.L;

		W[P3++P2] = R4.L;

	LOOP_END lfo15loop;

	R7 = I1;
	[P5] = R7; //_lfo->current
	P5 += 8;
	[P5] = R0; //_lfo->err

	( R7:2, P5:2 ) = [ SP ++ ];
	RTS;

/*
 * int _lfo_q16(int lastPos, q16 *buf, q16 rate, q16 depth);
 */
.align 2
.global __lfo_q16;
.type __lfo_q16, STT_FUNC;
__lfo_q16:
	[--SP] = R3;
	R3 = [SP+16]; //depth

	[--SP] = ( R7:4, P5:0 );
	[--SP] = A0.x;
	[--SP] = A0.w;
	[--SP] = A1.x;
	[--SP] = A1.w;
	[--SP] = ASTAT;

	P0 = R1; //buf
	R1 = R2; //rate
	P4.H = __fm_sine_log;
	P4.L = __fm_sine_log;

	[--SP] = R3;

	R4 = 0x3FF;
	R6 = R0 & R4;
    R0 = R0 >> 10;

	R7 = AUDIO_BUFSIZE;
	LC0 = R7;
	LOOP lfo16loop LC0;
	LOOP_BEGIN lfo16loop;
		R7 = R1 << 16; //fraction
		R5 = R1 >> 16; //integer
		R4 = FM_LOOKUP_INC;
		R3 = R4 * R7 (FU);
		(R5:R4) = ((A1:0) = R4 * R5) (TFU);

		R2 = R4 + R3; // R2 = pi*freq*t

		R2 = R2 + R0;
		R3 = FM_LOOKUP_MOD;
		R0 = R2 & R3; //add some error bits and save the new error

		R4 = R2 >> 21;

		//add to plast, plast % 1024
		R4 = R4 + R6;
		R3 = 0x3FF;
		R4 = R4 & R3;
		R6 = R4; //save plast

		R4 <<= 2; //each is 4 bytes
		R5 = P4;
		R4 = R4 + R5;
		P5 = R4;
		R2 = [P5]; //val = y[pos0]

		//multiply by depth
		R3 = [SP];

		(R5:R4) = ((A1:0) = R3 * R2) (T);
		R5 <<= 16;
		R4 >>= 16;
		R2 = R5 | R4;
		R7 = 0x10000;
		R2 = R2 + R7;

		R3 = [P0];

		(R5:R4) = ((A1:0) = R3 * R2) (TFU);
		R5 <<= 16;
		R4 >>= 16;
		R4 = R5 | R4;

		[P0] = R4;
		P0 += 4;

	LOOP_END lfo16loop;

	R0 <<= 10;
    R0 = R0 | R6; //return plast and error

	SP+=4;

	ASTAT = [SP++];
	A1.w = [SP++];
	A1.x = [SP++];
	A0.w = [SP++];
	A0.x = [SP++];
	( R7:4, P5:0 ) = [ SP ++ ];
	R3 = [SP++];
	RTS;
