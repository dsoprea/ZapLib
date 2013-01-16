// Support for digital terrestrial (air).

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
#include "zaptypes.h"
#include "azaplib.h"

int azap_break_tune = 0;

static int setup_frontend (int fe_fd, struct dvb_frontend_parameters *frontend)
{
	struct dvb_frontend_info fe_info;

	if (ioctl(fe_fd, FE_GET_INFO, &fe_info) < 0)
		return -10;

	if (fe_info.type != FE_ATSC)
		return -11;

	if (ioctl(fe_fd, FE_SET_FRONTEND, frontend) < 0)
		return -12;

	return 0;
}

static int check_frontend (int fe_fd, const int interval_us, StatusReceiver statusReceiver)
{
	fe_status_t status;
	uint16_t snr, signal_strength;
	uint32_t ber, uncorrected_blocks;
    int is_locked;

	while(azap_break_tune == 0) {
		ioctl(fe_fd, FE_READ_STATUS, &status);
		ioctl(fe_fd, FE_READ_SIGNAL_STRENGTH, &signal_strength);
		ioctl(fe_fd, FE_READ_SNR, &snr);
		ioctl(fe_fd, FE_READ_BER, &ber);
		ioctl(fe_fd, FE_READ_UNCORRECTED_BLOCKS, &uncorrected_blocks);

        is_locked = (status & FE_HAS_LOCK) > 0;

		if(statusReceiver(status, signal_strength, snr, ber, uncorrected_blocks, is_locked) == 0)
            break;

		usleep(interval_us);
	}

	return 0;
}

static void handleSigalarm()
{
    azap_break_tune = 1;
}

int azap_tune_silent(t_tuner_descriptor tuner, t_atsc_tune_info tune_info, 
                     int dvr, StatusReceiver statusReceiver)
{
    // We use SIGALRM out of convenience, for whether we're testing tuning by 
    // handle, or need to interrupt it from another thread.
    signal(SIGALRM, handleSigalarm);

	const int status_interval_us = 1000000;

	struct dvb_frontend_parameters frontend_param;
	int frontend_fd, audio_fd, video_fd;
	char FRONTEND_DEV [80];
	char DEMUX_DEV [80];
	int retval;

    // Validate.

    int valid_modulations[] = { VSB_8, VSB_16, QAM_64, QAM_256 };
    int i = 0, found = 0;
    while(i < 4)
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

	frontend_param.frequency = tune_info.frequency;
	frontend_param.u.vsb.modulation = tune_info.modulation;

	if ((frontend_fd = open(FRONTEND_DEV, O_RDWR)) < 0)
		return -2;

	if ((retval = setup_frontend (frontend_fd, &frontend_param)) < 0)
		return retval;

    if ((video_fd = open(DEMUX_DEV, O_RDWR)) < 0)
        return -3;

	if (set_pesfilter (video_fd, tune_info.vpid, DMX_PES_VIDEO, dvr) < 0)
		return -4;

	if ((audio_fd = open(DEMUX_DEV, O_RDWR)) < 0)
        return -5;

	if (set_pesfilter (audio_fd, tune_info.apid, DMX_PES_AUDIO, dvr) < 0)
		return -6;

	check_frontend (frontend_fd, status_interval_us, statusReceiver);

	close (audio_fd);
	close (video_fd);
	close (frontend_fd);

	return 0;
}


