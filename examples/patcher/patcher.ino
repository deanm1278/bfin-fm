/* 8 Voices, 6 operators.
 * For use with MIDI MAX MSP patcher
 */

#include "MIDI.h"
#include "adau17x1.h"
#include "audioFX.h"
#include "fm.h"
#include "midi_notes.h"
#include "delay.h"
#include "filter_coeffs.h"

using namespace FX;

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1,  MIDI);

static RAMB q31 outputDataL[AUDIO_BUFSIZE], scratch[AUDIO_BUFSIZE], scratch2[AUDIO_BUFSIZE];
static uint32_t roundRobin = 0;

LFO<q31> lfoVol( _F16(2.0) );
LFO<q31> lfoVol2( _F16(2.5) );

#define CC_SCALE 16892410

adau17x1 iface;

void handleNoteOn(byte channel, byte pitch, byte velocity);
void handleNoteOff(byte channel, byte pitch, byte velocity);
void handleCC(byte channel, byte number, byte value);
void audioHook(q31 *data);

//********** SYNTH SETUP *****************//

#define NUM_OPERATORS 6
Operator op1(0), op2(1), op3(2), op4(3), op5(4), op6(5);
Operator *ops[NUM_OPERATORS] = {&op1, &op2, &op3, &op4, &op5, &op6};
Algorithm A(ops, NUM_OPERATORS);
bool isFixed[NUM_OPERATORS] = { false, false, false, false, false, false };

#define NUM_VOICES 8
#define VOICE_GAIN _F(.25)
Voice 	v1(&A, VOICE_GAIN), v2(&A, VOICE_GAIN),
		v3(&A, VOICE_GAIN), v4(&A, VOICE_GAIN),
		v5(&A, VOICE_GAIN), v6(&A, VOICE_GAIN),
		v7(&A, VOICE_GAIN), v8(&A, VOICE_GAIN);

Voice *voices[] = {&v1, &v2, &v3, &v4, &v5, &v6, &v7, &v8};

RatioFrequency ratios[] = {
		RatioFrequency(&op1, _F16(1.0)),
		RatioFrequency(&op1, _F16(1.0)),
		RatioFrequency(&op1, _F16(1.0)),
		RatioFrequency(&op1, _F16(1.0)),
		RatioFrequency(&op1, _F16(1.0)),
};

FixedFrequency fixed[] = {
		FixedFrequency(_F16(158.5)),
		FixedFrequency(_F16(158.5)),
		FixedFrequency(_F16(158.5)),
		FixedFrequency(_F16(158.5)),
		FixedFrequency(_F16(158.5)),
};

byte coarseVals[] = {64,64,64,64,64,64};
byte fineVals[] = {64,64,64,64,64,64};

float transposeSteps[] = {
		0,
		RATIO_MIN_SECOND,
		RATIO_MAJ_SECOND,
		RATIO_MIN_THIRD,
		RATIO_MAJ_THIRD,
		RATIO_FOURTH,
		RATIO_DIM_FIFTH,
		RATIO_FIFTH,
		RATIO_MIN_SIXTH,
		RATIO_MAJ_SIXTH,
		RATIO_MIN_SEVENTH,
		RATIO_MAJ_SEVENTH
};

//********** FILTER *****************//
RAMB q31 bqData[NUM_VOICES*2][BIQUAD_SIZE];
struct biquad *bq[NUM_VOICES*2];
q31 filterLasts[NUM_VOICES];

//the filter parameters
int Q;
bool filterOn = true;

Envelope<q31> filterEnv;

//********** DELAY *****************//
#define DELAY_SIZE (AUDIO_BUFSIZE*128)
static L2DATA q31 delayBuf[DELAY_SIZE];
struct delayLine *line;
struct delayTap *tap;

q31 delayMix = 0, delayFeedback = 0;

void chooseAlgo(uint8_t num);

