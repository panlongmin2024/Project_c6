//----------------------------------------------------------------------------//
#ifndef __WIFI_API_H
#define __WIFI_API_H

#ifdef __cplusplus
  extern "C" {
#endif

typedef void (*evt_handler)(uint32_t nEvtId, void *data, int32_t len);

void netdev_status_change_cb(void *netif);
void reg_wifi_evt_cb(evt_handler evtcb);
int Cli_RunCmd_arg(int argc, char *argv[]);
int Cli_RunCmd(char *CmdBuffer);

#ifdef __cplusplus
  }
#endif

#endif // __WIFI_API_H

//----------------------------------------------------------------------------//
