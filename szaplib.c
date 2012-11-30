/* szap -- simple zapping tool for the Linux DVB API
 *
 * szap operates on VDR (http://www.cadsoft.de/people/kls/vdr/index.htm)
 * satellite channel lists (e.g. from http://www.dxandy.de/cgi-bin/dvbchan.pl).
 * szap assumes you have a "Universal LNB" (i.e. with LOFs 9750/10600 MHz).
 *
 * Compilation: `gcc -Wall -I../../ost/include -O2 szap.c -o szap`
 *  or, if your DVB driver is in the kernel source tree:
 *              `gcc -Wall -DDVB_IN_KERNEL -O2 szap.c -o szap`
 *
 * Copyright (C) 2001 Johannes Stezenbach (js@convergence.de)
 * for convergence integrated media
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/param.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include <stdint.h>
#include <sys/time.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/audio.h>
#include "lnb.h"
#include "util.h"
#include "zaptypes.h"
#include "szaplib.h"

#ifndef TRUE
#define TRUE (1==1)
#endif
#ifndef FALSE
#define FALSE (1==0)
#endif

#define FRONTENDDEVICE "/dev/dvb/adapter%d/frontend%d"
#define DEMUXDEVICE "/dev/dvb/adapter%d/demux%d"
#define AUDIODEVICE "/dev/dvb/adapter%d/audio%d"

static struct lnb_types_st lnb_type;

struct diseqc_cmd {
   struct dvb_diseqc_master_cmd cmd;
   uint32_t wait;
};

void diseqc_send_msg(int fd, fe_sec_voltage_t v, struct diseqc_cmd *cmd,
		     fe_sec_tone_mode_t t, fe_sec_mini_cmd_t b)
{
   if (ioctl(fd, FE_SET_TONE, SEC_TONE_OFF) == -1)
      perror("FE_SET_TONE failed");
   if (ioctl(fd, FE_SET_VOLTAGE, v) == -1)
      perror("FE_SET_VOLTAGE failed");
   usleep(15 * 1000);
   if (ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &cmd->cmd) == -1)
      perror("FE_DISEQC_SEND_MASTER_CMD failed");
   usleep(cmd->wait * 1000);
   usleep(15 * 1000);
   if (ioctl(fd, FE_DISEQC_SEND_BURST, b) == -1)
      perror("FE_DISEQC_SEND_BURST failed");
   usleep(15 * 1000);
   if (ioctl(fd, FE_SET_TONE, t) == -1)
      perror("FE_SET_TONE failed");
}




/* digital satellite equipment control,
 * specification is available from http://www.eutelsat.com/
 */
static int diseqc(int secfd, int sat_no, int pol_vert, int hi_band)
{
   struct diseqc_cmd cmd =
       { {{0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00}, 4}, 0 };

   /* param: high nibble: reset bits, low nibble set bits,
    * bits are: option, position, polarization, band
    */
   cmd.cmd.msg[3] =
       0xf0 | (((sat_no * 4) & 0x0f) | (hi_band ? 1 : 0) | (pol_vert ? 0 : 2));

   diseqc_send_msg(secfd, pol_vert ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18,
		   &cmd, hi_band ? SEC_TONE_ON : SEC_TONE_OFF,
		   sat_no % 2 ? SEC_MINI_B : SEC_MINI_A);

   return TRUE;
}

static int do_tune(int fefd, unsigned int ifreq, unsigned int sr)
{
   struct dvb_frontend_parameters tuneto;
   struct dvb_frontend_event ev;

   /* discard stale QPSK events */
   while (1) {
      if (ioctl(fefd, FE_GET_EVENT, &ev) == -1)
	 break;
   }

   tuneto.frequency = ifreq;
   tuneto.inversion = INVERSION_AUTO;
   tuneto.u.qpsk.symbol_rate = sr;
   tuneto.u.qpsk.fec_inner = FEC_AUTO;

   if (ioctl(fefd, FE_SET_FRONTEND, &tuneto) == -1) {
      perror("FE_SET_FRONTEND failed");
      return FALSE;
   }

   return TRUE;
}


static
int check_frontend (int fe_fd, int dvr, const int interval_us, StatusReceiver statusReceiver)
{
    (void)dvr;
    fe_status_t status;
    uint16_t snr, signal;
    uint32_t ber, uncorrected_blocks;
    int is_locked;

    while(1) {
        if (ioctl(fe_fd, FE_READ_STATUS, &status) == -1)
            perror("FE_READ_STATUS failed");
        /* some frontends might not support all these ioctls, thus we
         * avoid printing errors */
        if (ioctl(fe_fd, FE_READ_SIGNAL_STRENGTH, &signal) == -1)
            signal = -2;
        if (ioctl(fe_fd, FE_READ_SNR, &snr) == -1)
            snr = -2;
        if(ioctl(fe_fd, FE_READ_BER, &ber) == -1)
            ber = -2;
        if(ioctl(fe_fd, FE_READ_UNCORRECTED_BLOCKS, &uncorrected_blocks) == -1)
            uncorrected_blocks = -2;

        is_locked = (status & FE_HAS_LOCK) > 0;

		if(statusReceiver(status, signal, snr, ber, uncorrected_blocks, is_locked) == 0)
            break;

		usleep(interval_us);
   }

   return 0;
}


