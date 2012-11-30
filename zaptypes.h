#ifndef __ZAPTYPES__H
#define __ZAPTYPES__H

typedef int (*StatusReceiver)(fe_status_t status, uint16_t signal, uint16_t snr, uint32_t ber, uint32_t uncorrected_blocks, int is_locked);

typedef struct
{
    unsigned int adapter;
    unsigned int frontend;
    unsigned int demux;
} t_tuner_descriptor;

#endif

