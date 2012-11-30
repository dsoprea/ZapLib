#ifndef __AZAPLIB__H
#define __AZAPLIB__H

#include "zaptypes.h"

typedef struct
{
    int frequency;
    fe_modulation_t modulation;
    int vpid;
    int apid;
} t_atsc_tune_info;

extern int azap_tune_silent(t_tuner_descriptor tuner, 
                            t_atsc_tune_info tune_info, int dvr, 
                            StatusReceiver statusReceiver);

#endif
