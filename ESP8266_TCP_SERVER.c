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
*		(1) Espressif Sample Codes
*				http://espressif.com/en/support/explore/sample-codes
*
*		(2) https://lujji.github.io/blog/esp-httpd/
*
*   (3) https://www.tutorialspoint.com/http/http_responses.htm
****************************************************************/

#include "ESP8266_TCP_SERVER.h"

//LOCAL LIBRARY VARIABLES/////////////////////////////////////
//DEBUG RELATRED
static uint8_t _esp8266_tcp_server_debug;

//TCP RELATED
static esp_tcp _esp8266_tcp_server_tcp;
static struct espconn _esp8266_tcp_server_espconn;
static struct espconn* _esp8266_tcp_server_client_conection;

//IP / HOSTNAME RELATED
static struct ip_info _self_ip;
static uint16_t _self_port;
static uint16_t _self_tcp_timeout_seconds;

//USER DATA RELATED
static char* _esp8266_tcp_server_data_ending_string;
static uint8_t _esp8266_tcp_server_registered_path_cb_count;
ESP8266_TCP_SERVER_PATH_CB_ENTRY _esp8266_path_callbacks[ESP8266_TCP_SERVER_MAX_PATH_CALLBACKS];
static uint8_t _esp8266_tcp_server_path_found;

//CALLBACK FUNCTION POINTERS
static void (*_esp8266_tcp_server_tcp_conn_cb)(void*);
static void (*_esp8266_tcp_server_tcp_discon_cb)(void*);
static void (*_esp8266_tcp_server_tcp_recon_cb)(void*);
static void (*_esp8266_tcp_server_tcp_sent_cb)(void*);
static void (*_esp8266_tcp_server_tcp_recv_cb)(void*, char*, unsigned short);

//END LOCAL LIBRARY VARIABLES/////////////////////////////////

//CONFIGURATION FUNCTIONS
void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_SetDebug(uint8_t debug_on)
{
    //SET DEBUG PRINTF ON(1) OR OFF(0)

    _esp8266_tcp_server_debug = debug_on;
}

void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_Initialize(uint16_t local_port,
															uint32_t tcp_timeout,
															ESP8266_TCP_SERVER_SYSTEM_MODE mode)
{
	//INITIALIZE THE TCP SERVER AT THE SPECIFIED PORT WITH SPECIFIED BUFFER SIZE
	//FOR INCOMING DATA

	//CHECK TCP TIMEOUT NOT MORE THAN 7200 SECONDS (2 HOURS)
	if(tcp_timeout > 7200)
	{
		//TCP TIMEOUT ERROR
		if(_esp8266_tcp_server_debug)
		{
			os_printf("ESP8266 : TCP SERVER : TCP timeout cannot be more than 7200 seconds!\n");
		}
		return;
	}

	//SET UP TCP PARAMETERS
	_esp8266_tcp_server_tcp.local_port = local_port;
	_esp8266_tcp_server_espconn.type = ESPCONN_TCP;
	_esp8266_tcp_server_espconn.state = ESPCONN_NONE;
	_esp8266_tcp_server_espconn.proto.tcp = &_esp8266_tcp_server_tcp;

	//STORE THE SERVER PARAMETERS
	wifi_get_ip_info(mode, &_self_ip);
	_self_port = local_port;
	_self_tcp_timeout_seconds = tcp_timeout;

  //REGISTER CONNECT CB
  espconn_regist_connectcb(&_esp8266_tcp_server_espconn, _esp8266_tcp_server_connect_cb);

	//SET THE DATA ENDING STRING TO NULL
	_esp8266_tcp_server_data_ending_string = NULL;

	//RESET THE PATHS CALLBACKS COUNT
	_esp8266_tcp_server_registered_path_cb_count = 0;

	if(_esp8266_tcp_server_debug)
	{
		os_printf("ESP8266 : TCP SERVER : Initialized tcp server with parameters!\n");
		os_printf("ESP8266 : TCP SERVER : server ip = %d.%d.%d.%d\n", IP2STR(&_self_ip));
		os_printf("ESP8266 : TCP SERVER : server port = %u\n", _self_port);
		os_printf("ESP8266 : TCP SERVER : tcp timeout (s) = %u\n", _self_tcp_timeout_seconds);
	}
}

void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_SetDataEndingString(char* data_ending)
{
	//SET THE DATA ENDING CHARACTERS FOR THE INCOMING DATA
	//USER CAN ALSO USE THE LIBRARY PROVIDED ENDING ESP8266_TCP_SERVER_HTTP_GET_REQUEST_ENDING
	//FOR HTTP GET REQUESTS

	_esp8266_tcp_server_data_ending_string = data_ending;

	if(_esp8266_tcp_server_debug)
	{
		os_printf("ESP8266 : TCP SERVER : Data ending string set to %s!\n", _esp8266_tcp_server_data_ending_string);
	}
}

