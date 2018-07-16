#ifndef __ASSEMBLY__
#define __ASSEMBLY__
#endif

#include <bf706_device.h>
#include "audioFX_config.h"

.align 2
.global __mult_q16;
.type __mult_q16, STT_FUNC;
__mult_q16:
	[--SP] = ( R7:2 );

	I0 = R0;
	M0 = 4;
	L0 = 0;
	B0 = R0;

	R7 = AUDIO_BUFSIZE;
	LC0 = R7;
	LOOP mult16Loop LC0;
	LOOP_BEGIN mult16Loop;
		R2 = [I0];

		(R5:R4) = ((A1:0) = R2 * R1) (TFU);
		R2 = PACK(R5.L, R4.H);

		[I0++M0] = R2;
	LOOP_END mult16Loop;

	( R7:2 ) = [ SP ++ ];
	RTS;

.align 2
.global __mult_q16_single;
.type __mult_q16_single, STT_FUNC;
__mult_q16_single:

	(R1:R0) = ((A1:0) = R0 * R1) (TFU);
	R0 = PACK(R1.L, R0.H);

	RTS;

.extern __fm_sine;
.type __fm_sine,STT_OBJECT;

/* This is used if the modulator is an output
 *
 * void _fm_modulate(_operator *op, q15 *buf);
 */
.align 2
.global __fm_modulate;
.type __fm_modulate, STT_FUNC;
__fm_modulate:
	[--SP] = ( R7:2, P5:3 );

	P5 = R0;
	P4 = R0; //_operator
	R7 = [P4]; //_operator->current
	I1 = R7;
	P4 += 4;
	R7 = [P4]; //_operator->cfreq
	I2 = R7; //cfreq
	B2 = R7;
	L2 = 0;
	M2 = 4;
	P4 += 4;
	R7 = [P4]; //_operator->mod
	I0 = R7;
	B0 = R7;
	L0 = 0;
	M0 = 2;
	P4 += 4;
	R7 = [P4]; //_operator->volume
	I3 = R7;
	B3 = R7;
	L3 = 0;
	M3 = 2;
	P4 += 4;
	R0 = [P4]; //_operator->err

	R7.H = __fm_sine;
	R7.L = __fm_sine;
	B1 = R7;
	L1 = 8192; //sizeof(__fm_sine)

	P3 = R1; //buf

	R7 = FM_LOOKUP_INC;

	R4 = AUDIO_BUFSIZE;
	R6 = 0x80000000;
	R5 = FM_LOOKUP_MOD;
	LC0 = R4;
	LOOP fmOutputLoop LC0;
	LOOP_BEGIN fmOutputLoop;
		R1 = [I2++M2];
		(R3:R2) = ((A1:0) = R7 * R1) (TFU);

		R2 = PACK(R3.L, R2.H) || R3 = [I0++M0];

		R2 = R2 + R0; //add previous error
		R0 = R2 & R5;
		R2 >>= 19; //shift out the error
		R2 <<= 1;

		M1 = R2;
		I1 += M1;

		R3 = R3.L (X); //sign extend mod
		R3 >>>= 1;

		//get sign bit
		R4 = R6 & R3;
		R3 = R3 << 19;
		BITCLR(R3, 31); //clear sign bit
		R4 = R3 | R4; //add in old sign bit
		R4 >>>= 18; // % N

		M1 = R4;
		I1 += M1;
		R4 = [I1];
		I1 -= M1 || R3 = [I3++M3];
		R4.L = R4.L * R3.L || R3 = [P3];

		//add to output
		R4.L = R4.L + R3.L (S);

		W[P3] = R4.L;
		P3 += 2;

	LOOP_END fmOutputLoop;

	R7 = I1;
	[P5] = R7; //_operator->current
	P5 += 16;
	[P5] = R0; //_operator->err

	( R7:2, P5:3 ) = [ SP ++ ];
	RTS;
