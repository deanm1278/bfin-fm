#include "fm.h"

#define NUM_OPERATORS 6
extern Operator op1, op2, op3, op4, op5, op6;
extern Operator *ops[NUM_OPERATORS];

void chooseAlgo(uint8_t num)
{
  //see http://blog.dubspot.com/files/2012/12/DX-7-Algorithms.png

  //clear current settings
  for(int i=0; i<NUM_OPERATORS; i++){
    ops[i]->isOutput = false;
    ops[i]->feedbackLevel = 0;
    for(int j=0; j<OP_MAX_INPUTS; j++){
      ops[i]->mods[j] = NULL;
    }
  }
  //op1 is always an output
  op1.isOutput = true;

  switch(num){
  case 1:
  case 2:
    op3.isOutput = true;

    op1.mods[0] = &op2;

    op3.mods[0] = &op4;
    op4.mods[0] = &op5;
    op5.mods[0] = &op6;
    break;

  case 3:
    op4.isOutput = true;

    op1.mods[0] = &op2;
    op2.mods[0] = &op3;

    op4.mods[0] = &op5;
    op5.mods[0] = &op6;
    break;

  case 4:
    //not implemented
    break;

  case 5:
    op3.isOutput = true;
    op5.isOutput = true;

    op1.mods[0] = &op2;
    op3.mods[0] = &op4;
    op5.mods[0] = &op6;
    break;

  case 6:
    //not implemented
    break;

  case 7:
  case 8:
  case 9:
    op3.isOutput = true;

    op1.mods[0] = &op2;

    op3.mods[0] = &op4;
    op3.mods[1] = &op5;
    op5.mods[0] = &op6;
    break;

  case 10:
  case 11:
    op4.isOutput = true;

    op1.mods[0] = &op2;
    op2.mods[0] = &op3;

    op4.mods[0] = &op6;
    op4.mods[1] = &op5;

    break;

  case 12:
  case 13:
    op3.isOutput = true;

    op1.mods[0] = &op2;

    op3.mods[0] = &op4;
    op3.mods[1] = &op5;
    op3.mods[2] = &op6;
    break;

  case 14:
  case 15:
    op3.isOutput = true;

    op1.mods[0] = &op2;

    op3.mods[0] = &op4;
    op4.mods[0] = &op5;
    op4.mods[1] = &op6;
    break;

  case 16:
  case 17:
    op1.mods[0] = &op2;
    op1.mods[1] = &op3;
    op1.mods[2] = &op5;
    op3.mods[0] = &op4;
    op5.mods[0] = &op6;
    break;

  case 18:
    op1.mods[0] = &op2;
    op1.mods[1] = &op3;
    op1.mods[2] = &op4;
    op4.mods[0] = &op5;
    op5.mods[0] = &op6;
    break;

  case 19:
    op4.isOutput = true;
    op5.isOutput = true;

    op1.mods[0] = &op2;
    op2.mods[0] = &op3;

    op4.mods[0] = &op6;

    op5.mods[0] = &op6;
    break;

  case 20:
    op2.isOutput = true;
    op4.isOutput = true;

    op1.mods[0] = &op3;
    op2.mods[0] = &op3;

    op4.mods[0] = &op5;
    op4.mods[1] = &op6;
    break;

  case 21:
    op2.isOutput = true;
    op4.isOutput = true;
    op5.isOutput = true;

    op1.mods[0] = &op3;
    op2.mods[0] = &op3;

    op4.mods[0] = &op6;
    op5.mods[0] = &op6;
    break;

  case 22:
    op3.isOutput = true;
    op4.isOutput = true;
    op5.isOutput = true;

    op1.mods[0] = &op2;

    op3.mods[0] = &op6;

    op4.mods[0] = &op6;

    op5.mods[0] = &op6;
    break;

  case 23:
    op2.isOutput = true;
    op4.isOutput = true;
    op5.isOutput = true;

    op2.mods[0] = &op3;

    op4.mods[0] = &op6;

    op5.mods[0] = &op6;
    break;

  case 24:
    op2.isOutput = true;
    op3.isOutput = true;
    op4.isOutput = true;
    op5.isOutput = true;

    op3.mods[0] = &op6;

    op4.mods[0] = &op6;

    op5.mods[0] = &op6;
    break;

  case 25:
    op2.isOutput = true;
    op3.isOutput = true;
    op4.isOutput = true;
    op5.isOutput = true;

    op4.mods[0] = &op6;

    op5.mods[0] = &op6;
    break;

  case 26:
  case 27:
    op2.isOutput = true;
    op4.isOutput = true;

    op2.mods[0] = &op3;

    op4.mods[0] = &op5;
    op4.mods[1] = &op6;
    break;

  case 28:
    op3.isOutput = true;
    op6.isOutput = true;

    op1.mods[0] = &op2;

    op3.mods[0] = &op4;
    op4.mods[0] = &op5;
    break;

  case 29:
    op2.isOutput = true;
    op3.isOutput = true;
    op5.isOutput = true;

    op3.mods[0] = &op4;
    op5.mods[0] = &op6;
    break;

  case 30:
    op2.isOutput = true;
    op3.isOutput = true;
    op6.isOutput = true;

    op3.mods[0] = &op4;
    op4.mods[0] = &op5;
    break;

  case 31:
    op2.isOutput = true;
    op3.isOutput = true;
    op4.isOutput = true;
    op5.isOutput = true;

    op5.mods[0] = &op6;
    break;

  case 32:
    op2.isOutput = true;
    op3.isOutput = true;
    op4.isOutput = true;
    op5.isOutput = true;
    op6.isOutput = true;
    break;

    default:
      break;
  }
}

