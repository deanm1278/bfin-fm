#ifndef __ASSEMBLY__
#define __ASSEMBLY__
#endif

#include <bf706_device.h>
#include "audioFX_config.h"

/*
 * void _envelope_interpolate(_envelope *e, q15 *buf, q31 roc);
 */
.align 2
.global __envelope_interpolate;
.type __envelope_interpolate, STT_FUNC;
__envelope_interpolate:
	[--SP] = ( R7:2, P5:2 );

	P5 = R0;
	P4 = R0; //_envelope
	R7 = [P4]; //_envelope->err
	P4 += 4;
	R6 = [P4]; //_envelope->lim

	P3 = R1; //buf
	P2 = 2 (X);

	R4 = AUDIO_ENVELOPE_SIZE;
	LC0 = R4;

	R5 = 0 (X);
	CC = R2 < R5;
	IF CC JUMP envdown;

envup:
	LOOP envuploop LC0;
	LOOP_BEGIN envuploop;
		R7 = R7 + R2 (S) || W[P3 ++ P2] = R7.H;
		R7 = MIN(R7, R6);
	LOOP_END envuploop;
	JUMP envend;

envdown:
	LOOP envdownloop LC0;
	LOOP_BEGIN envdownloop;
		R7 = R7 + R2 (S) || W[P3 ++ P2] = R7.H;
		R7 = MAX(R7, R6);
	LOOP_END envdownloop;

envend:
	[P5] = R7; //_operator->err

	( R7:2, P5:2 ) = [ SP ++ ];
	RTS;
