/*
 * A Simple FM sketch
 */
#include "ak4558.h"
#include "audioFX.h"
#include "fm.h"

using namespace FX;

ak4558 iface;

//create a 3 operator algorithm
Operator op1(0), op2(1), op3(2);
Operator *ops[] = {&op1, &op3, &op2};
Algorithm A(ops, 3);

/* create a 2 voice synth with the algorithm
 * (this example will only use v1 but this is
 *  here to show how to create multiple voices)
 */
Voice v1(&A), v2(&A);

//op1 will be a static frequency (1000.8 Hz)
FixedFrequency op1Fixed(_F16(1000.8));

//op3 will be a ratio frequency (8x the carrier freq of op2)
RatioFrequency op3Ratio(&op2, _F16(8));

//define a few notes we will use
q16 notes[] = {
    _F16(440.00),
    _F16(466.16),
    _F16(493.88),
    _F16(523.25),
    _F16(554.37),
};

q31 outL[AUDIO_BUFSIZE];

//this will be called when a buffer is ready to be filled
void audioHook(q31 *data)
{
  v1.play(outL);
  //the output is 24 bit
  for(int i=0; i<AUDIO_BUFSIZE; i++) outL[i] >>= 7;

  interleave(data, outL, outL);
}

void setup(){
    Serial.begin(9600);

    /******* DEFINE STRUCTURE *******
     *   OP1     OP3
     *      \   /
     *       OP2
     *        |
     *      output
     */
    op2.isOutput = true; // op2 is carrier frequency

    op2.mods[0] = &op3; // op3 modulates op2
    op2.mods[1] = &op1; // op1 modulates op2

    //******** DEFINE MODULATOR FREQUENCIES ********//
    op1.carrier = &op1Fixed; // op1 is a fixed frequency (set above)
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

    iface.begin();

    //begin fx processor
    fx.begin();

    //set the function to be called when a buffer is ready
    fx.setHook(audioHook);
}

void loop(){
    //Type 1-5 to play notes
    if(Serial.available()){
        char c = Serial.read();
        switch(c){
        case '1':
            v1.setOutput(notes[0]);
            break;
        case '2':
            v1.setOutput(notes[1]);
            break;
        case '3':
            v1.setOutput(notes[2]);
            break;
        case '4':
            v1.setOutput(notes[3]);
            break;
        case '5':
            v1.setOutput(notes[4]);
            break;
        default:
            return;
        }
        v1.trigger(true); //note on (start all envelope attacks)
        delay(500);
        v1.trigger(false); //note off (start all envelope release)
    }
    delay(10);
}
