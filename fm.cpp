/*
 * fm.cpp
 *
 *  Created on: Feb 12, 2018
 *      Author: deanm
 */

#include "fm.h"

using namespace FX;

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
        q15 volume_buf[AUDIO_BUFSIZE];
        volume.getOutput(volume_buf, voice, voice->_ops[id].lastVolume);
        if(voice->active)
        	voice->_ops[id].lastVolume = volume_buf[AUDIO_BUFSIZE - 1]; //save the last volume for release

		//velocity scaling
        q15 vmod = 0x7FFF - __builtin_bfin_multr_fr1x16((0x7FFF - voice->velocity), velSense);
        gain(volume_buf, volume_buf, vmod);

    	//gain by amod LFO
    	if(voice->amod != NULL){
    		q15 amod_buf[AUDIO_BUFSIZE];
    		gain(amod_buf, voice->amod, amodSense);
    		for(int i=0; i<AUDIO_BUFSIZE; i++){
    			volume_buf[i] = __builtin_bfin_mult_fr1x16(volume_buf[i], 0x7FFF - amod_buf[i]);
    		}
    	}

		//TODO: keyboard/range scaling

        q16 cfreq[AUDIO_BUFSIZE];
        carrier->getOutput(cfreq);
		voice->_ops[id].cfreq = cfreq;
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
	q15 tmpbuf[AUDIO_BUFSIZE];
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

void Envelope::getOutput(q15 *buf, Voice *voice, q15 last){

	//for now calculate the envelope twice per buffer
	q15 *ptr = buf;
	q15 start[3] = {0, 0, 0};
	q15 inc[3] = {0, 0, 0};
	for(int i=-1; i<2; i++){
		int idx = i + 1;
		int32_t ms = voice->ms + i;
        //tick the envelope
		if(!voice->active){
			if(voice->ms > release.time){
				start[idx] = release.level;
				inc[idx] = 0;
			}
			else{
				//we are in R
				if(ms == -1)
					start[idx] = last;
				else
					start[idx] = last + (release.level - last)/release.time * ms;
				inc[idx] = (release.level - last)/release.time;
			}
		}
		else{
            //we are in ADS
			if(ms >= attack.time + decay.time + decay2.time){
				//Sustain
				start[idx] = sustain.level;
				inc[idx] = 0;
			}
			else if(ms >= attack.time + decay.time){
            	//Decay2
				start[idx] = decay.level + (sustain.level - decay.level)/decay2.time * (ms - decay.time);
				inc[idx] = (sustain.level - decay.level)/decay2.time;
				voice->hold = true;
            }
            else if(ms > attack.time){
                //Decay
            	start[idx] = attack.level + (decay.level - attack.level)/decay.time * (ms - attack.time);
            	inc[idx] = (decay.level - attack.level)/decay.time;
            	voice->hold = true;
            	voice->interruptable = false;
            }
            else{
            	voice->hold = true;
            	voice->interruptable = false;
                //Attack
            	if(ms == -1)
            		start[idx] = 0;
            	else
					start[idx] = attack.level/attack.time * ms;
				inc[idx] = attack.level/attack.time;
            }
        }
		inc[idx] = inc[idx]/(AUDIO_BUFSIZE/2);
    }

	for(int i=1; i<=2; i++){
		*ptr++ = start[i-1];
		for(int j=1; j<AUDIO_BUFSIZE/2; j++)
			*ptr++ = *(ptr-1) + inc[i];
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
	this->hold = false;
	this->interruptable = true;

	cfreq = (q16 *)malloc(sizeof(q16)*AUDIO_BUFSIZE);
	for(int i=0; i<AUDIO_BUFSIZE; i++)
		cfreq[i] = output;

	if(mod != NULL)
		mod->getOutput(cfreq); //run it through the modulator

	q31 tmpBuffer[AUDIO_BUFSIZE];
	zero(tmpBuffer);

	algorithm->getOutput(tmpBuffer, this);

	for(int i=0; i<AUDIO_BUFSIZE; i++){
		q31 u, v = tmpBuffer[i], w = buf[i];
		__asm__ volatile("R2 = %1 * %2;" \
						"%0 = R2 + %3 (S);"
						: "=r"(u) : "d"(v), "d"(this->gain), "d"(w) : "R2");
		buf[i] = u;
	}

	if(!this->hold && this->queueStop){
		this->active = false;
		ms = 2;
		this->queueStop = false;
	}
	else
    	ms += 2;

	free(cfreq);
}
