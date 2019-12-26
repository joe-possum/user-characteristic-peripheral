/***************************************************************************//**
 * @file
 * @brief Event handling and application code for Empty NCP Host application example
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

/* standard library headers */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

/* BG stack headers */
#include "bg_types.h"
#include "gecko_bglib.h"

/* Own header */
#include "app.h"
#include "dump.h"
#include "support.h"

// App booted flag
static bool appBooted = false;
static struct {
  double timeout;
  char *name;
  uint32 advertising_interval;
  uint16 connection_interval, mtu; 
  bd_addr remote;
  uint8 advertise, connection, reset;
} config = { .remote = { .addr = {0,0,0,0,0,0}},
	     .connection = 0xff,
	     .advertise = 1,
	     .name = NULL,
	     .advertising_interval = 160, // 100 ms
	     .connection_interval = 80, // 100 ms
	     .mtu = 23,
	     .reset = 0,
	     .timeout = 5.0d,
};
  
void parse_address(const char *fmt,bd_addr *address) {
  char buf[3];
  int octet;
  for(uint8 i = 0; i < 6; i++) {
    memcpy(buf,&fmt[3*i],2);
    buf[2] = 0;
    sscanf(buf,"%02x",&octet);
    address->addr[5-i] = octet;
  }
}

uint8 ad_flags(uint8 *buffer, uint flags) {
  if(0 == flags) return 0;
  buffer[0] = 2;
  buffer[1] = 1;
  buffer[2] = flags;
  return 3;
}

uint8 ad_name(uint8 *buffer, char *name) {
  uint8 len = strlen(name);
  buffer[0] = len + 1;
  buffer[1] = 9;
  memcpy(&buffer[2],(uint8*)&name[0],len);
  return len + 2;
}

uint8 ad_manufacturer(uint8 *buffer, uint8 *data, uint8 len) {
  buffer[0] = len + 3;
  buffer[1] = 0xff;
  buffer[2] = 0xff;
  buffer[3] = 0xff;
  memcpy(&buffer[4],data,len);
  return len + 4;
}

const char *getAppOptions(void) {
  return "l<level>";
}

void appOption(int option, const char *arg) {
  switch(option) {
  case 'l':
    config.reset = atoi(arg);
    break;
  case 't':
    sscanf(arg,"%lf",&config.timeout);
    break;
  default:
    fprintf(stderr,"Unhandled option '-%c'\n",option);
    exit(1);
  }
}

void appInit(void) {
  if(config.advertise) return;
  for(int i = 0; i < 6; i++) {
    if(config.remote.addr[i]) return;
  }
  printf("Usage: bt-system-reset [ -l <level> ]\n");
  exit(1);
}

struct stopwatch {
  struct timeval start, stop;
  double value;
  uint8 running, cached;
};

void stw_start(struct stopwatch *ptr) {
  gettimeofday(&ptr->start,NULL);
  ptr->running = 1;
  ptr->cached = 0;
}

void stw_stop(struct stopwatch *ptr) {
  gettimeofday(&ptr->start,NULL);
  ptr->running = 0;
}

double stw_read(struct stopwatch *ptr) {
  if(ptr->running) {
    gettimeofday(&ptr->stop,NULL);
    return ptr->stop.tv_sec - ptr->start.tv_sec \
      + 1e-6d*(ptr->stop.tv_usec - ptr->start.tv_usec);
  } else if (ptr->cached) return ptr->value;
  else {
    ptr->value = ptr->stop.tv_sec - ptr->start.tv_sec \
      + 1e-6d*(ptr->stop.tv_usec - ptr->start.tv_usec);
    ptr->cached = 1;
    return ptr->value;
  }
}

struct stopwatch sw;

/***********************************************************************************************//**
 *  \brief  Event handler function.
 *  \param[in] evt Event pointer.
 **************************************************************************************************/
void appHandleEvents(struct gecko_cmd_packet *evt)
{
  if (NULL == evt) {
    if (stw_read(&sw) > config.timeout) {
      printf("Exiting due to timeout waiting for response\n");
      exit(1);
    }
    return;
  }

  // Do not handle any events until system is booted up properly.
  if ((BGLIB_MSG_ID(evt->header) != gecko_evt_system_boot_id)
      && !appBooted) {
#if defined(DEBUG)
    printf("Event: 0x%04x\n", BGLIB_MSG_ID(evt->header));
#endif
    millisleep(50);
    return;
  }

  /* Handle events */
#ifdef DUMP
  switch (BGLIB_MSG_ID(evt->header)) {
  default:
    dump_event(evt);
  }
#endif
  switch (BGLIB_MSG_ID(evt->header)) {
  case gecko_evt_system_boot_id:
    appBooted = true;
    if(0xff == config.reset) {
      stw_stop(&sw);
      printf("Reboot took %lf seconds\n",stw_read(&sw));
      exit(0);
    }
    gecko_cmd_system_reset(config.reset);
    stw_start(&sw);
    config.reset = 0xff;
    break;

  default:
    break;
  }
}