void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_SetCallbackFunctions(void (*tcp_con_cb)(void*),
																	void (*tcp_discon_cb)(void*),
																	void (*tcp_recon_cb)(void*),
																	void (tcp_sent_cb)(void*),
																	void (tcp_recv_cb)(void*, char*, unsigned short))
{
	//REGISTER USER CB FUNCTIONS FOR VARIOUS TCP EVENTS

	_esp8266_tcp_server_tcp_conn_cb = tcp_con_cb;
	_esp8266_tcp_server_tcp_discon_cb = tcp_discon_cb;
	_esp8266_tcp_server_tcp_recon_cb = tcp_recon_cb;
	_esp8266_tcp_server_tcp_sent_cb = tcp_sent_cb;
	_esp8266_tcp_server_tcp_recv_cb = tcp_recv_cb;

	if(_esp8266_tcp_server_debug)
	{
		os_printf("ESP8266 : TCP SERVER : User tcp cb registered!\n");
	}
}

void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_RegisterUrlPathCb(ESP8266_TCP_SERVER_PATH_CB_ENTRY entry)
{
	//REGISTER CB FUNCTION FOR SPECIFIED URL PATH
	//USEFULL FOR HTTP REQEUSTS

	//CHECK WE HAVE SPACE TO ADD ANOTHER PATH CALLBACK
	if(_esp8266_tcp_server_registered_path_cb_count >= ESP8266_TCP_SERVER_MAX_PATH_CALLBACKS)
	{
		//LIMIT EXCEEDED
		if(_esp8266_tcp_server_debug)
		{
			os_printf("ESP8266 : TCP SERVER :No more path callbacks can be registered!\n");
		}
		return;
	}

	//SPACE AVAILABLE TO ADD NEW CALLBACK
	_esp8266_path_callbacks[_esp8266_tcp_server_registered_path_cb_count] = entry;
	_esp8266_tcp_server_registered_path_cb_count += 1;

	if(_esp8266_tcp_server_debug)
	{
		os_printf("ESP8266 : TCP SERVER : Added path cb for %s\n", entry.path_string);
	}
}

//GET PARAMETERS FUNCTIONS
uint8_t ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_GetConnectedClientList(struct ip_addr* list)
{
	//RETURN THE IP LIST OF CURRENTLY CONNECTED CLIENTS TO THE SERVER IN THE SUPPLIED POINTER
	//FUNCTION RETURNS THE NUMBER OF CLIENTS CONNECTED (0 IF NONE)

	remot_info* rinfo = NULL;
	uint8_t count = 0;

	os_printf("ESP8266 : TCP SERVER : Client list\n");
	if(espconn_get_connection_info(&_esp8266_tcp_server_espconn, &rinfo, 0) == ESPCONN_OK)
	{
		for(count = 0; count < _esp8266_tcp_server_espconn.link_cnt; count++)
		{
			os_printf("ESP8266 : TCP SERVER : client # %u ip %d.%d.%d.%d disconnected\n", count, IP2STR(rinfo[count].remote_ip));

			//POPULATE THE ESPCONN STRCUTURE WITH REMOTE INFO
			IP4_ADDR((list + count), rinfo[count].remote_ip[0], rinfo[count].remote_ip[1], rinfo[count].remote_ip[2], rinfo[count].remote_ip[3]);
		}
	}
	return count;
}

uint16_t ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_GetServerPort(void)
{
	//RETURN THE PORT THE TCP SERVER IS RUNNING ON

	return _self_port;
}

struct ip_info ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_GetServerIp(void)
{
	//RETURN THE IP ADDRESS OF THE SERVER

	return _self_ip;
}

uint32_t ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_GetTCPTimeoutSeconds(void)
{
	//RETURN THE CURRENTLY SET TCP TIMEOUT SECONDS VALUE

	return _self_tcp_timeout_seconds;
}

//CONTROL FUNCTIONS
void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_Start(void)
{
	//START THE TCP SERVER AND START LISTENING

	//SET THE MAXIMUM NUMBER OF CONNECTIONS ALLOWED
	espconn_tcp_set_max_con(ESP8266_TCP_SERVER_MAX_CLIENT_COUNT);

	//START SERVER LISTENING
	espconn_accept(&_esp8266_tcp_server_espconn);

	//SET THE TCP TIMEOUT VALUE FOR ALL TCP CONNECTIONS
	espconn_regist_time(&_esp8266_tcp_server_espconn, _self_tcp_timeout_seconds, 0);

	if(_esp8266_tcp_server_debug)
	{
		os_printf("ESP8266 : TCP SERVER : Server started\n");
	}
}

