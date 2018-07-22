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
  zero(outL);
  v1.play(outL);

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
    op2.volume.setRate(ENVELOPE_ATTACK, 300); // 1ms attack time
    op2.volume.setLevel(ENVELOPE_ATTACK, _F15(.999)); // max attack level (99.9 %)

    op2.volume.setRate(ENVELOPE_DECAY, 100); // 300 ms decay
    op2.volume.setLevel(ENVELOPE_DECAY, _F15(.5)); // decay to zero

    op2.volume.setRate(ENVELOPE_SUSTAIN, 0); // 300 ms decay
    op2.volume.setLevel(ENVELOPE_SUSTAIN, _F15(.25));

    op2.volume.setRate(ENVELOPE_RELEASE, 500);
    op2.volume.setLevel(ENVELOPE_RELEASE, _F15(0));

    //***** OP1 ENVELOPE *****//
    op1.volume.setLevel(ENVELOPE_SUSTAIN, _F15(.5)); // 50% sustain

    //***** OP3 ENVELOPE *****//
    op3.volume.setRate(ENVELOPE_ATTACK, 3); // 3ms attack time
    op3.volume.setLevel(ENVELOPE_ATTACK, _F15(.6)); // 60% attack

    op3.volume.setRate(ENVELOPE_DECAY, 300); // 300ms decay time
    op3.volume.setLevel(ENVELOPE_DECAY, _F15(0)); // decay to zero

    op3.volume.setLevel(ENVELOPE_SUSTAIN, _F15(0)); //zero sustain

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
        delay(1000);
        v1.trigger(false); //note off (start all envelope release)
    }
    delay(10);
}
