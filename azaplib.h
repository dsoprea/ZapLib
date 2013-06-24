#ifndef __AZAPLIB__H
#define __AZAPLIB__H

#include "zaptypes.h"

typedef struct
{
    int frequency;
    fe_modulation_t modulation;

    // Video PID.
    int vpid;
    
    // Audio PID.
    int apid;
    
    // Service ID.
    int sid;
} t_atsc_tune_info;

extern int azap_tune_silent(t_tuner_descriptor tuner, 
                            t_atsc_tune_info tune_info, int dvr, int rec_psi, 
                            StatusReceiver statusReceiver);

#endif