void setup()
{
  chooseAlgo(32);

  //initialize voice filters
  for(int i=0; i<NUM_VOICES*2; i++)
	  bq[i] = initBiquad(bqData[i]);

  line = initDelayLine(delayBuf, DELAY_SIZE);
  tap = initDelayTap(line, 0);

  for(int i=1; i<NUM_OPERATORS; i++)
	  ops[i]->setCarrier(&ratios[i-1]);

  Serial.begin(115200);
#if 0
  q31 fakeData[AUDIO_BUFSIZE*2];
  PROFILE("fm test", {
		  audioHook(fakeData);
  })
#endif

  iface.begin();

  //begin fx processor
  fx.begin();

  //set the function to be called when a buffer is ready
  fx.setHook(audioHook);

  //set callbacks
  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleNoteOff(handleNoteOff);
  MIDI.setHandleControlChange(handleCC);

  // Initiate MIDI communications, listen to all channels
  MIDI.begin(MIDI_CHANNEL_OMNI);
}

void handleNoteOn(byte channel, byte pitch, byte velocity)
{
  for(int i=roundRobin; i<roundRobin+NUM_VOICES; i++){
  int v = i%NUM_VOICES;
    if(!voices[v]->active){
      voices[v]->setOutput(midi_notes[pitch]);
      voices[v]->trigger(true);
      voices[v]->velocity = (q31)velocity * CC_SCALE;
      roundRobin++;
      return;
    }
  }

  //look for an interruptable voice (one where all envelopes are passed first decay stage)
  for(int i=roundRobin; i<roundRobin+NUM_VOICES; i++){
    int v = i%NUM_VOICES;
      if(voices[v]->interruptable){
        voices[v]->setOutput(midi_notes[pitch]);
        voices[v]->trigger(true);
        voices[v]->velocity = (q31)velocity * CC_SCALE;
        roundRobin++;
        return;
      }
    }
}

void handleNoteOff(byte channel, byte pitch, byte velocity)
{
  for(int i=0; i<NUM_VOICES; i++){
    //look for the note that was played
    if(voices[i]->active && !voices[i]->queueStop && voices[i]->output == midi_notes[pitch]){
      voices[i]->trigger(false, true);
      break;
    }
  }
}

q16 midiToPitch(uint8_t ix){
	q16 ret = 0;
	if(isFixed[ix]){

	}
	else{
		int octave = (((int)coarseVals[ix]-64)/12);
		int8_t offset = ((int)coarseVals[ix]-64)%12;

		q16 cents = _F16(powf(2.0, ((float)((int)fineVals[ix]-64)/1200.)));

		float transpose = transposeSteps[abs(offset)];

		if(coarseVals[ix] >= 64)
			ret = _F16((float)(abs(octave) + 1) + transpose);
		else
			ret = _F16(1.0/((float)(abs(octave) + 1) + transpose));

		ret = _mult_q16_single(ret, cents);
	}
	return ret;
}

