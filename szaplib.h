#ifndef __SZAPLIB__H
#define __SZAPLIB__H

#include "zaptypes.h"

typedef struct
{
   unsigned int frequency;
   unsigned int pol;
   unsigned int sat_no;
   unsigned int sr;
   unsigned int vpid;
   unsigned int apid;
   unsigned int sid;
} t_dvbs_tune_info;

extern int szap_tune_silent(t_tuner_descriptor tuner, 
                            t_dvbs_tune_info tune_info, int dvr, 
                            unsigned int rec_psi, 
                            StatusReceiver statusReceiver, int audio_bypass, 
                            char *lnb_raw);

#endif
