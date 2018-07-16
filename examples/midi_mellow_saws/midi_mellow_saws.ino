/* 4 operators w/ feedback, 8 voice poly
 */

#include "MIDI.h"
#include "audioFX.h"
#include "fm.h"
#include "midi_notes.h"


MIDI_CREATE_INSTANCE(HardwareSerial, Serial1,  MIDI);

volatile bool bufferReady;
q31 *dataPtr;

static RAMB q31 outputData[AUDIO_BUFSIZE];
static uint32_t roundRobin = 0;

LFO<q16> lfo( _F16(6.0) );
#define LFO_DEPTH 0.01

#define ATTACK_LVL .49
#define SUSTAIN_LVL .4

#define DECAY_TIME 100

void handleNoteOn(byte channel, byte pitch, byte velocity);
void handleNoteOff(byte channel, byte pitch, byte velocity);
void handleCC(byte channel, byte number, byte value);
void audioHook(q31 *data);

//********** SYNTH SETUP *****************//

#define NUM_OPERATORS 4
Operator op1(0), op2(1), op3(2), op4(3);
Operator *ops[NUM_OPERATORS] = {&op1, &op3, &op2, &op4};
Algorithm A(ops, NUM_OPERATORS);

#define NUM_VOICES 8
Voice v1(&A), v2(&A), v3(&A), v4(&A), v5(&A), v6(&A), v7(&A), v8(&A);
Voice *voices[NUM_VOICES] = {&v1, &v2, &v3, &v4, &v5, &v6, &v7, &v8};

RatioFrequency op2Ratio(&op1, _F16(2.005));
RatioFrequency op3Ratio(&op1, _F16(2.006));
RatioFrequency op4Ratio(&op1, _F16(1.998));

void setup()
{
   /******* DEFINE STRUCTURE *******
   *           /\   /\     /\   /\
   *           \OP1 \OP2 OP3/ OP4/
   *               \  \  /   /
   *                  output
   */
  op1.isOutput = true;
  op2.isOutput = true;
  op3.isOutput = true;
  op4.isOutput = true;

  //******** DEFINE MODULATOR FREQUENCIES ********//
  op2.setCarrier(&op2Ratio);
  op3.setCarrier(&op3Ratio);
  op4.setCarrier(&op4Ratio);
  op1.feedbackLevel = _F(.999);
  op2.feedbackLevel = _F(.999);
  op3.feedbackLevel = _F(.999);
  op4.feedbackLevel = _F(.999);

  //******* DEFINE ENVELOPES *******//

  //***** OP1 ENVELOPE *****//
  op1.volume.attack.time = 0;
  op1.volume.attack.level = _F(ATTACK_LVL);

  op1.volume.decay.time = DECAY_TIME;
  op1.volume.decay.level = _F(SUSTAIN_LVL);

  op1.volume.sustain.level = _F(SUSTAIN_LVL);

  op1.volume.release.time = 25;

  //***** OP2 ENVELOPE *****//
  op2.volume.attack.time = 0;
  op2.volume.attack.level = _F(ATTACK_LVL);

  op2.volume.decay.time = DECAY_TIME;
  op2.volume.decay.level = _F(SUSTAIN_LVL);

  op2.volume.sustain.level = _F(SUSTAIN_LVL);

  op2.volume.release.time = 25;

  //***** OP3 ENVELOPE *****//
  op3.volume.attack.time = 0;
  op3.volume.attack.level = _F(ATTACK_LVL);

  op3.volume.decay.time = DECAY_TIME;
  op3.volume.decay.level = _F(SUSTAIN_LVL);

  op3.volume.sustain.level = _F(SUSTAIN_LVL);

  op3.volume.release.time = 25;

  //***** OP4 ENVELOPE *****//
  op4.volume.attack.time = 0;
  op4.volume.attack.level = _F(ATTACK_LVL);

  op4.volume.decay.time = DECAY_TIME;
  op4.volume.decay.level = _F(SUSTAIN_LVL);

  op4.volume.sustain.level = _F(SUSTAIN_LVL);

  op4.volume.release.time = 25;

  lfo.depth = _F16(LFO_DEPTH);

  //***** END OF PATCH SETUP *****//

  Serial.begin(9600);

  bufferReady = false;

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

    if(bufferReady){

    memset(outputData, 0, AUDIO_BUFSIZE*sizeof(q31));

    for(int i=0; i<NUM_VOICES; i++)
      voices[i]->play(outputData, _F(.99/(NUM_VOICES-4)), &lfo);

    //lfoVol.getOutput(outputData);

    q31 *ptr = outputData;
    for(int i=0; i<AUDIO_BUFSIZE; i++){
      *dataPtr++ = *ptr;
      *dataPtr++ = *ptr++;
    }

    bufferReady = false;
    }
    __asm__ volatile("IDLE;");
}

void handleNoteOn(byte channel, byte pitch, byte velocity)
{
  for(int i=roundRobin; i<roundRobin+NUM_VOICES; i++){
  int v = i%NUM_VOICES;
    if(!voices[v]->active){
      voices[v]->setOutput(midi_notes[pitch]);
      voices[v]->trigger(true);
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
      voices[i]->trigger(false, true);
      break;
    }
  }
}

void handleCC(byte channel, byte number, byte value)
{
  if(number == 1){
	  //map modwheel to feedback level
	  q31 lvl = (q31)value * 16892410;
	  op1.feedbackLevel = lvl;
	  op2.feedbackLevel = lvl;
	  op3.feedbackLevel = lvl;
	  op4.feedbackLevel = lvl;
  }
}

void audioHook(q31 *data)
{
  dataPtr = data;
  bufferReady = true;
}