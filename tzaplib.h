#ifndef __SZAPLIB__H
#define __SZAPLIB__H

#include <linux/dvb/frontend.h>

#include "zaptypes.h"

typedef struct
{
    int frequency;
    fe_modulation_t         modulation;
    fe_spectral_inversion_t inversion;
    fe_bandwidth_t          bandwidth;
    fe_code_rate_t          forward_err_corr_hp;
    fe_code_rate_t          forward_err_corr_lp;
    fe_transmit_mode_t      transmission_mode;
    fe_guard_interval_t     guard_interval;
    fe_hierarchy_t          heirarchy_information;
    int vpid;
    int apid;
    int sid;
} t_dvbt_tune_info;

extern int tzap_tune_silent(t_tuner_descriptor tuner, 
                            t_dvbt_tune_info tune_info, 
                            int dvr, unsigned int rec_psi, 
                            StatusReceiver statusReceiver);

#endif
