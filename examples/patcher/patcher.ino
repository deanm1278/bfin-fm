/* 8 Voices, 6 operators.
 * For use with MIDI MAX MSP patcher
 */

#include "MIDI.h"
#include "ak4558.h"
#include "audioFX.h"
#include "fm.h"
#include "midi_notes.h"
#include <Adafruit_NeoPixel.h>

using namespace FX;

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1,  MIDI);

extern q15 q15_lookup[];

static RAMB q31 outputDataL[AUDIO_BUFSIZE];
static RAMB q15 lfoData[AUDIO_BUFSIZE];
static uint32_t roundRobin = 0;

LFO<q15> amod;

#define CC_SCALE 16892410
#define CC_SCALE15 258

ak4558 iface;

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(2, PIN_NEOPIX, SPORT1, NEO_GRB + NEO_KHZ800);

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

#define NUM_VOICES 16
#define VOICE_GAIN _F(.35)
Voice 	v1(&A, VOICE_GAIN), v2(&A, VOICE_GAIN),
		v3(&A, VOICE_GAIN), v4(&A, VOICE_GAIN),
		v5(&A, VOICE_GAIN), v6(&A, VOICE_GAIN),
		v7(&A, VOICE_GAIN), v8(&A, VOICE_GAIN),
		v9(&A, VOICE_GAIN), v10(&A, VOICE_GAIN),
    v11(&A, VOICE_GAIN), v12(&A, VOICE_GAIN),
    v13(&A, VOICE_GAIN), v14(&A, VOICE_GAIN),
    v15(&A, VOICE_GAIN), v16(&A, VOICE_GAIN);

Voice *voices[] = {&v1, &v2, &v3, &v4, &v5, &v6, &v7, 
                  &v8, &v9, &v10, &v11, &v12, &v13, &v14, &v15, &v16};

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

byte coarseVals[] = {1,1,1,1,1,1};
byte fineVals[] = {64,64,64,64,64,64};

void chooseAlgo(uint8_t num);

void setup()
{
  
  chooseAlgo(32);

  for(int i=1; i<NUM_OPERATORS; i++)
	  ops[i]->setCarrier(&ratios[i-1]);
   
#if 0
  Serial.begin(9600);
  delay(5000);
  q31 fakeData[AUDIO_BUFSIZE*2];
  PROFILE("fm test", {
		  audioHook(fakeData);
  })
#endif
  pinMode(13, OUTPUT);

  pixels.begin();
  pixels.setPixelColor(0, pixels.Color(0,0,0));
  pixels.setPixelColor(1, pixels.Color(0,0,0));
  pixels.show();
  
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
  //digitalWrite(13, HIGH);
  //delay(50);
  //digitalWrite(13, LOW);
  for(int i=roundRobin; i<roundRobin+NUM_VOICES; i++){
  int v = i%NUM_VOICES;
    if(!voices[v]->active){
      voices[v]->setOutput(midi_notes[pitch]);
      voices[v]->trigger(true);
      voices[v]->velocity = q15_lookup[velocity];
      roundRobin++;
      return;
    }
  }
}

void handleNoteOff(byte channel, byte pitch, byte velocity)
{
  for(int i=0; i<NUM_VOICES; i++){
    //look for the note that was played
    if(voices[i]->active && voices[i]->output == midi_notes[pitch]){
      voices[i]->trigger(false);
      break;
    }
  }
}

q16 midiToPitch(uint8_t ix){
	q16 ret = 0;
	if(isFixed[ix]){

	}
	else{
		q16 cents = _F16(powf(2.0, ((float)((int)fineVals[ix]-64)/1200.)));

		if(coarseVals[ix] > 0)
			ret = _F16((float)coarseVals[ix]);
		else
			ret = _F16(.5);

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
		  ops[op]->feedbackLevel = q15_lookup[value];
		  break;
	  case 3:
      //fixed vs. ratio
      break;
    case 4:
      ops[op]->volume.setRate(ENVELOPE_ATTACK, q15_lookup[value] >> 2);
      break;
    case 5:
      ops[op]->volume.setLevel(ENVELOPE_ATTACK, q15_lookup[value]);
      break;
    case 6:
      ops[op]->volume.setRate(ENVELOPE_DECAY, q15_lookup[value] >> 2);
      break;
    case 7:
      ops[op]->volume.setLevel(ENVELOPE_DECAY, q15_lookup[value]);
      break;
    case 8:
      ops[op]->volume.setRate(ENVELOPE_SUSTAIN, q15_lookup[value] >> 2);
      break;
    case 9:
      ops[op]->volume.setLevel(ENVELOPE_SUSTAIN, q15_lookup[value]);
      break;
    case 10:
      ops[op]->volume.setRate(ENVELOPE_RELEASE, q15_lookup[value] >> 2);
      break;
    case 11:
      ops[op]->volume.setLevel(ENVELOPE_RELEASE, q15_lookup[value]);
      break;
    case 12:
      ops[op]->velSense = q15_lookup[value];
      break;
    case 13:
      ops[op]->amodSense = (q15)value * CC_SCALE15;
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
        amod.setRate(q15_lookup[value] << 6);
        break;

      case 4:
        amod.setDepth((q15)value * CC_SCALE15);
        break;

      default:
        break;
    }
  }
}


void audioHook(q31 *data)
{
  zero(outputDataL);
  amod.getOutput(lfoData);

  for(int i=0; i<NUM_VOICES; i++){
    voices[i]->play(outputDataL, lfoData);
  }
  interleave(data, outputDataL, outputDataL);
}

void loop()
{
    MIDI.read();
    __asm__ volatile("IDLE;");
}
