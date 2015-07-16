#ifndef _LOAD_MODEL_H_
#define _LOAD_MODEL_H_

#include "switch_float.h"
#include "MODEL_info.h"

extern MODEL *load_model(FLOAT ratio);		//load MODEL(filter)
extern void free_model(MODEL *MO);		//release model 

#endif /* _LOAD_MODEL_H_ */