static
int zap_to(t_tuner_descriptor tuner,
      unsigned int sat_no, unsigned int freq, unsigned int pol,
      unsigned int sr, unsigned int vpid, unsigned int apid, int sid,
      int dvr, int rec_psi, int bypass, int interval_us, StatusReceiver statusReceiver)
{
   char fedev[128], dmxdev[128], auddev[128];
   static int fefd, dmxfda, dmxfdv, audiofd = -1, patfd, pmtfd;
   int pmtpid;
   uint32_t ifreq;
   int hiband, result;
   static struct dvb_frontend_info fe_info;

   if (!fefd) {
      snprintf(fedev, sizeof(fedev), FRONTENDDEVICE, tuner.adapter, tuner.frontend);
      snprintf(dmxdev, sizeof(dmxdev), DEMUXDEVICE, tuner.adapter, tuner.demux);
      snprintf(auddev, sizeof(auddev), AUDIODEVICE, tuner.adapter, tuner.demux);
      printf("using '%s' and '%s'\n", fedev, dmxdev);

      if ((fefd = open(fedev, O_RDWR | O_NONBLOCK)) < 0) {
	 perror("opening frontend failed");
	 return FALSE;
      }

      result = ioctl(fefd, FE_GET_INFO, &fe_info);

      if (result < 0) {
	 perror("ioctl FE_GET_INFO failed");
	 close(fefd);
	 return FALSE;
      }

      if (fe_info.type != FE_QPSK) {
	 fprintf(stderr, "frontend device is not a QPSK (DVB-S) device!\n");
	 close(fefd);
	 return FALSE;
      }

      if ((dmxfdv = open(dmxdev, O_RDWR)) < 0) {
	 perror("opening video demux failed");
	 close(fefd);
	 return FALSE;
      }

      if ((dmxfda = open(dmxdev, O_RDWR)) < 0) {
	 perror("opening audio demux failed");
	 close(fefd);
	 return FALSE;
      }

      if (dvr == 0)	/* DMX_OUT_DECODER */
	 audiofd = open(auddev, O_RDWR);

      if (rec_psi){
         if ((patfd = open(dmxdev, O_RDWR)) < 0) {
	    perror("opening pat demux failed");
	    close(audiofd);
	    close(dmxfda);
	    close(dmxfdv);
	    close(fefd);
	    return FALSE;
         }

         if ((pmtfd = open(dmxdev, O_RDWR)) < 0) {
	    perror("opening pmt demux failed");
	    close(patfd);
	    close(audiofd);
	    close(dmxfda);
	    close(dmxfdv);
	    close(fefd);
	    return FALSE;
         }
      }
   }

   hiband = 0;
   if (lnb_type.switch_val && lnb_type.high_val &&
	freq >= lnb_type.switch_val)
	hiband = 1;

   if (hiband)
      ifreq = freq - lnb_type.high_val;
   else {
      if (freq < lnb_type.low_val)
          ifreq = lnb_type.low_val - freq;
      else
          ifreq = freq - lnb_type.low_val;
   }
   result = FALSE;

   if (diseqc(fefd, sat_no, pol, hiband))
      if (do_tune(fefd, ifreq, sr))
	 if (set_pesfilter(dmxfdv, vpid, DMX_PES_VIDEO, dvr))
	    if (audiofd >= 0)
	       (void)ioctl(audiofd, AUDIO_SET_BYPASS_MODE, bypass);
	    if (set_pesfilter(dmxfda, apid, DMX_PES_AUDIO, dvr)) {
	       if (rec_psi) {
	          pmtpid = get_pmt_pid(dmxdev, sid);
		  if (pmtpid < 0) {
		     result = FALSE;
		  }
		  if (pmtpid == 0) {
		     fprintf(stderr,"couldn't find pmt-pid for sid %04x\n",sid);
		     result = FALSE;
		  }
		  if (set_pesfilter(patfd, 0, DMX_PES_OTHER, dvr))
	             if (set_pesfilter(pmtfd, pmtpid, DMX_PES_OTHER, dvr))
	                result = TRUE;
	          } else {
		    result = TRUE;
		  }
	       }

    check_frontend (fefd, dvr, interval_us, statusReceiver);

    close(patfd);
    close(pmtfd);

    if (audiofd >= 0)
	    close(audiofd);

    close(dmxfda);
    close(dmxfdv);
    close(fefd);

   return result;
}

static int read_channels(t_tuner_descriptor tuner, t_dvbs_tune_info tune_info, 
                         int dvr, int rec_psi, int bypass, int interval_us, 
                         StatusReceiver statusReceiver)
{
    unsigned int vpid, apid;

    vpid = (tune_info.vpid ? tune_info.vpid : 0x1fff);
    apid = (tune_info.apid ? tune_info.apid : 0x1fff);

	return zap_to(tuner, tune_info.sat_no, tune_info.freq * 1000, 
	              tune_info.pol, tune_info.sr, vpid, apid, tune_info.sid, dvr, 
	              rec_psi, bypass, interval_us, statusReceiver);
}


int szap_tune_silent(t_tuner_descriptor tuner, t_dvbs_tune_info tune_info, 
                     StatusReceiver statusReceiver, int audio_bypass, int dvr, 
                     unsigned int rec_psi, char *lnb_raw)
{
	const int status_interval_us = 1000000;

    lnb_type = *lnb_enum(0);

    if(lnb_raw != NULL && lnb_decode(lnb_raw, &lnb_type) < 0) 
        return -1;

    lnb_type.low_val *= 1000;	/* convert to kiloherz */
    lnb_type.high_val *= 1000;	/* convert to kiloherz */
    lnb_type.switch_val *= 1000;	/* convert to kiloherz */

    if(rec_psi)
        dvr = 1;

    if (!read_channels(tuner, tune_info, dvr, rec_psi, audio_bypass, 
                       status_interval_us, statusReceiver))
        return -1;

   return 0;
}