void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_Stop(void)
{
	//DISCONNECT ALL CLIENT + STOP SERVER (STOP LISTENING)

	//DISCONNECT ALL EXISTING CLIENTS
	ESP8266_TCP_SERVER_DisconnectAllClients();

	//STOP THE SERVER
	espconn_delete(&_esp8266_tcp_server_espconn);

	if(_esp8266_tcp_server_debug)
	{
		os_printf("ESP8266 : TCP SERVER : Server stopped\n");
	}
}

void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_DisconnectAllClients(void)
{
	//DISCONNECT ALL CURRENTLY CONNECTED CLIENTS

	struct espconn econn;
	remot_info* rinfo = NULL;
	uint8_t count = 0;

	if(espconn_get_connection_info(&econn, &rinfo, 0) == ESPCONN_OK)
	{
		for(count = 0; count < econn.link_cnt; count++)
		{
			os_printf("ESP8266 : TCP SERVER : client ip %d.%d.%d.%d disconnected\n", IP2STR(rinfo[count].remote_ip));

			//POPULATE THE ESPCONN STRCUTURE WITH REMOTE INFO
			econn.proto.tcp->remote_port = rinfo[count].remote_port;
			econn.proto.tcp->remote_ip[0] = rinfo[count].remote_ip[0];
			econn.proto.tcp->remote_ip[1] = rinfo[count].remote_ip[1];
			econn.proto.tcp->remote_ip[2] = rinfo[count].remote_ip[2];
			econn.proto.tcp->remote_ip[3] = rinfo[count].remote_ip[3];

			//DISCONNECT CLIENT
			espconn_disconnect(&econn);
		}
	}

	if(_esp8266_tcp_server_debug)
	{
		os_printf("ESP8266 : TCP SERVER : All clients disconnected from server\n");
	}
}

void ICACHE_FLASH_ATTR ESP8266_TCP_SERVER_SendData(const char* data, uint16_t len, uint8_t disconnect)
{
	//SEND THE DATA OF THE SPECIFIED LENGTH TO THE CLIENT

	espconn_send(_esp8266_tcp_server_client_conection, (uint8_t*)data, len);
  if(disconnect)
  {
    espconn_disconnect(_esp8266_tcp_server_client_conection);
  }
}

//INTERNAL CALLBACK FUNCTIONS
void ICACHE_FLASH_ATTR _esp8266_tcp_server_connect_cb(void* arg)
{
	//INTERNAL TCP CLIENT CONNECT CB

	if(_esp8266_tcp_server_debug)
	{
		os_printf("ESP8266 : TCP SERVER : connect cb\n");
	}

	//STORE THE NEW CLIENT CONNECTION
	_esp8266_tcp_server_client_conection = (struct espconn*)arg;

	if(_esp8266_tcp_server_debug)
	{
		os_printf("ESP8266 : TCP SERVER : New connection received from %d.%d.%d.%d\n", IP2STR(((struct espconn*)arg)->proto.tcp->remote_ip));
	}

  //REGISTER CLIENT CONNECTION CALLBACKS
  espconn_regist_recvcb(_esp8266_tcp_server_client_conection, _esp8266_tcp_server_receive_cb);
  espconn_regist_reconcb(_esp8266_tcp_server_client_conection, _esp8266_tcp_server_reconnect_cb);
  espconn_regist_disconcb(_esp8266_tcp_server_client_conection, _esp8266_tcp_server_disconnect_cb);
  espconn_regist_sentcb(_esp8266_tcp_server_client_conection, _esp8266_tcp_server_sent_cb);

	//CALL USER CB IF NOT NULL
	if(_esp8266_tcp_server_tcp_conn_cb != NULL)
	{
		(*_esp8266_tcp_server_tcp_conn_cb)(arg);
	}
}

void ICACHE_FLASH_ATTR _esp8266_tcp_server_disconnect_cb(void* arg)
{
	//INTERNAL TCP CLIENT DISCONNECT CB

	if(_esp8266_tcp_server_debug)
	{
		os_printf("ESP8266 : TCP SERVER : Disconnect received from %d.%d.%d.%d\n", IP2STR(((struct espconn*)arg)->proto.tcp->remote_ip));
	}

	//CONNECTION WILL AUTOMATICALLY BE REMOVED FROM THE CLIENT LIST
	_esp8266_tcp_server_client_conection = NULL;

	//CALL USER CB IF NOT NULL
	if(_esp8266_tcp_server_tcp_discon_cb != NULL)
	{
		(*_esp8266_tcp_server_tcp_discon_cb)(arg);
	}
}

void ICACHE_FLASH_ATTR _esp8266_tcp_server_reconnect_cb(void* arg, int8_t err)
{
	//INTERNAL TCP CLIENT RECONNECT CB

	if(_esp8266_tcp_server_debug)
	{
		os_printf("ESP8266 : TCP SERVER : Reconnect received from %d.%d.%d.%d\n", IP2STR(((struct espconn*)arg)->proto.tcp->remote_ip));
	}

	//CALL USER CB IF NOT NULL
	if(_esp8266_tcp_server_tcp_recon_cb != NULL)
	{
		(*_esp8266_tcp_server_tcp_recon_cb)(arg);
	}
}

