#include <Arduino.h>
#include "MIDI.h"
#include "audioFX.h"
#include "fm.h"
#include "midi_notes.h"

//create a 3 operator algorithm
Operator op1, op2, op3;
Operator *ops[] = {&op1, &op3, &op2};
Algorithm A(ops, 3);

#define NUM_VOICES 4
Voice v1(&A), v2(&A), v3(&A), v4(&A);
Voice *voices[] = {&v1, &v2, &v3, &v4};

RatioFrequency op1Ratio(&op2, _F16(6));

//op3 will be a ratio frequency (8x the carrier freq of op2)
RatioFrequency op3Ratio(&op2, _F16(9));

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1,  MIDI);

volatile bool bufferReady;
q31 *dataPtr;

static RAMB q31 outputData[AUDIO_BUFSIZE];

void handleNoteOn(byte channel, byte pitch, byte velocity)
{
  for(int i=0; i<NUM_VOICES; i++){
    if(!voices[i]->active){
      voices[i]->setOutput(midi_notes[pitch]);
      voices[i]->trigger(true);
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
  Serial.begin(9600);
   /******* DEFINE STRUCTURE *******
   *   OP1     OP3
   *      \   /
   *       OP2
   *        |
   *      output
   */
  op2.isCarrier = true; // op2 is carrier frequency
  op2.mods[0] = &op3; // op3 modulates op2
  op2.mods[1] = &op1; // op1 modulates op2

  //******** DEFINE MODULATOR FREQUENCIES ********//
  op1.carrier = &op1Ratio; // op1 is a fixed frequency (set above)
  op3.carrier = &op3Ratio; // op3 is a ratio frequency (set above)

  //******* DEFINE ENVELOPES *******//

  //***** OP2 ENVELOPE *****//
  op2.volume.attack.time = 1; // 1ms attack time
  op2.volume.attack.level = _F(.999); // max attack level (99.9 %)

  op2.volume.decay.time = 300; // 300 ms decay
  op2.volume.decay.level = _F(0); // decay to zero

  op2.volume.sustain.level = _F(0); // zero sustain

  //***** OP1 ENVELOPE *****//
  op1.volume.sustain.level = _F(.5); // 50% sustain

  //***** OP3 ENVELOPE *****//
  op3.volume.attack.time = 3; // 3ms attack time
  op3.volume.attack.level = _F(.6); // 60% attack

  op3.volume.decay.time = 300; // 300ms decay time
  op3.volume.decay.level = _F(0); // decay to zero

  op3.volume.sustain.level = _F(0); //zero sustain

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
      voices[i]->play(outputData, _F(.9));

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

