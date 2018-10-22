/*
 * fm.cpp
 *
 *  Created on: Feb 12, 2018
 *      Author: deanm
 */

#include "fm.h"

using namespace FX;

static RAMB q16 cfreq_stat[AUDIO_BUFSIZE];
static RAMB q31 tmpBuffer[AUDIO_BUFSIZE];
static RAMB q16 cfreq_op[AUDIO_BUFSIZE];
static RAMB q15 volume_buf[AUDIO_BUFSIZE];
static RAMB q15 amod_buf[AUDIO_BUFSIZE];
static RAMB q15 tmpbuf[AUDIO_BUFSIZE];

Operator::Operator(int id) : volume(), id(id) {
    isOutput = false;
    active = true;
    carrierOverride = false;
    carrier = NULL;
    feedbackLevel = 0;
    saved = false;
    velSense = _F15(.999);

    for(int i=0; i<OP_MAX_INPUTS; i++)
        mods[i] = NULL;
}

void Operator::setCarrier(Modulator<q16> *mod)
{
	if(mod == NULL) carrierOverride = false;
	else{
		carrierOverride = true;
		carrier = mod;
	}
}

// calculate the output and add it to buf
void Operator::getOutput(q15 *buf, Voice *voice) {

    if(!calculated){
        calculated = true;

    	//calculate modulators
        q15 mod_buf[AUDIO_BUFSIZE];
    	zero(mod_buf);

        for(int ix=0; ix<OP_MAX_INPUTS; ix++){
            if(mods[ix] != NULL && mods[ix]->active)
            	mods[ix]->getOutput(mod_buf, voice);
        }

        //calculate envelope
        volume.getOutput(volume_buf, &voice->_envs[id], voice);

		//velocity scaling
        q15 vmod = 0x7FFF - __builtin_bfin_multr_fr1x16((0x7FFF - voice->velocity), velSense);
        gain(volume_buf, volume_buf, vmod);

    	//gain by amod LFO
    	if(voice->amod != NULL){
    		gain(amod_buf, voice->amod, amodSense);
    		for(int i=0; i<AUDIO_BUFSIZE; i++){
    			volume_buf[i] = __builtin_bfin_mult_fr1x16(volume_buf[i], 0x7FFF - amod_buf[i]);
    		}
    	}

		//TODO: keyboard/range scaling
        carrier->getOutput(cfreq_op);
		voice->_ops[id].cfreq = cfreq_op;
		voice->_ops[id].mod = mod_buf;
		voice->_ops[id].vol = volume_buf;

		if(feedbackLevel == 0){
			_fm_modulate(&voice->_ops[id], buf);
		}
		else{
			_fm_modulate_feedback(&voice->_ops[id], buf, feedbackLevel);
		}

		//look for another place this is used and save output if necessary
		for(int i=0; i<voice->algorithm->numOps; i++){
			if(!voice->algorithm->ops[i]->calculated){
				for(int j=0; j<OP_MAX_INPUTS; j++){
					if(voice->algorithm->ops[i]->mods[j] != NULL){
						if(voice->algorithm->ops[i]->mods[j]->id == id){
							saved = true;
							break;
						}
					}
				}
				if(saved) break;
			}
		}

		if(saved){
			precalculated = (q15 *)malloc(AUDIO_BUFSIZE*sizeof(q15));
			copy(precalculated, buf);
		}
    }
	else {
		//sum precalculated buffer into output
		sum(buf, precalculated);
	}
}

void Algorithm::getOutput(q31 *buf, Voice *voice) {
    for(int i=0; i<numOps; i++){
        ops[i]->calculated = false;
        ops[i]->saved = false;
        if((ops[i]->isOutput && !ops[i]->carrierOverride) || ops[i]->carrier == NULL) ops[i]->carrier = voice;
    }

	//TODO: auto-determine outputs based on routing
    for(int i=0; i<numOps; i++){
		zero(tmpbuf);
        if(ops[i]->isOutput && ops[i]->active){
            ops[i]->getOutput(tmpbuf, voice);
			convertAdd(buf, tmpbuf);
		}
    }

	//free any saved outputs
	for(int i=0; i<numOps; i++){
		if(ops[i]->saved)
			free(ops[i]->precalculated);
    }
}

void Envelope::recalculate(){
	int prev = 3;
	for(int i=0; i<4; i++){
		states[i].roc = (states[i].level - states[prev].level)/(states[i].rate * (AUDIO_BUFSIZE/2));
		prev = (prev + 1)%4;
	}
}

void Envelope::setRate(uint8_t param, int32_t rate){
	//if(rate < 1) rate = 1;
	rate = max(rate, 0);
	states[param].rate = rate;
	recalculate();
}

void Envelope::setLevel(uint8_t param, q15 level){
	//make sure it's a multiple of (AUDIO_BUFSIZE/2)
	q31 lvlAdj = (q31)level << 16;
	states[param].level = (lvlAdj + ((AUDIO_BUFSIZE/2)-1)) & ~((AUDIO_BUFSIZE/2)-1);
	recalculate();
}

void Envelope::getOutput(q15 *buf, _envelope *e, Voice *v){
	q31 roc;
	for(int t=v->ms; t<v->ms+2; t++){
		if(!v->active && e->state != ENVELOPE_RELEASE){
			e->state = ENVELOPE_RELEASE;
			e->per = 0;
		}
		else if(t == 0){
			e->state = ENVELOPE_ATTACK;
			e->per = 0;
		}

		e->lim = states[e->state].level;
		_envelope_interpolate(e, buf, states[e->state].roc);
		//if(e->state == ENVELOPE_ATTACK) __asm__ volatile("EMUEXCPT;");

		e->per++;
		if( (states[e->state].level == e->err && states[e->state].roc > 0) ||
			(states[e->state].roc == 0 && e->per >= states[e->state].rate))
			{
			if(e->state != ENVELOPE_RELEASE && e->state != ENVELOPE_SUSTAIN){
				e->state++;
				e->per = 0;
			}
		}
		buf += AUDIO_BUFSIZE/2;
	}
}

template<> void LFO<q15>::getOutput(q15 *buf) {
	_lfo_q15(&_l, buf);
}


template<> void LFO<q16>::getOutput(q16 *buf) {
	_lfo_q16(&_l, buf);
}

void Voice::play(q31 *buf, q31 gain, LFO<q16> *mod) {
	if(gain)
		this->gain = gain;

	if(dynamicFreq){
		cfreq = cfreq_stat;
		for(int i=0; i<AUDIO_BUFSIZE; i++)
			cfreq[i] = output;
	}

	if(mod != NULL)
		mod->getOutput(cfreq); //run it through the modulator

	zero(tmpBuffer);

	algorithm->getOutput(tmpBuffer, this);

	for(int i=0; i<AUDIO_BUFSIZE; i++){
		q31 u, v = tmpBuffer[i], w = buf[i];
		__asm__ volatile("R2 = %1 * %2;" \
						"%0 = R2 + %3 (S);"
						: "=r"(u) : "d"(v), "d"(this->gain), "d"(w) : "R2");
		buf[i] = u;
	}

	ms += 2;
}
