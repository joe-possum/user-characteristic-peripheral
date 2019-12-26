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
#include <ctype.h>

/* BG stack headers */
#include "bg_types.h"
#include "gecko_bglib.h"

/* Own header */
#include "app.h"
#include "dump.h"

// App booted flag
static bool appBooted = false;
static struct {
  int16 tx_power;
  uint32 interval;
  uint8 *adv_data;
  uint8 adv_len;
} config = {
	    .interval = 1600,
	    .adv_data = NULL,
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

const char *getAppOptions(void) {
  return "p<tx-power>i<interval>n<name>";
}

void appOption(int option, const char *arg) {
  double v;
  switch(option) {
  case 'p':
    sscanf(arg,"%lf",&v);
    config.tx_power = round(10*v);
    break;
  case 'i':
    config.interval = atoi(arg);
    break;
  case 'n':
    config.adv_len = strlen(arg)+5;
    config.adv_data = malloc(config.adv_len);
    memcpy(config.adv_data,"\x02\x01\x06\xff\x09",5);
    config.adv_data[3] = strlen(arg)+1;
    memcpy(config.adv_data+5,arg,strlen(arg));
    break;
  default:
    fprintf(stderr,"Unhandled option '-%c'\n",option);
    exit(1);
  }
}

void appInit(void) {
  if(config.adv_data) {
    printf("adv_data = \"");
    for(int i = 0; i < config.adv_len; i++) {
      int byte = config.adv_data[i];
      if(isprint(byte)) printf("%c",byte);
      else printf("\\x%02x",byte);
    }
    printf("\"\n");
  }
}

/***********************************************************************************************//**
 *  \brief  Event handler function.
 *  \param[in] evt Event pointer.
 **************************************************************************************************/
void appHandleEvents(struct gecko_cmd_packet *evt)
{
  if (NULL == evt) {
    return;
  }

  // Do not handle any events until system is booted up properly.
  if ((BGLIB_MSG_ID(evt->header) != gecko_evt_system_boot_id)
      && !appBooted) {
#if defined(DEBUG)
    printf("Event: 0x%04x\n", BGLIB_MSG_ID(evt->header));
#endif
    usleep(50000);
    return;
  }

  /* Handle events */
#ifdef DUMP
  dump_event(evt);
#endif
  switch (BGLIB_MSG_ID(evt->header)) {
    case gecko_evt_system_boot_id:
      appBooted = true;
      gecko_cmd_system_set_tx_power(config.tx_power);
      gecko_cmd_system_get_bt_address();
      gecko_cmd_le_gap_set_advertise_timing(0,config.interval,config.interval,0,0);
      if(config.adv_data) gecko_cmd_le_gap_bt5_set_adv_data(0,0,config.adv_len,config.adv_data);
      // no break
    case gecko_evt_le_connection_closed_id:
      if(config.adv_data) {
	gecko_cmd_le_gap_start_advertising(0,le_gap_user_data,le_gap_connectable_scannable);
      } else {
	gecko_cmd_le_gap_start_advertising(0,le_gap_general_discoverable, le_gap_connectable_scannable);
      }
      break;

  case gecko_evt_gatt_mtu_exchanged_id:
#define ED evt->data.evt_gatt_mtu_exchanged
    gecko_cmd_le_connection_set_parameters(ED.connection,24,24,0,20);
    break;
#undef ED

  case gecko_evt_gatt_server_user_read_request_id: /*********************************************** gatt_server_user_read_request **/
#define ED evt->data.evt_gatt_server_user_read_request
    gecko_cmd_gatt_server_send_user_read_response(ED.connection,ED.characteristic,0,13,(uint8*)"Hello, World!");
    break;
#undef ED

  case gecko_evt_gatt_server_user_write_request_id: /********************************************* gatt_server_user_write_request **/
#define ED evt->data.evt_gatt_server_user_write_request
    if(0 == (rand() & 15)) gecko_cmd_le_connection_close(ED.connection);
    gecko_cmd_gatt_server_send_user_write_response(ED.connection,ED.characteristic,0);
    gecko_cmd_gatt_server_send_characteristic_notification(ED.connection,0x18,1,&ED.connection);

    break;
#undef ED
default:
      break;
  }
}
