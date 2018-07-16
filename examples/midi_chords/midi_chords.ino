#include "MIDI.h"
#include "audioFX.h"
#include "fm.h"
#include "midi_notes.h"

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1,  MIDI);

volatile bool bufferReady;
q31 *dataPtr;

static RAMB q31 outputData[AUDIO_BUFSIZE];
static uint32_t roundRobin = 0;

void handleNoteOn(byte channel, byte pitch, byte velocity);
void handleNoteOff(byte channel, byte pitch, byte velocity);
void audioHook(q31 *data);

//********** SYNTH SETUP *****************//

#define NUM_OPERATORS 4
Operator op1, op2, op3, op4;
Operator *ops[NUM_OPERATORS] = {&op1, &op3, &op2, &op4};
Algorithm A(ops, NUM_OPERATORS);

#define NUM_VOICES 8
Voice v1(&A), v2(&A), v3(&A), v4(&A), v5(&A), v6(&A), v7(&A), v8(&A);
Voice *voices[NUM_VOICES] = {&v1, &v2, &v3, &v4, &v5, &v6, &v7, &v8};

RatioFrequency op2Ratio(&op1, _F16(3.01));
RatioFrequency op4Ratio(&op1, _F16(1.0));
RatioFrequency op3Ratio(&op1, _F16(2.01));

void setup()
{
   /******* DEFINE STRUCTURE *******
   *                        /\
   *                      OP4/
   *                       |
   *           OP1  OP2   OP3
   *              \  |   /
   *               output
   */
  op1.isOutput = true;
  op2.isOutput = true;
  op3.isOutput = true;

  op3.mods[0] = &op4;

  //******** DEFINE MODULATOR FREQUENCIES ********//
  op2.setCarrier(&op2Ratio);
  op3.setCarrier(&op3Ratio);
  op4.setCarrier(&op4Ratio);
  op4.feedbackLevel = _F(.4);

  //******* DEFINE ENVELOPES *******//

  //***** OP1 ENVELOPE *****//
  op1.volume.attack.time = 15;
  op1.volume.attack.level = _F(.5);

  op1.volume.decay.time = 200;
  op1.volume.decay.level = _F(.4);

  op1.volume.sustain.level = _F(.4);

  op1.volume.release.time = 25;

  //***** OP2 ENVELOPE *****//
  op2.volume.attack.time = 15;
  op2.volume.attack.level = _F(.5);

  op2.volume.decay.time = 200;
  op2.volume.decay.level = _F(.2);

  op2.volume.sustain.level = _F(.2);

  op2.volume.release.time = 25;

  //***** OP3 ENVELOPE *****//
  op3.volume.attack.time = 15;
  op3.volume.attack.level = _F(.45);

  op3.volume.decay.time = 200;
  op3.volume.decay.level = _F(.3);

  op3.volume.sustain.level = _F(.3);

  op3.volume.release.time = 25;

  //***** OP4 ENVELOPE *****//
  op4.volume.attack.time = 15;
  op4.volume.attack.level = _F(.5);

  op4.volume.decay.time = 150;
  op4.volume.decay.level = _F(.1);

  op4.volume.sustain.level = _F(.1);

  op4.volume.release.time = 25;

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
      voices[i]->play(outputData, _F(.99/(NUM_VOICES-4)));

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
