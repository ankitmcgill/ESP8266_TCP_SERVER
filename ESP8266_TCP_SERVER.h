/****************************************************************
* ESP8266 TCP SERVER LIBRARY
*
* FOR NOW ONLY SUPPORTS 1 CLIENT !
*
* JUNE 18 2017
*
* ANKIT BHATNAGAR
* ANKIT.BHATNAGARINDIA@GMAIL.COM
*
* REFERENCES
* ------------
*
****************************************************************/

#ifndef _ESP8266_TCP_SERVER_H_
#define _ESP8266_TCP_SERVER_H_

#include "osapi.h"
#include "mem.h"
#include "ets_sys.h"
#include "ip_addr.h"
#include "espconn.h"
#include "os_type.h"

#define ESP8266_TCP_SERVER_MAX_CLIENT_COUNT 		1
#define ESP8266_TCP_SERVER_MAX_PATH_CALLBACKS 		5
#define ESP8266_TCP_SERVER_HTTP_GET_REQUEST_ENDING	"\r\n\r\n"

//CUSTOM VARIABLE STRUCTURES/////////////////////////////
typedef enum
{
	ESP8266_TCP_SERVER_SYSTEM_MODE_STATION = 0,
	ESP8266_TCP_SERVER_SYSTEM_MODE_SOFTAP = 1
}ESP8266_TCP_SERVER_SYSTEM_MODE;

typedef struct
{
	char* path_string;
	void (*path_cb_fn)(void);
	uint8_t path_found;
}ESP8266_TCP_SERVER_PATH_CB_ENTRY;
//END CUSTOM VARIABLE STRUCTURES/////////////////////////

//FUNCTION PROTOTYPES/////////////////////////////////////
//CONFIGURATION FUNCTIONS
void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_SetDebug(uint8_t debug_on);
void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_Initialize(uint16_t local_port,
															uint32_t tcp_timeout,
															ESP8266_TCP_SERVER_SYSTEM_MODE mode);
void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_SetDataEndingString(char* data_ending);
void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_SetCallbackFunctions(void (*tcp_con_cb)(void*),
																	void (*tcp_discon_cb)(void*),
																	void (*tcp_recon_cb)(void*),
																	void (tcp_sent_cb)(void*),
																	void (tcp_recv_cb)(void*, char*, unsigned short));
void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_RegisterUrlPathCb(ESP8266_TCP_SERVER_PATH_CB_ENTRY entry);

//GET PARAMETERS FUNCTIONS
uint8_t ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_GetConnectedClientList(struct ip_addr* list);
uint16_t ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_GetServerPort(void);
struct ip_info ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_GetServerIp(void);
uint32_t ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_GetTCPTimeoutSeconds(void);

//CONTROL FUNCTIONS
void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_Start(void);
void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_Stop(void);
void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_DisconnectAllClients(void);
void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_SendData(char* data, uint16_t len);

//INTERNAL CALLBACK FUNCTIONS
void ICACHE_FLASH_ATTR _esp8266_tcp_server_connect_cb(void* arg);
void ICACHE_FLASH_ATTR _esp8266_tcp_server_disconnect_cb(void* arg);
void ICACHE_FLASH_ATTR _esp8266_tcp_server_reconnect_cb(void* arg);
void ICACHE_FLASH_ATTR _esp8266_tcp_server_sent_cb(void* arg);
void ICACHE_FLASH_ATTR _esp8266_tcp_server_receive_cb(void* arg, char* pusrdata, unsigned short length);

//INTERNAL DATA PROCESSING FUNCTION
uint8_t ICACHE_FLASH_ATTR _esp8266_tcp_server_received_data_process(char* data, uint16_t len);
//END FUNCTION PROTOTYPES/////////////////////////////////
#endif