void handleCC(byte channel, byte number, byte value)
{
  //Serial.print(number); Serial.print(", "); Serial.println(value);
  if(number >= 10 && number < 100){
	  uint8_t op = floor( (number - 10) /15);
	  uint8_t offset = (number - 10) % 15;

	  switch(offset){
	  case 0:
		  coarseVals[op] = value;
		  ratios[op-1].ratio = midiToPitch(op);
		  break;
	  case 1:
		  fineVals[op] = value;
		  ratios[op-1].ratio = midiToPitch(op);
		  break;
	  case 2:
		  ops[op]->feedbackLevel = (q31)value * CC_SCALE;
		  break;
	  case 3:
		  //fixed vs. ratio
		  break;
	  case 4:
		  ops[op]->volume.attack.time = (uint16_t)value * (uint16_t)value;
		  break;
	  case 5:
		  ops[op]->volume.attack.level = (q31)value * CC_SCALE;
		  break;
	  case 6:
		  ops[op]->volume.decay.time =  (uint16_t)value * (uint16_t)value;
		  break;
	  case 7:
		  ops[op]->volume.decay.level = (q31)value * CC_SCALE;
		  break;
	  case 8:
		  ops[op]->volume.decay2.time = (uint16_t)value * (uint16_t)value;
		  break;
	  case 9:
		  ops[op]->volume.sustain.level = (q31)value * CC_SCALE;
		  break;
	  case 10:
		  ops[op]->volume.release.time =  (uint16_t)value * (uint16_t)value;
		  break;
	  case 11:
		  ops[op]->volume.release.level = (q31)value * CC_SCALE;
		  break;
	  case 12:
		  ops[op]->velSense = (q31)value * CC_SCALE;
		  break;
	  default:
		  break;
	  }
  }
  else{
	  switch(number){
		  case 2:
			  chooseAlgo(value);
			  break;

		  case 3:
			  lfoVol.rate = (q16)value * 2048;
			  break;

		  case 4:
			  lfoVol2.rate = (q16)value * 2048;
			  break;

		  case 5:
			  lfoVol.depth = (q31)value * CC_SCALE;
			  lfoVol2.depth = (q31)value * CC_SCALE;
			  break;

		  case 6:
			  Q = constrain(value, 0, 15);
			  break;

		  case 7:
			  _delay_move(tap, (127 - (int)value) * AUDIO_BUFSIZE);
			  break;

		  case 8:
			  delayFeedback = (q31)value * CC_SCALE;
			  break;

		  case 9:
			  delayMix = (q31)value * CC_SCALE;
			  break;

		  case 100:
			  filterEnv.attack.time = (uint16_t)value * (uint16_t)value;
			  break;
		  case 101:
			  filterEnv.attack.level = (q31)value * CC_SCALE;
			  break;
		  case 102:
			  filterEnv.decay.time =  (uint16_t)value * (uint16_t)value;
			  break;
		  case 103:
			  filterEnv.decay.level = (q31)value * CC_SCALE;
			  break;
		  case 104:
			  filterEnv.decay2.time = (uint16_t)value * (uint16_t)value;
			  break;
		  case 105:
			  filterEnv.sustain.level = (q31)value * CC_SCALE;
			  break;
		  case 106:
			  filterEnv.release.time =  (uint16_t)value * (uint16_t)value;
			  break;
		  case 107:
			  filterEnv.release.level = (q31)value * CC_SCALE;
			  break;
		  case 108:
			  filterOn = (value > 0);
			  break;

		  default:
			  break;
	  }
  }
}


void audioHook(q31 *data)
{
  zero(outputDataL);

  q31 tmpVoice[AUDIO_BUFSIZE], envData[AUDIO_BUFSIZE];
  for(int i=0; i<NUM_VOICES; i++){
	zero(tmpVoice);
	filterEnv.getOutput(envData, voices[i], filterLasts[i]);
	filterLasts[i] = envData[AUDIO_BUFSIZE-1];

	if(filterOn) voices[i]->play(tmpVoice);
	else voices[i]->play(outputDataL);

    uint8_t cut = envData[0] >> 23;
	setBiquadCoeffs(bq[i*2], lp_coeffs[Q][cut]);
    setBiquadCoeffs(bq[i*2+1], lp_coeffs[Q][cut]);

    //process both biquad filters
    biquadProcess(bq[i*2], tmpVoice, tmpVoice);
    biquadProcess(bq[i*2+1], tmpVoice, tmpVoice);

    sum(outputDataL, tmpVoice);
  }

  for(int i=0; i<AUDIO_BUFSIZE; i++) outputDataL[i] >>= 7;

  delayPop(tap, scratch);
  copy(scratch2, outputDataL);
  mix(scratch2, scratch, delayFeedback);
  delayPush(line, scratch2);

  mix(outputDataL, scratch, delayMix);

  limit24(outputDataL);
  copy(scratch, outputDataL);

  lfoVol.getOutput(outputDataL);
  lfoVol2.getOutput(scratch);

  interleave(data, outputDataL, scratch);
}

void loop()
{
    MIDI.read();
    __asm__ volatile("IDLE;");
}