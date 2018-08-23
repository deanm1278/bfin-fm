#ifndef __ASSEMBLY__
#define __ASSEMBLY__
#endif

#include <bf706_device.h>
#include "audioFX_config.h"

.extern __fm_sine;
.type __fm_sine,STT_OBJECT;

/* This is used if the operator uses feedback
 *
 * void _fm_modulate_feedback(_operator *op, q15 *buf, q15 feedbackLevel);
 */
.align 2
.global __fm_modulate_feedback;
.type __fm_modulate_feedback, STT_FUNC;
__fm_modulate_feedback:
[--SP] = ( R7:2, P5:2 );

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
	P4 += 8;
	R7 = [P4]; //_operator->volume
	I3 = R7;
	B3 = R7;
	L3 = 0;
	M3 = 2;
	P4 += 4;
	R0 = [P4]; //_operator->err
	P4 += 4;
	R6 = W[P4] (X); //_operator->lastFeedback

	R7.H = __fm_sine;
	R7.L = __fm_sine;
	B1 = R7;
	L1 = 8192; //sizeof(__fm_sine)

	P3 = R1; //buf
	P2 = 2 (X);

	[--SP] = R2; //feedbackLevel

	R7 = FM_LOOKUP_INC;

	R4 = AUDIO_BUFSIZE;
	R5 = FM_LOOKUP_MOD;
	LC0 = R4;
	LOOP fmFeedbackLoop LC0;
	LOOP_BEGIN fmFeedbackLoop;
		R1 = [I2++M2];
		(R3:2) = ((A1:0) = R7 * R1) (TFU);

		R2 = PACK(R3.L, R2.H);

		R2 = R2 + R0; //add previous error
		R0 = R2 & R5;
		R2 >>= 19; //shift out the error
		R2 <<= 1;

		M1 = R2;
		I1 += M1;

		R1 = [SP];
		R2.L = R6.L * R1.L;
		R2 = R2.L (X);
		R2 >>>= 4;

		//get sign bit
		R3 = 0x80000000;
		R3 = R3 & R2;
		R2 = R2 << 19;
		BITCLR(R2, 31); //clear sign bit
		R4 = R2 | R3; //add in old sign bit
		R4 >>>= 18; // % N

		M1 = R4;
		I1 += M1;
		R4 = [I1];
		R4 = R4.L (X);
		I1 -= M1 || R1 = [I3++M3];

		//moving average to get rid of oscillations
		R4 >>>= 1;
		R2 = R6 >>> 1;
		R4.L = R4.L + R2.L (S);
		R6 = R4.L (X); //new lastFeedback

		//multiply by volume
		R4.L = R4.L * R1.L || R1 = [P3];

		//add to output
		R4.L = R4.L + R1.L (S);

		W[P3++P2] = R4.L;

	LOOP_END fmFeedbackLoop;

	SP += 4;

	R7 = I1;
	[P5] = R7; //_operator->current
	P5 += 16;
	[P5] = R0; //_operator->err
	P5 += 4;
	W[P5] = R6.L; //_operator->lastFeedback

	( R7:2, P5:2 ) = [ SP ++ ];
	RTS;
