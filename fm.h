/*
 * fm.h
 *
 *  Created on: Feb 12, 2018
 *      Author: deanm
 */

#ifndef AUDIOFX_FM_H_
#define AUDIOFX_FM_H_

#include "utility.h"
#include "audioFX.h"

using namespace FX;

#define FM_NUM_VOICES 8
#define OP_MAX_INPUTS 4
#define FM_MAX_OPERATORS 6
#define FM_MAX_ENVELOPES FM_MAX_OPERATORS

#define RATIO_MIN_SECOND 	.05946
#define RATIO_MAJ_SECOND 	.12246
#define RATIO_MIN_THIRD 	.18921
#define RATIO_MAJ_THIRD 	.25992
#define RATIO_FOURTH		.33483
#define RATIO_DIM_FIFTH		.41421
#define RATIO_FIFTH			.49831
#define RATIO_MIN_SIXTH		.58760
#define RATIO_MAJ_SIXTH		.68179
#define RATIO_MIN_SEVENTH	.78180
#define RATIO_MAJ_SEVENTH	.88775

enum {
	ENVELOPE_ATTACK,
	ENVELOPE_DECAY,
	ENVELOPE_SUSTAIN,
	ENVELOPE_RELEASE
};

class Voice;
class Operator;
class Algorithm;

extern q15 _fm_sine[];

extern "C" {
    struct _operator {
        q15 *current;
        q16 *cfreq;
        q15 *mod;
        q15 *vol;
        q31 err;
        q15 lastFeedback;
    };
    extern void _fm_modulate(_operator *op, q15 *buf);
    extern void _fm_modulate_feedback(_operator *op, q15 *buf, q15 feedbackLevel);

    struct _lfo {
        q15 *current;
        q16 cfreq;
        q31 err;
        q15 depth;
    };

    extern void _lfo_q15(_lfo *l, q15 *buf);
    extern void _lfo_q16(_lfo *l, q16 *buf);

    struct _envelope {
    	q31 err;
    	q31 lim;
    	int state;
    	int per;
    };

    extern void _envelope_interpolate(_envelope *e, q15 *buf, q31 roc);
};

/************* BASE MODULATOR CLASS **************/
template<class T> class Modulator {
public:
    Modulator(T initialOutput = 0){ this->output = initialOutput; this->calculated = false; }
    ~Modulator() {}

    virtual void getOutput(T *buf) { for(int i=0; i<AUDIO_BUFSIZE; i++) *buf++ = output; };
    void setOutput(T out) { output = out; }

    bool calculated;
    T output;
};
template class Modulator<q31>;
template class Modulator<q16>;
template class Modulator<q15>;

/************* FIXED FREQUENCY CLASS **************/
class FixedFrequency : public Modulator<q16> {
public:
    FixedFrequency(q16 freq) { output = freq; }
    ~FixedFrequency() {}
};

/************* ENVELOPE CLASS **************/
class Envelope : public Modulator<q15> {
public:
    Envelope() {
    	for(int i=0; i<4; i++){
    		states[i].rate = 1;
    		states[i].level = 0;
    		states[i].roc = 0;
    	}
    	recalculate();
    }
    ~Envelope() {}

    void recalculate();
    void setRate(uint8_t param, int32_t rate);
    void setLevel(uint8_t param, q15 level);
    void getOutput(q15 *buf, _envelope *e, Voice *v);

    typedef struct {
        q31 level;
        int32_t rate;
        q31 roc;
    } envParam;

    envParam states[4];
private:

};

/************* OPERATOR CLASS **************/
class Operator : public Modulator<q15> {
public:
    Operator(int id);
    ~Operator() {}

    /* runs FM equation. Calculates operators this on depends on if they are not done
     * requires the current status of any envelopes.
     * If a circular reference is encountered, the previous calculation is used
     */
    void getOutput(q15 *buf, Voice *voice);
    void setCarrier(Modulator<q16> *mod=NULL);

