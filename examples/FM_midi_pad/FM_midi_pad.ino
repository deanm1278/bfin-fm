#include "MIDI.h"
#include "audioFX.h"
#include "fm.h"
#include "midi_notes.h"

#define NUM_OPERATORS 6
Operator op1, op2, op3, op4, op5, op6;
Operator *ops[6] = {&op1, &op3, &op2, &op4, &op5, &op6};
Algorithm A(ops, NUM_OPERATORS);

#define NUM_VOICES 6
Voice v1(&A), v2(&A), v3(&A), v4(&A), v5(&A), v6(&A);
Voice *voices[] = {&v1, &v2, &v3, &v4, &v5, &v6};

RatioFrequency op1Ratio(&op3, _F16(1.0 + RATIO_MAJ_THIRD));
RatioFrequency op2Ratio(&op1, _F16(3.0));

RatioFrequency op4Ratio(&op3, _F16(1.0));
RatioFrequency op5Ratio(&op3, _F16(1.5));
RatioFrequency op6Ratio(&op3, _F16(.5));

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1,  MIDI);

volatile bool bufferReady;
q31 *dataPtr;

static RAMB q31 outputData[AUDIO_BUFSIZE];
static uint32_t roundRobin = 0;

void handleNoteOn(byte channel, byte pitch, byte velocity)
{
  for(int i=roundRobin; i<roundRobin+NUM_VOICES; i++){
  int v = i%NUM_VOICES;
    if(!voices[v]->active){
      voices[v]->setOutput(midi_notes[pitch]);
      voices[v]->trigger(true);
      roundRobin++;
      break;
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

void audioHook(q31 *data)
{
  dataPtr = data;
  bufferReady = true;
}

// -----------------------------------------------------------------------------

void setup()
{
   /******* DEFINE STRUCTURE *******
   *                  OP6
   *                   |
   *                  OP5
   *                   |
   *           OP2    OP4
   *            |      |
   *           OP1    OP3
   *             \    /
   *             output
   */
  op1.isOutput = true;
  op3.isOutput = true;

  op1.mods[0] = &op2;
  op3.mods[0] = &op4;
  op4.mods[0] = &op5;
  op5.mods[0] = &op6;

  //******** DEFINE MODULATOR FREQUENCIES ********//
  op1.setCarrier(&op1Ratio);
  op2.setCarrier(&op2Ratio);
  op4.setCarrier(&op4Ratio);
  op5.setCarrier(&op5Ratio);
  op6.setCarrier(&op6Ratio);

  //******* DEFINE ENVELOPES *******//

  //***** OP1 ENVELOPE *****//
  op1.volume.attack.time = 50;
  op1.volume.attack.level = _F(.9);

  op1.volume.decay.time = 100;
  op1.volume.decay.level = _F(.7);

  op1.volume.sustain.level = _F(.7);

  op1.volume.release.time = 100;
  op1.volume.release.level = _F(0);

  //***** OP2 ENVELOPE *****//
  op2.volume.attack.time = 3000;
  op2.volume.attack.level = _F(.9);

  op2.volume.decay.time = 700;
  op2.volume.decay.level = _F(.4);

  op2.volume.sustain.level = _F(.4);

  op2.volume.release.time = 100;
  op2.volume.release.level = _F(0);

  //***** OP3 ENVELOPE *****//
  op3.volume.attack.time = 2000;
  op3.volume.attack.level = _F(.9);

  op3.volume.decay.time = 50;
  op3.volume.decay.level = _F(.7);

  op3.volume.sustain.level = _F(.7);

  op3.volume.release.time = 20;
  op3.volume.release.level = _F(0);

  //***** OP4 ENVELOPE *****//
  op4.volume.attack.time = 2000;
  op4.volume.attack.level = _F(.9);

  op4.volume.decay.time = 400;
  op4.volume.decay.level = _F(.2);

  op4.volume.sustain.level = _F(.2);

  op4.volume.release.time = 20;
  op4.volume.release.level = _F(0);

  //***** OP5 ENVELOPE *****//
  op5.volume.attack.time = 1000;
  op5.volume.attack.level = _F(.9);

  op5.volume.decay.time = 700;
  op5.volume.decay.level = _F(.6);

  op5.volume.sustain.level = _F(.6);

  op5.volume.release.time = 20;
  op5.volume.release.level = _F(0);

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
//PROFILE("main loop", {
    memset(outputData, 0, AUDIO_BUFSIZE*sizeof(q31));

    for(int i=0; i<NUM_VOICES; i++)
      voices[i]->play(outputData, _F(.7/NUM_VOICES));

    q31 *ptr = outputData;
    for(int i=0; i<AUDIO_BUFSIZE; i++){
    *dataPtr++ = *ptr;
    *dataPtr++ = *ptr++;
    }
    bufferReady = false;
//});
    }
    __asm__ volatile("IDLE;");
}
