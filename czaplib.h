#ifndef __CZAPLIB__H
#define __CZAPLIB__H

#include "zaptypes.h"

typedef struct
{
    int frequency;
    int vpid;
    int apid;
    int sid;
    int inversion; 
    int sym_per_sec;
    int modulation;
    int forward_err_corr;
} t_dvbc_tune_info;

extern int czap_tune_silent(t_tuner_descriptor tuner, 
                            t_dvbc_tune_info tune_info, int dvr, int rec_psi, 
                            StatusReceiver statusReceiver);

#endif