    Operator *mods[OP_MAX_INPUTS];

    Modulator<q16> *carrier;

    Envelope volume;

    bool active;
    bool isOutput;

    q15 velSense;
    q15 amodSense;

    q15 feedbackLevel;

    q15 *precalculated;
    bool saved;

    friend class Algorithm;

protected:
    bool carrierOverride;
    int id;
};

/************* RATIO FREQUENCY CLASS **************/
class RatioFrequency : public Modulator<q16> {
public:
    RatioFrequency(Operator *source, q16 ratio) {
        this->source = source;
        this->ratio = ratio;
    }
    ~RatioFrequency() {}

    //TODO: this can be optimized if carrier is always static
    void getOutput(q16 *buf){
    	source->carrier->getOutput(buf);
    	_mult_q16(buf, this->ratio);
    }

    Operator *source;
    q16 ratio;

};

/************* ALGORITHM CLASS **************/
class Algorithm : public Modulator<q31>{
public:
    Algorithm(Operator **operators, uint8_t numOperators) { setOperators(operators, numOperators); }
    ~Algorithm() {}

    /* runs all operators for a given voice, return mixed output */
    void getOutput(q31 *buf, Voice *voice);

    void setOperators(Operator **operators, uint8_t numOperators) { ops = operators; numOps = numOperators; }

    friend class Operator;

protected:
    Operator **ops;
    uint8_t numOps;
};

/************* LFO CLASS **************/
template<class T> class LFO : public Modulator<T> {
public:
	LFO() {
        _l.current = _fm_sine;
        _l.cfreq = 0;
        _l.err = 0;
        _l.depth = 0;
    }
    ~LFO() {}

    void setDepth(q15 depth){ _l.depth = depth; }
    q15 getDepth(){ return _l.depth; }
    void setRate(q16 rate){ _l.cfreq = rate; }
    void getOutput(T *buf);

    _lfo _l;
};

/************* VOICE CLASS **************/

class Voice : public Modulator<q16>{
public:
    Voice(Algorithm *algo, q31 gain = _F(.999)) {
    	algorithm = algo;
    	active = false;
    	ms = 0;
    	this->gain = gain;
    	velocity = _F15(.999);
    	cfreq = NULL;
        amod = NULL;
        dynamicFreq = true;
        for(int i=0; i<FM_MAX_OPERATORS;i++){
            _operator *op = &_ops[i];
            op->current = _fm_sine;
            op->cfreq = NULL;
            op->mod = NULL;
            op->vol = NULL;
            op->lastFeedback = 0;

            _envelope *env = &_envs[i];
            env->err = 0;
        }
    }
    ~Voice() {}

    void setOutput(q16 *buf){
    	dynamicFreq = false;
    	cfreq = buf;
    }

    void setOutput(q16 val){
    	dynamicFreq = true;
    	output = val;
    }

    void getOutput(q16 *buf) {
    	copy((q31 *)buf, (q31 *)cfreq);
    };

    void play(q31 *buf, q31 gain=0, LFO<q16> *mod=NULL);
    void play(q31 *buf, q15 *amod){
        this->amod = amod;
        play(buf);
    }
    void trigger(bool state) {
    	if(!state && !active) return;
        if(state){
        	if(!active){
				for(int i=0; i<FM_MAX_OPERATORS;i++)
					_ops[i].current = _fm_sine;
        	}
            ms = 0;
        }
        active = state;
    }

    bool active;
    q15 velocity;
    uint32_t ms;
    q31 gain;

    friend class Operator;

protected:
    _operator _ops[FM_MAX_OPERATORS];
    _envelope _envs[FM_MAX_OPERATORS];
    q16 *cfreq;
    q15 *amod;

private:
    Algorithm *algorithm;
    bool dynamicFreq;
};

#endif /* AUDIOFX_FM_H_ */
