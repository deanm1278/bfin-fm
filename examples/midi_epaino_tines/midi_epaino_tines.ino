/* Sample epiano tines patch
 * 8 voices, 6 operators
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

LFO<q31> lfoVol( _F16(2.0) );
#define LFO_VOL_DEPTH 0.02

#define ATTACK_LVL .36
#define DECAY_TIME 50
#define DECAY_LVL .25
#define SUSTAIN_LVL 0

void handleNoteOn(byte channel, byte pitch, byte velocity);
void handleNoteOff(byte channel, byte pitch, byte velocity);
void audioHook(q31 *data);

//********** SYNTH SETUP *****************//

#define NUM_OPERATORS 6
Operator op1(0), op2(1), op3(2), op4(3), op5(4), op6(5);
Operator *ops[NUM_OPERATORS] = {&op1, &op3, &op2, &op4, &op5, &op6};
Algorithm A(ops, NUM_OPERATORS);

#define NUM_VOICES 8
Voice v1(&A), v2(&A), v3(&A), v4(&A), v5(&A), v6(&A), v7(&A), v8(&A);
Voice *voices[NUM_VOICES] = {&v1, &v2, &v3, &v4, &v5, &v6, &v7, &v8};

RatioFrequency op2Ratio(&op1, _F16(4.0));
RatioFrequency op3Ratio(&op1, _F16(2.0));
RatioFrequency op4Ratio(&op1, _F16(1.007));
RatioFrequency op5Ratio(&op1, _F16(2.007));
RatioFrequency op6Ratio(&op1, _F16(2.007));

void setup()
{
  /******* DEFINE STRUCTURE *******
  *                                 /\
  *           OP1 OP2 OP3 OP4 OP5 OP6/
  *            |   |   |   |   |   |
  *            --------------------
  *                      |
  *                   output
  */

  op1.isOutput = true;
  op2.isOutput = true;
  op3.isOutput = true;
  op4.isOutput = true;
  op5.isOutput = true;
  op6.isOutput = true;

  //******** DEFINE MODULATOR FREQUENCIES ********//
  op2.setCarrier(&op2Ratio);
  op3.setCarrier(&op3Ratio);
  op4.setCarrier(&op4Ratio);
  op5.setCarrier(&op5Ratio);
  op6.setCarrier(&op6Ratio);
  op6.feedbackLevel = _F(.4);

  //******* DEFINE ENVELOPES *******//

  //***** OP1 ENVELOPE *****//
  op1.volume.attack.time = 8;
  op1.volume.attack.level = _F(ATTACK_LVL);

  op1.volume.decay.time = DECAY_TIME;
  op1.volume.decay.level = _F(DECAY_LVL);

  op1.volume.decay2.time = 2000;

  op1.volume.sustain.level = _F(SUSTAIN_LVL);

  //***** OP2 ENVELOPE *****//
  op2.volume.attack.time = 8;
  op2.volume.attack.level = _F(ATTACK_LVL);

  op2.volume.decay.time = DECAY_TIME;
  op2.volume.decay.level = _F(DECAY_LVL);

  op2.volume.decay2.time = 2000;

  op2.volume.sustain.level = _F(SUSTAIN_LVL);

  //***** OP3 ENVELOPE *****//
  op3.volume.attack.time = 8;
  op3.volume.attack.level = _F(ATTACK_LVL);

  op3.volume.decay.time = DECAY_TIME;
  op3.volume.decay.level = _F(DECAY_LVL);

  op3.volume.decay2.time = 2000;

  op3.volume.sustain.level = _F(SUSTAIN_LVL);

  //***** OP4 ENVELOPE *****//
  op4.volume.attack.time = 8;
  op4.volume.attack.level = _F(ATTACK_LVL);

  op4.volume.decay.time = DECAY_TIME;
  op4.volume.decay.level = _F(DECAY_LVL);

  op4.volume.decay2.time = 2000;

  op4.volume.sustain.level = _F(SUSTAIN_LVL);

  //***** OP5 ENVELOPE *****//
  op5.volume.attack.time = 8;
  op5.volume.attack.level = _F(ATTACK_LVL);

  op5.volume.decay.time = DECAY_TIME;
  op5.volume.decay.level = _F(DECAY_LVL);

  op5.volume.decay2.time = 2000;

  op5.volume.sustain.level = _F(SUSTAIN_LVL);

  //***** OP6 ENVELOPE *****//
  op6.volume.attack.time = 8;
  op6.volume.attack.level = _F(ATTACK_LVL);

  op6.volume.decay.time = 100;
  op6.volume.decay.level = _F(.1);

  op6.volume.decay2.time = 1500;

  op6.volume.sustain.level = _F(SUSTAIN_LVL);

  lfoVol.depth = _F(LFO_VOL_DEPTH);

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
      voices[i]->play(outputData, _F(.99/(NUM_VOICES-4)));

    lfoVol.getOutput(outputData);

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
    if(voices[i]->active && !voices[i]->queueStop && voices[i]->output == midi_notes[pitch]){
      voices[i]->trigger(false);
      break;
    }
  }
}

void audioHook(q31 *data)
{
  dataPtr = data;
  bufferReady = true;
}