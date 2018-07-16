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
 * int _lfo_q31(int lastPos, q31 *buf, q16 rate, q31 depth);
 */
.align 2
.global __lfo_q31;
.type __lfo_q31, STT_FUNC;
__lfo_q31:
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
	P4.H = __fm_sine;
	P4.L = __fm_sine;

	[--SP] = R3;

	R4 = 0x3FF;
	R6 = R0 & R4;
    R0 = R0 >> 10;

	R7 = AUDIO_BUFSIZE;
	LC0 = R7;
	LOOP lfo31loop LC0;
	LOOP_BEGIN lfo31loop;
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

		R4 <<= 2; //each is 4 bytes long
		R5 = P4;
		R4 = R4 + R5;
		P5 = R4;
		R4 = [P5]; //val = y[pos0]

		//multiply by depth
		R2 = [SP];
		R4 = R4 * R2;

		//TODO: may be a better way to do this
		R3 = [P0];
		R5 = 0x7FFFFFFF;
		R2 = R5 - R2;
		R4 = R4 * R3;
		R3 = R3 * R2;
		R4 = R3 - R4 (S);
		[P0] = R4;
		P0 += 4;

	LOOP_END lfo31loop;

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
