!! IMPORTANT !!
===============

I am currently testing on ATSC and DVB-C (both are in the US and standard 
service). As I do not have access to a DVB-S or DVB-T source, I am asking that 
anyone who has such service and is willing to help, get in contact with me.

Introduction
============

The ability to tune channels was [obviously] a dependency of another project 
written in Python. I put a significant amount effort into calling the standard 
dvb-apps by invoking them as external processes in a separate thread, but I 
kept having critical issues reading the output using the "subprocess" module. 
This was most likely due to a bug that I was missing, but the choice to 
refactor dvb-apps into a set of libraries, although higher risk, was obviously
going to both work and produce a far more efficient solution.

As the original utilities had also made design decisions that weren't of use to
me, it also afforded me the chance to make different decisions, as long as I
was changing things. I also did some general clean-up over the original code.

Goals
=====

> AZAP and CZAP were similar in design, but SZAP and TZAP were not, and they
  were, themselves, only vaguely similar. The libraries were made to be very 
  similar if not an identical design between all four.

> The original utilities expect a channels.conf file to exist. Now, you pass a 
  corresponding struct. This makes it feel more like a useful library than a 
  series of forced command-line utilities (less impedance mismatch).

> All output has been stripped, and status is reported via a callback.

> TZAP had support for recording the video, whereas none of the others did. 
  Since it was merely a feature of one of the utilities, it is assumed that any
  required recording will be done in the application (thus, the point of 
  requiring a tuning library), and recording is simply a matter of reading from
  the dvb<n> device and writing to a file, I yanked this functionality.

Install
=======

    ./configure
    make
    sudo make install

Usage
=====

    1) Dynamically link to zaplib.so
    2) Statically link to one of the following:

        azaplib.o
        czaplib.o
        szaplib.o (which also requires lnb.o)
        tzaplib.o

Comments
========

I do not do any handling of CTRL-BREAK. Although I usually make calls to this 
library from Python, I believe it's simply that something clears the signal 
handlers during the execution of this library, which prevents the user from 
being able to break.


