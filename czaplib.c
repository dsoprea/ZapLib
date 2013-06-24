// Support for cable in the US (QAM, etc..).

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

#include "zaptypes.h"
#include "util.h"
#include "czaplib.h"

int czap_break_tune = 0;

static int setup_frontend(int fe_fd, struct dvb_frontend_parameters *frontend)
{
	struct dvb_frontend_info fe_info;

	if (ioctl(fe_fd, FE_GET_INFO, &fe_info) < 0)
		return -1;

	if (fe_info.type != FE_QAM)
		return -1;

	if (ioctl(fe_fd, FE_SET_FRONTEND, frontend) < 0)
		return -1;

	return 0;
}

static int check_frontend (int fe_fd, const int interval_us, 
                           StatusReceiver statusReceiver)
{
	fe_status_t status;
	uint16_t snr, signal_strength;
	uint32_t ber, uncorrected_blocks;
    int is_locked;

	while(czap_break_tune == 0) {
		ioctl(fe_fd, FE_READ_STATUS, &status);
		ioctl(fe_fd, FE_READ_SIGNAL_STRENGTH, &signal_strength);
		ioctl(fe_fd, FE_READ_SNR, &snr);
		ioctl(fe_fd, FE_READ_BER, &ber);
		ioctl(fe_fd, FE_READ_UNCORRECTED_BLOCKS, &uncorrected_blocks);

        is_locked = (status & FE_HAS_LOCK) > 0;

		if(statusReceiver(status, signal_strength, snr, ber, uncorrected_blocks, 
		                  is_locked) == 0)
            break;

		usleep(interval_us);
	}

	return 0;
}

static void handleSigalarm()
{
    czap_break_tune = 1;
}

// Tune a DVB-C device. The rec_psi argument indicates that PAT and PMT packets 
// should come through (important if MPEGTS feed is to be readable by players).
int czap_tune_silent(t_tuner_descriptor tuner, t_dvbc_tune_info tune_info, 
                     int dvr, int rec_psi, StatusReceiver statusReceiver)
{
    // We use SIGALRM out of convenience, for whether we're testing tuning by 
    // handle, or need to interrupt it from another thread.
    signal(SIGALRM, handleSigalarm);

	const int status_interval_us = 1000000;

	struct dvb_frontend_parameters frontend_param;
	int pmtpid, frontend_fd, video_fd, audio_fd, pat_fd, pmt_fd;
    int i, found;
    char FRONTEND_DEV [80];
    char DEMUX_DEV [80];

    // Validate.
    
    int valid_inversions[] = { INVERSION_OFF, INVERSION_ON, INVERSION_AUTO };
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
        return -1;

    int valid_fec_list[] = { FEC_1_2, FEC_2_3, FEC_3_4, FEC_4_5, FEC_5_6, 
                             FEC_6_7, FEC_7_8, FEC_8_9, FEC_AUTO, FEC_NONE };
    i = 0;
    found = 0;
    while(i < 10)
    {
        if(valid_fec_list[i] == tune_info.forward_err_corr)
        {
            found = 1;
            break;
        }
        
        i++;
    }
    
    if(found == 0)
        return -1;

    int valid_modulations[] = { QAM_16, QAM_32, QAM_64, QAM_128, QAM_256, 
                                QAM_AUTO };
    i = 0;
    found = 0;
    while(i < 6)
    {
        if(valid_modulations[i] == tune_info.modulation)
        {
            found = 1;
            break;
        }
        
        i++;
    }
    
    if(found == 0)
        return -1;


    // Continue to tuning.

	snprintf (FRONTEND_DEV, sizeof(FRONTEND_DEV),
		  "/dev/dvb/adapter%i/frontend%i", tuner.adapter, tuner.frontend);

	snprintf (DEMUX_DEV, sizeof(DEMUX_DEV),
		  "/dev/dvb/adapter%i/demux%i", tuner.adapter, tuner.demux);

	memset(&frontend_param, 0, sizeof(struct dvb_frontend_parameters));

    frontend_param.frequency         = tune_info.frequency;
    frontend_param.inversion         = tune_info.inversion;
    frontend_param.u.qam.symbol_rate = tune_info.sym_per_sec;
    frontend_param.u.qam.modulation  = tune_info.modulation;
    frontend_param.u.qam.fec_inner   = tune_info.forward_err_corr;

	if ((frontend_fd = open(FRONTEND_DEV, O_RDWR)) < 0)
		return -1;

	if (setup_frontend(frontend_fd, &frontend_param) < 0)
		return -1;

	if (rec_psi) 
	{
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

