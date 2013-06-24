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
#include <syslog.h>

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

static void start_log()
{
    openlog("zaplib", LOG_PID, LOG_USER);
}

static void stop_log()
{
    closelog();
}

typedef struct
{
    int frontend;
    int audio;
    int video;
    int pat;
    int pmt;
} fd_state_t;

static void cleanup_fd(fd_state_t *fd_state)
{
    if(fd_state->audio != 0)
    	close(fd_state->audio);

    if(fd_state->video != 0)
    	close(fd_state->video);

    if(fd_state->frontend != 0)
    	close(fd_state->frontend);

    if(fd_state->pat != 0)
    	close(fd_state->pat);
    	
    if(fd_state->pmt != 0)
    	close(fd_state->pmt);
}

static int permit_psi(fd_state_t *fd_state, t_atsc_tune_info *tune_info, 
                      char *DEMUX_DEV, int dvr)
{
	int pmtpid;

    syslog(LOG_DEBUG, "Opening demux for PATs.");
    if ((fd_state->pat = open(DEMUX_DEV, O_RDWR)) < 0)
	    return -1;

    syslog(LOG_DEBUG, "Permitting packets for PATs.");
    if (set_pesfilter(fd_state->pat, 0, DMX_PES_OTHER, dvr) < 0)
	    return -2;

    syslog(LOG_DEBUG, "Resolving PMT for SID (%X).", tune_info->sid);
	pmtpid = get_pmt_pid(DEMUX_DEV, tune_info->sid);
    if (pmtpid <= 0)
	    return -3;

    syslog(LOG_DEBUG, "Opening demux for PMTs.");
    if ((fd_state->pmt = open(DEMUX_DEV, O_RDWR)) < 0)
	    return -4;

    syslog(LOG_DEBUG, "Permitting packets for PMTs with PID (%X).", 
                      pmtpid);

    if (set_pesfilter(fd_state->pmt, pmtpid, DMX_PES_OTHER, dvr) < 0)
	    return -5;
	    
	return 0;
}

// Tune an ATSC DVB device. The rec_psi argument indicates that PAT and PMT 
// packets should come through (important if MPEGTS feed is to be readable by 
// players).
int azap_tune_silent(t_tuner_descriptor tuner, t_atsc_tune_info tune_info, 
                     int dvr, int rec_psi, StatusReceiver statusReceiver)
{
    start_log();
    syslog(LOG_DEBUG, "Initializing ZapLib for ATSC tune of (%d, %d, %d) on "
                      "adapter (%d).", tune_info.vpid, tune_info.apid, 
                      tune_info.sid, tuner.adapter);

    // We use SIGALRM out of convenience, for whether we're testing tuning by 
    // handle, or need to interrupt it from another thread.
    signal(SIGALRM, handleSigalarm);

	const int status_interval_us = 1000000;

	struct dvb_frontend_parameters frontend_param;
	char FRONTEND_DEV [80];
	char DEMUX_DEV [80];
	int retval;

    fd_state_t fd_state;
    memset(&fd_state, 0, sizeof(fd_state_t));

	//int frontend_fd, audio_fd, video_fd;
	//int pat_fd = 0, pmt_fd = 0;
	
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

    syslog(LOG_DEBUG, "Opening frontend [%s].", FRONTEND_DEV);
	if ((fd_state.frontend = open(FRONTEND_DEV, O_RDWR)) < 0)
		return -2;

    syslog(LOG_DEBUG, "Configuring frontend.");
	if ((retval = setup_frontend (fd_state.frontend, &frontend_param)) < 0)
		return retval;

	if (rec_psi) 
	{
        syslog(LOG_DEBUG, "Permitting PSI packets on frontend.");

        if(permit_psi(&fd_state, &tune_info, DEMUX_DEV, dvr) < 0)
        {
            cleanup_fd(&fd_state);
            stop_log();

            return -8;
        }
    }
    else
        syslog(LOG_DEBUG, "No PSI packets will be permitted on frontend.");

    syslog(LOG_DEBUG, "Opening demux for video PIDs.");
    if ((fd_state.video = open(DEMUX_DEV, O_RDWR)) < 0)
    {
        cleanup_fd(&fd_state);
        stop_log();

        return -3;
    }

    syslog(LOG_DEBUG, "Permitting packets for VPID (%X).", tune_info.vpid);
	if (set_pesfilter (fd_state.video, tune_info.vpid, DMX_PES_VIDEO, dvr) < 0)
    {
        cleanup_fd(&fd_state);
        stop_log();

		return -4;
    }

    syslog(LOG_DEBUG, "Opening demux for audio PIDs.");
	if ((fd_state.audio = open(DEMUX_DEV, O_RDWR)) < 0)
    {
        cleanup_fd(&fd_state);
        stop_log();

        return -5;
    }

    syslog(LOG_DEBUG, "Permitting packets for APID (%X).", tune_info.vpid);
	if (set_pesfilter (fd_state.audio, tune_info.apid, DMX_PES_AUDIO, dvr) < 0)
    {
        cleanup_fd(&fd_state);
        stop_log();

		return -6;
    }

    syslog(LOG_DEBUG, "Entering tune-loop.");
	check_frontend (fd_state.frontend, status_interval_us, statusReceiver);
    syslog(LOG_DEBUG, "Tune-loop has exited.");

    cleanup_fd(&fd_state);
    stop_log();

	return 0;
}


