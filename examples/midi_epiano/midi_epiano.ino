/* Sample stereo epiano patch
 * 8 voices, 6 operators
 */

#include "MIDI.h"
#include "adau17x1.h"
#include "audioFX.h"
#include "fm.h"
#include "midi_notes.h"

using namespace FX;

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1,  MIDI);

static RAMB q31 outputDataL[AUDIO_BUFSIZE], outputDataR[AUDIO_BUFSIZE];
static uint32_t roundRobin = 0;

LFO<q31> lfoVol( _F16(2.0) );
LFO<q31> lfoVol2( _F16(2.5) );
#define LFO_VOL_DEPTH .45

#define ATTACK_LVL .36
#define DECAY_TIME 50
#define DECAY_LVL .25
#define SUSTAIN_LVL 0

adau17x1 iface;

void handleNoteOn(byte channel, byte pitch, byte velocity);
void handleNoteOff(byte channel, byte pitch, byte velocity);
void handleCC(byte channel, byte number, byte value);
void audioHook(q31 *data);

//********** SYNTH SETUP *****************//

#define NUM_OPERATORS 6
Operator op1(0), op2(1), op3(2), op4(3), op5(4), op6(5);
Operator *ops[NUM_OPERATORS] = {&op1, &op3, &op2, &op4, &op5, &op6};
Algorithm A(ops, NUM_OPERATORS);

#define NUM_VOICES 8
Voice v1(&A), v2(&A), v3(&A), v4(&A), v5(&A), v6(&A), v7(&A), v8(&A);
Voice *voices[] = {&v1, &v2, &v3, &v4, &v5, &v6, &v7, &v8};

RatioFrequency op2Ratio(&op1, _F16(14.0));
RatioFrequency op4Ratio(&op1, _F16(.5));
FixedFrequency op5Freq(_F16(158.5));
RatioFrequency op6Ratio(&op1, _F16(1.003));

void setup()
{
  /******* DEFINE STRUCTURE *******
  *                     /\
  *           OP4 OP5 OP6/  OP2
  *             \  |  /      |
  *               OP3       OP1
  *                \_________/
  *                     |
  *                  output
  */

	op1.isOutput = true;
	op3.isOutput = true;
	//op6.isOutput = true;

	op1.mods[0] = &op2;

	op3.mods[0] = &op5;
	op3.mods[1] = &op4;
	op3.mods[2] = &op6;

  //******** DEFINE MODULATOR FREQUENCIES ********//
  op2.setCarrier(&op2Ratio);
  op4.setCarrier(&op4Ratio);
  op5.setCarrier(&op5Freq);
  op6.setCarrier(&op6Ratio);
  op6.feedbackLevel = _F(.35);

  //******* DEFINE ENVELOPES *******//

  //***** OP1 ENVELOPE *****//
  op1.volume.attack.time = 0;
  op1.volume.attack.level = _F(.05);

  op1.volume.decay.time = 200;
  op1.volume.decay.level = _F(.005);

  op1.volume.decay2.time = 800;

  op1.volume.sustain.level = _F(0);

  //***** OP2 ENVELOPE *****//
  op2.volume.attack.time = 0;
  op2.volume.attack.level = _F(.55);

  op2.volume.decay.time = 25;
  op2.volume.decay.level = _F(.25);

  op2.volume.decay2.time = 50;

  op2.volume.sustain.level = _F(0);

  //***** OP3 ENVELOPE *****//
  op3.volume.attack.time = 0;
  op3.volume.attack.level = _F(.95);

  op3.volume.decay.time = 200;
  op3.volume.decay.level = _F(.85);

  op3.volume.decay2.time = 800;

  op3.volume.sustain.level = _F(.25);

  op3.volume.release.time = 50;

  //***** OP4 ENVELOPE *****//
  op4.volume.attack.time = 0;
  op4.volume.attack.level = _F(.35);
  op4.volume.decay.time = 500;
  op4.volume.decay.level = _F(.2);

  op4.volume.decay2.time = 500;

  op4.volume.sustain.level = _F(.05);

  op4.volume.release.time = 50;

  //***** OP5 ENVELOPE *****//
  op5.volume.attack.time = 0;
  op5.volume.attack.level = _F(.35);

  op5.volume.decay.time = 8;
  op5.volume.decay.level = _F(.1);

  op5.volume.sustain.level = _F(0);

  //***** OP6 ENVELOPE *****//
  op6.volume.attack.time = 0;
  op6.volume.attack.level = _F(.35);
  op6.volume.decay.level = _F(.35);

  op6.volume.decay2.time = 800;

  op6.volume.sustain.level = _F(.1);

  op6.volume.release.time = 50;

  lfoVol.depth = _F(LFO_VOL_DEPTH);
  lfoVol2.depth = _F(LFO_VOL_DEPTH);

  //***** END OF PATCH SETUP *****//

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

void loop()
{
    // Call MIDI.read the fastest you can for real-time performance.
    MIDI.read();
    __asm__ volatile("IDLE;");
}

void handleNoteOn(byte channel, byte pitch, byte velocity)
{
  for(int i=roundRobin; i<roundRobin+NUM_VOICES; i++){
  int v = i%NUM_VOICES;
    if(!voices[v]->active){
      voices[v]->setOutput(midi_notes[pitch]);
      voices[v]->trigger(true);
      voices[v]->gain = (q31)velocity * (16892410/2);
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
        voices[v]->gain = (q31)velocity * (16892410/2);
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

void handleCC(byte channel, byte number, byte value)
{
  if(number == 1){
	  //map modwheel to feedback level
	  op6.feedbackLevel = (q31)value * 16892410;
  }
}


void audioHook(q31 *data)
{
  zero(outputDataL);

  for(int i=0; i<NUM_VOICES; i++)
    voices[i]->play(outputDataL);

  for(int i=0; i<AUDIO_BUFSIZE; i++) outputDataL[i] >>= 7;

  copy(outputDataR, outputDataL);

  lfoVol.getOutput(outputDataL);
  lfoVol2.getOutput(outputDataR);

  interleave(data, outputDataL, outputDataR);
}