void ICACHE_FLASH_ATTR _esp8266_tcp_server_sent_cb(void* arg)
{
	//INTERNAL TCP DATA SENT CB

	if(_esp8266_tcp_server_debug)
	{
		os_printf("ESP8266 : TCP SERVER : data sent to %d.%d.%d.%d\n", IP2STR(((struct espconn*)arg)->proto.tcp->remote_ip));
	}

	//CALL USER CB IF NOT NULL
	if(_esp8266_tcp_server_tcp_sent_cb != NULL)
	{
		(*_esp8266_tcp_server_tcp_sent_cb)(arg);
	}
}

void ICACHE_FLASH_ATTR _esp8266_tcp_server_receive_cb(void* arg, char* pusrdata, unsigned short length)
{
	//INTERNAL TCP DATA RECEIVED CB

	if(_esp8266_tcp_server_debug)
	{
		os_printf("ESP8266 : TCP SERVER : data received\n");
		os_printf("from %d.%d.%d.%d\n", IP2STR(((struct espconn*)arg)->proto.tcp->remote_ip));
		os_printf("length = %u\n", length);
    os_printf("data = %s\n", pusrdata);
	}

	//DO DATA PROCESSING
	if(_esp8266_tcp_server_received_data_process(pusrdata, length) == 1)
	{
		//DATA ENDING STRING MATCHED

		if(_esp8266_tcp_server_path_found == 0)
    {
      //NO PATH WAS FOUND IN REQUEST
      //SEND 404 MESSAGE
      ESP8266_TCP_SERVER_SendData((char*)&ESP8266_TCP_SERVER_HTTP_GET_RESPONSE_404_HEADER, strlen(ESP8266_TCP_SERVER_HTTP_GET_RESPONSE_404_HEADER), 1);
      return;
    }

		//GO THROUGH ALL THE PATHS AND CALL THEIR CALLBACKS IF THEY WERE FOUND
		uint8_t count = 0;
		while(count < _esp8266_tcp_server_registered_path_cb_count)
		{
			if(_esp8266_path_callbacks[count].path_found == 1)
			{
				//RESET PATH FOUND TO 0
				_esp8266_path_callbacks[count].path_found = 0;
        //SEND PATH RESPONSE
        ESP8266_TCP_SERVER_SendData(_esp8266_path_callbacks[count].path_response, strlen(_esp8266_path_callbacks[count].path_response), 1);
        //INITIATE CALLBACK
				(*_esp8266_path_callbacks[count].path_cb_fn)();
			}
      count += 1;
		}

    //CALL USER CB IF NOT NULL
		if(_esp8266_tcp_server_tcp_recv_cb != NULL)
		{
			(*_esp8266_tcp_server_tcp_recv_cb)(arg, pusrdata, length);
		}
	}
}

//INTERNAL DATA PROCESSING FUNCTION
uint8_t ICACHE_FLASH_ATTR _esp8266_tcp_server_received_data_process(char* data, uint16_t len)
{
	//PROCESS THE RECEIVED DATA TO VALIDATE DATA (AS PER USER SPECIFICATION) AND TO LOOK
	//FOR URL PATH

  //ENSURE THE DATA IS HTTP 1.1
  if(strstr(data, "HTTP/1.1") == NULL || strstr(data, "GET") == NULL)
  {
      //NOT VALID HTTP GET DATA
      if(_esp8266_tcp_server_debug)
  		{
  			os_printf("ESP8266 : TCP SERVER : invalid http get data\n");
  		}
      return 0;
  }

	//CHECK FOR CONFIGURED PATHS IN THE DATA
	uint8_t count = 0;
  _esp8266_tcp_server_path_found = 0;
	while(count < _esp8266_tcp_server_registered_path_cb_count)
	{
		if(strstr(data, _esp8266_path_callbacks[count].path_string) != NULL)
		{
			//PATH FOUND
			//MARK IT FOUND
			_esp8266_path_callbacks[count].path_found = 1;
      _esp8266_tcp_server_path_found = 1;
		}
		else
		{
			//PATH NOT FOUND
			//MARK IT NOT FOUND
			_esp8266_path_callbacks[count].path_found = 0;
		}
		count += 1;
	}

	//CHECK IF DATA ENDING STRING FOUND
	if(strstr(data, _esp8266_tcp_server_data_ending_string) != NULL)
	{
		//DATA ENDING STRING PRESENT
		if(_esp8266_tcp_server_debug)
		{
			os_printf("ESP8266 : TCP SERVER : data ending string found!\n");
		}
		return 1;
	}
	return 0;
}
