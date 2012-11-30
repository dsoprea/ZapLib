/* tzap -- DVB-T zapping utility
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include "util.h"
#include "tzaplib.h"

static char FRONTEND_DEV [80];
static char DEMUX_DEV [80];

static int check_fec(fe_code_rate_t *fec)
{
	switch (*fec)
	{
	case FEC_NONE:
		*fec = FEC_AUTO;
	case FEC_AUTO:
	case FEC_1_2:
	case FEC_2_3:
	case FEC_3_4:
	case FEC_5_6:
	case FEC_7_8:
		return 0;
	default:
		;
	}
	return 1;
}


static int parse(t_dvbt_tune_info tune_info, 
                 struct dvb_frontend_parameters *frontend)
{

    fe_spectral_inversion_t valid_inversions[] = { INVERSION_OFF, INVERSION_ON, 
                                                   INVERSION_AUTO };

    fe_bandwidth_t valid_bandwidths[] = { BANDWIDTH_6_MHZ, BANDWIDTH_7_MHZ, 
                                          BANDWIDTH_8_MHZ };

    fe_code_rate_t valid_fec[] = { FEC_1_2, FEC_2_3, FEC_3_4, FEC_4_5, FEC_5_6, 
                                   FEC_6_7, FEC_7_8, FEC_8_9, FEC_AUTO, 
                                   FEC_NONE };

    fe_guard_interval_t valid_guards[] = { GUARD_INTERVAL_1_16, 
                                           GUARD_INTERVAL_1_32, 
                                           GUARD_INTERVAL_1_4, 
                                           GUARD_INTERVAL_1_8, 
                                           GUARD_INTERVAL_AUTO };

    fe_hierarchy_t valid_heirarchies[] = { HIERARCHY_1, HIERARCHY_2, 
                                           HIERARCHY_4, HIERARCHY_NONE, 
                                           HIERARCHY_AUTO };

    fe_modulation_t valid_modulations[] = { QPSK, QAM_128, QAM_16, QAM_256, 
                                            QAM_32, QAM_64, QAM_AUTO };

    fe_transmit_mode_t valid_transmission_modes[] = { TRANSMISSION_MODE_2K, 
                                                      TRANSMISSION_MODE_8K, 
                                                      TRANSMISSION_MODE_AUTO };

    int i, found;

    // Validate inversion.
    
    i = 0;
    found = 0;
    while(i < 3)
    {
        if(valid_inversions[i] == tune_info.inversion)
        {
            found = 1;
            break;
        }
    
        i++;
    }

    if(found == 0)
		return -4;

    // Validate bandwidth.

    i = 0;
    found = 0;
    while(i < 3)
    {
        if(valid_bandwidths[i] == tune_info.bandwidth)
        {
            found = 1;
            break;
        }
    
        i++;
    }

	if(found == 0)
		return -5;

    // Validate FEC-HI.

    i = 0;
    found = 0;
    while(i < 10)
    {
        if(valid_fec[i] == tune_info.forward_err_corr_hp)
        {
            found = 1;
            break;
        }
    
        i++;
    }

	if(found == 0)
		return -6;

	if (check_fec(&tune_info.forward_err_corr_hp))
		return -6;

    // Validate FEC-LOW.

    i = 0;
    found = 0;
    while(i < 10)
    {
        if(valid_fec[i] == tune_info.forward_err_corr_lp)
        {
            found = 1;
            break;
        }
    
        i++;
    }

	if(found == 0)
		return -7;

	if (check_fec(&tune_info.forward_err_corr_lp))
		return -7;

    // Validate modulation.

    i = 0;
    found = 0;
    while(i < 7)
    {
        if(valid_modulations[i] == tune_info.modulation)
        {
            found = 1;
            break;
        }
    
        i++;
    }

	if(found == 0)
		return -8;

    // Validate transmission-mode.

    i = 0;
    found = 0;
    while(i < 7)
    {
        if(valid_transmission_modes[i] == tune_info.transmission_mode)
        {
            found = 1;
            break;
        }
    
        i++;
    }

	if(found == 0)
		return -9;

    // Validate guard-interval.

    i = 0;
    found = 0;
    while(i < 5)
    {
        if(valid_guards[i] == tune_info.guard_interval)
        {
            found = 1;
            break;
        }
    
        i++;
    }

	if(found == 0)
		return -10;

    // Validate heirarchy.

    i = 0;
    found = 0;
    while(i < 5)
    {
        if(valid_heirarchies[i] == tune_info.heirarchy_information)
        {
            found = 1;
            break;
        }
    
        i++;
    }

	if(found == 0)
		return -11;

	frontend->frequency                    = tune_info.frequency;
	frontend->inversion                    = tune_info.inversion;
	frontend->u.ofdm.bandwidth             = tune_info.bandwidth;
	frontend->u.ofdm.code_rate_HP          = tune_info.forward_err_corr_hp;
	frontend->u.ofdm.code_rate_LP          = tune_info.forward_err_corr_lp;
	frontend->u.ofdm.constellation         = tune_info.modulation;
	frontend->u.ofdm.transmission_mode     = tune_info.transmission_mode;
	frontend->u.ofdm.guard_interval        = tune_info.guard_interval;
	frontend->u.ofdm.hierarchy_information = tune_info.heirarchy_information;
	
	return 0;
}


static
int setup_frontend (int fe_fd, struct dvb_frontend_parameters *frontend)
{
	struct dvb_frontend_info fe_info;

	if (ioctl(fe_fd, FE_GET_INFO, &fe_info) < 0)
		return -1;

	if (fe_info.type != FE_OFDM)
		return -1;

	if (ioctl(fe_fd, FE_SET_FRONTEND, frontend) < 0)
		return -1;

	return 0;
}

static
int check_frontend(int fe_fd, const int interval_us, 
                   StatusReceiver statusReceiver)
{
	fe_status_t status;
    uint16_t snr, signal;
    uint32_t ber, uncorrected_blocks;
    int is_locked;

	while(1) {
	    ioctl(fe_fd, FE_READ_STATUS, &status);
	    ioctl(fe_fd, FE_READ_SIGNAL_STRENGTH, &signal);
	    ioctl(fe_fd, FE_READ_SNR, &snr);
	    ioctl(fe_fd, FE_READ_BER, &ber);
	    ioctl(fe_fd, FE_READ_UNCORRECTED_BLOCKS, &uncorrected_blocks);

        is_locked = (status & FE_HAS_LOCK) > 0;

		if(statusReceiver(status, signal, snr, ber, uncorrected_blocks, 
		                  is_locked) == 0)
            break;

		usleep(interval_us);
	}

	return 0;
}

int tzap_tune_silent(t_tuner_descriptor tuner, t_dvbt_tune_info tune_info, 
                     int dvr, unsigned int rec_psi, 
                     StatusReceiver statusReceiver)
{
	const int status_interval_us = 1000000;

	struct dvb_frontend_parameters frontend_param;

	int pmtpid = 0;
	int pat_fd, pmt_fd;
	int frontend_fd, audio_fd = 0, video_fd = 0;

	snprintf (FRONTEND_DEV, sizeof(FRONTEND_DEV),
		  "/dev/dvb/adapter%i/frontend%i", tuner.adapter, tuner.frontend);

	snprintf (DEMUX_DEV, sizeof(DEMUX_DEV),
		  "/dev/dvb/adapter%i/demux%i", tuner.adapter, tuner.demux);

	memset(&frontend_param, 0, sizeof(struct dvb_frontend_parameters));

	if (parse (tune_info, &frontend_param))
		return -1;

	if ((frontend_fd = open(FRONTEND_DEV, O_RDWR)) < 0)
		return -1;

	if (setup_frontend (frontend_fd, &frontend_param) < 0)
		return -1;

	if (rec_psi) {
	    pmtpid = get_pmt_pid(DEMUX_DEV, tune_info.sid);
	    if (pmtpid <= 0)
    		return -1;

	    if ((pat_fd = open(DEMUX_DEV, O_RDWR)) < 0)
    		return -1;

	    if (set_pesfilter(pat_fd, 0, DMX_PES_OTHER, dvr) < 0)
		    return -1;

	    if ((pmt_fd = open(DEMUX_DEV, O_RDWR)) < 0)
    		return -1;

	    if (set_pesfilter(pmt_fd, pmtpid, DMX_PES_OTHER, dvr) < 0)
		    return -1;
	}

    if ((video_fd = open(DEMUX_DEV, O_RDWR)) < 0)
        return -1;

	if (set_pesfilter (video_fd, tune_info.vpid, DMX_PES_VIDEO, dvr) < 0)
		return -1;

	if ((audio_fd = open(DEMUX_DEV, O_RDWR)) < 0)
        return -1;

	if (set_pesfilter (audio_fd, tune_info.apid, DMX_PES_AUDIO, dvr) < 0)
		return -1;

	check_frontend (frontend_fd, status_interval_us, statusReceiver);

	close (pat_fd);
	close (pmt_fd);
	close (audio_fd);
	close (video_fd);
	close (frontend_fd);

	return 0;
}

