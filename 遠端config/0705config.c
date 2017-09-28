//Will remoteconfig   6/24

#include "twOSPort.h"
#include "twLogger.h"
#include "twApi.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

// (rtwang20150824)
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <syslog.h>


char * thingName = "103GW0000000000";
#define TW_HOST "52.193.113.239"
#define TW_APP_KEY "cf0c38c9-d6b5-4536-a61c-0afa962ff51a"
#define TW_PORT 443
#define FILE_NAME "record.txt"
#define Com1 0
#define Com2 4
#define ID5 1
#define ID6 2
#define DATA_COLLECTION_RATE_MSEC 5000



int handle = 0;  //set seriesport

struct {

	char Status[100] ;
	char Status2[100];
} properties; 


// rs485.h (rtwang20150824)
//建立modbus連結的通道
int setupSerialPort(char devNumber, int baudRate) {
	
	int brate;
	struct termios term;
	char dev[11] = "/dev/ttyS0";
	dev[9] = devNumber;

	//printf("open %s with %d\n", dev, baudRate);
	syslog(LOG_INFO, "open %s with %d\n", dev, baudRate);
	//open device
	handle = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);

	if (handle <= 0) {
		//printf("Serial Init: Open failed\n");
		syslog(LOG_ERR, "Serial Init: Open failed\n");
		return -1;
	}

	switch (baudRate) {
	case 9600:
		brate = B9600;
		break;
	case 19200:
		brate = B19200;
		break;
	case 38400:
		brate = B38400;
		break;
	case 57600:
		brate = B57600;
		break;
	case 115200:
		brate = B115200;
		break;
	default:
		//printf("Serial Init: Set baudrate failed\n");
		syslog(LOG_ERR, "Serial Init: Set baudrate failed\n");
		return -1;
	}

	//get device struct
	if (tcgetattr(handle, &term) != 0) {
		//printf("Serial_Init:: tcgetattr() failed\n");
		syslog(LOG_ERR, "Serial_Init:: tcgetattr() failed\n");
		return -1;
	}

	//input modes
	term.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | INPCK | ISTRIP | INLCR | IGNCR | ICRNL
		| IXON | IXOFF);
	term.c_iflag |= IGNPAR;

	//output modes
	term.c_oflag &= ~(OPOST | ONLCR | OCRNL | ONOCR | ONLRET | OFILL
		| OFDEL | NLDLY | CRDLY | TABDLY | BSDLY | VTDLY | FFDLY);
	//control modes
	term.c_cflag &= ~(CSIZE | PARENB | PARODD | HUPCL | CRTSCTS);
	term.c_cflag |= CREAD | CS8 | CSTOPB | CLOCAL;

	//local modes
	term.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO);
	term.c_lflag |= NOFLSH;

	//set baud rate
	cfsetospeed(&term, brate);
	cfsetispeed(&term, brate);

	//set new device settings
	if (tcsetattr(handle, TCSANOW, &term) != 0) {
		//printf("Serial_Init:: tcsetattr() failed\n");
		syslog(LOG_ERR, "Serial_Init:: tcgetattr() failed.\n");
		return -1;
	}

	//printf("Serial_Init:: success\n");
	syslog(LOG_ERR, "Serial_Init:: success\n");

	return handle;
}




/*****************
Helper Functions
*****************/
void sendPropertyUpdate() {
	propertyList * proplist = twApi_CreatePropertyList("Status", twPrimitive_CreateFromString(properties.Status, TRUE), 0);
	if (!proplist) {
		TW_LOG(TW_ERROR, "sendPropertyUpdate: Error allocating property list");
		return;
	}
	twApi_AddPropertyToList(proplist, "Status2", twPrimitive_CreateFromString(properties.Status2, TRUE), 0);
	twApi_PushProperties(TW_THING, thingName, proplist, -1, FALSE);
	twApi_DeletePropertyList(proplist);
}

unsigned int calculateCRC(unsigned char *msg, int size) {
	unsigned int crc = 0xFFFF;
	int i = 0;
	int j = 0;
	for (i = 0; i < size; i++) {
		crc ^= msg[i];
		for (j = 0; j < 8; j++) {
			if ((crc % 2) == 0) {
				crc >>= 1;
			}
			else {
				crc >>= 1;
				crc ^= 0xA001;
			}
		}
	}

	return crc;
}

void dumpRequest(unsigned char *request) {
	printf("\nslaveNumber= 0x%X\n", request[0]);
	printf("functionCode= 0x%X\n", request[1]);
	printf("Addressnumber1= 0x%2X\n", request[2]);
	printf("Addressnumber2= 0x%2X\n", request[3]);
	printf("returnvalue1= 0x%X\n", request[4]);
	printf("returnvalue=2 0x%2X\n", request[5]);
	printf("Crc1= 0x%2X\n", request[6]);
	printf("Crc2= 0x%2X\n", request[7]);
	return;
}

void dataCollectionTask(DATETIME now, void * params, int handle) {   //©|¥Œ§¹Šš
																	 /* TW_LOG(TW_TRACE,"dataCollectionTask: Executing"); */
	int ret = 0,flag = 0;
	int value = 0, i = 0,j=0,k=0,m=0;
	double ret_value = 0;
	unsigned int crc;
	unsigned char request[8];
	unsigned char response[256];
	int number = 0 ;
	memset(properties.Status, 0, 100);
	memset(properties.Status2, 0, 100);
	char dev = '0';
	int baudRate[5] = { 9600, 19200, 38400, 57600, 115200 };

	FILE *record = fopen(FILE_NAME, "r");
	char buffer[200];
	int n = 0;
	//char con[256], ID[256], add[256],brate[256];
	int initial[256][4];
	for (i = 0; feof(record)==0; i++)
	{
		buffer[i] = fgetc(record);
	}
	buffer[i - 1] = '\0';
	printf("%s", buffer);
	char * pch1 = strtok(buffer, ",");

	for (i = 0; pch1 != NULL; i++)
	{
		printf("%s\n", pch1);
		initial[n][i] = atoi(pch1);
		if (i == 3) {
			n++;
			i = -1;
		}pch1 = strtok(NULL, ",");
	}
	fclose(record);


	 request[1] = 0x03; request[2] = 0x00; request[4] = 0x00; request[5] = 0x01;
	 for (i = 0; i < n; i++) {
		 printf("\nPORT   %c\n", initial[i][0] + 48);
		 dev = initial[i][0] + 48;
		 if ((handle = setupSerialPort(dev, initial[i][3])) < 0) {
			 printf("Error in open /dev/ttyS%c in %d\n", dev, initial[i][3]);
			 syslog(LOG_ERR, "Error in open /dev/ttyS%c in %d\n", dev, initial[i][3]);
			 closelog();
			 // exit(EXIT_SUCCESS);
		 }
		 else printf("open /dev/ttyS%c in %d\n", dev, initial[i][3]);
		 request[0] = initial[i][1]; request[1] = 0x03; request[2] = 0x00; request[4] = 0x00; request[5] = 1;
		 if (initial[i][1] == 5)
		 {
			 request[3] = ID5-1;
			 flag = 1;
		 }
		 else if (initial[i][1] == 6)
		 {
			 request[3] = ID6-1;
			 flag = 2;
		 }
		 crc = calculateCRC(request, 6);
		 request[6] = crc & 0x00FF;
		 request[7] = crc >> 8;
		 ret = write(handle, request, sizeof(request)); //¶Ç¥Xrequest
		 dumpRequest(request);
		 printf("ret = %d\n", ret);
		 usleep(100000);
		 memset(response, 0, 256);
		 ret = read(handle, response, 256);
		 printf("%u ret = %d\n", (unsigned)time(NULL), ret);
		 if (ret > 0) {
			 dumpResponse(response, ret);
			 number = response[4];
		 }
		 else number = 0;
		 printf("\n\nnumber = %d\n\n", number);
		 if (initial[i][2]<2 || initial[i][2] > (number*flag)+2-flag)
		 {
			 if (initial[i][0] == Com1)
			 {
				 properties.Status[j] = '0';
				 j++;
				 printf("\nCOM1 = 0\n");
			 }
			 else
			 {
				 properties.Status2[k] = '0';
				 k++;
				 printf("\nCOM2 = 0\n");
			 }
		 }
		 else {
			 request[0] = initial[i][1]; request[3] = initial[i][2];
			 crc = calculateCRC(request, 6);
			 request[6] = crc & 0x00FF;
			 request[7] = crc >> 8;
			 dumpRequest(request);
			 ret = write(handle, request, sizeof(request));   //¶Ç¥Xrequest
			 printf("ret = %d\n", ret);
			 usleep(100000);
			 memset(response, 0, 256);
			 ret = read(handle, response, 256);                //Åª€J¥~³¡¶Ç¶išÓªº­È
			 printf("%u ret = %d\n", now = (unsigned)time(NULL), ret);
			 if (ret > 0) {
				 for (m = 0; m < 7; m++)printf("%d,", response[m]);
				 value = (response[3] << 8) + response[4];
				 printf("\nanswer = %d\n", value);
				 if (initial[i][0] == Com1)
				 {
					 properties.Status[j] = '1';
					 j++;
					 printf("\nCOM1 = 1\n");
				 }
				 else
				 {
					 properties.Status2[k] = '1';
					 k++;
					 printf("\nCOM2 = 1\n");
				 }
			 }
			 else {
				 printf("NO detect");
				 if (initial[i][0] == Com1)
				 {
					 properties.Status[j] = '0';
					 j++;
					 printf("\nCOM1 = 0\n");
				 }
				 else
				 {
					 properties.Status2[k] = '0';
					 k++;
					 printf("\nCOM2 = 0\n");
				 }
			 }
		 }
	 }
	/* Update the properties on the server */

	sendPropertyUpdate();
}

/*****************
Service Callbacks
******************/
/* Example of handling a single service in a callback */
enum msgCodeEnum SwitchService(const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content, void * userdata) {
	char * input = "";
	int ret;
	unsigned char request[8];
	unsigned int crc;
	int addressnumber = 0x00;
	request[0] = 0x06; request[1] = 0x06; request[2] = 0x00; request[4] = 0x00;
	TW_LOG(TW_TRACE, "addNumbersService - Function called");
	if (!params || !content) {
		TW_LOG(TW_ERROR, "addNumbersService - NULL params or content pointer");
		return TWX_BAD_REQUEST;
	}
	twInfoTable_GetString(params, "Switch", 0, &input);
	printf("switch service : %s\n", input);
	//1 ¿Oon 2 ¿Ooff 3žÁ»ïŸ¹on 4žÁ»ïŸ¹off
	if (!strcmp(input, "1"))
	{
		addressnumber = 2;
		request[3] = addressnumber;
		request[5] = 1;

	}
	if (!strcmp(input, "2"))
	{
		addressnumber = 2;
		request[3] = addressnumber;
		request[5] = 0;

	}
	if (!strcmp(input, "3"))
	{
		addressnumber = 4;
		request[3] = addressnumber;
		request[5] = 1;
	}
	if (!strcmp(input, "4"))
	{
		addressnumber = 4;
		request[3] = addressnumber;
		request[5] = 0;
	}
	crc = calculateCRC(request, 6);
	request[6] = crc & 0x00FF;
	request[7] = crc >> 8;
	ret = write(4, request, sizeof(request)); //¶Ç¥Xrequest
	dumpRequest(request);
	printf("ret = %d", ret);
	/*twPrimitive * value = 0;
	value = twPrimitive_CreateFromString(input,TRUE);
	twApi_WriteProperty(TW_THING, thingName, "Switch", value, -1,FALSE);
	twPrimitive_Delete(value);*/


	*content = twInfoTable_CreateFromNumber("result", ret);
	if (*content) return TWX_SUCCESS;
	else return TWX_INTERNAL_SERVER_ERROR;
}

void shutdownTask(DATETIME now, void * params) {
	TW_LOG(TW_FORCE, "shutdownTask - Shutdown service called.  SYSTEM IS SHUTTING DOWN");
	twApi_UnbindThing(thingName);
	twSleepMsec(100);
	twApi_Delete();
	twLogger_Delete();
	exit(0);
}


/* Example of handling multiple services in a callback */
enum msgCodeEnum multiServiceHandler(const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content, void * userdata) {
	TW_LOG(TW_TRACE, "multiServiceHandler - Function called");
	if (!content) {
		TW_LOG(TW_ERROR, "multiServiceHandler - NULL content pointer");
		return TWX_BAD_REQUEST;
	}
	if (strcmp(entityName, thingName) == 0) {
		if (strcmp(serviceName, "GetBigString") == 0) {
			int len = 10000;
			char text[] = "inna gadda davida ";
			char * bigString = (char *)TW_CALLOC(len, 1);
			int textlen = strlen(text);
			while (bigString && len > textlen) {
				strncat(bigString, text, len - textlen - 1);
				len = len - textlen;
			}
			*content = twInfoTable_CreateFromString("result", bigString, FALSE);
			if (*content) return TWX_SUCCESS;
			return TWX_ENTITY_TOO_LARGE;
		}
		else if (strcmp(serviceName, "Shutdown") == 0) {
			/* Create a task to handle the shutdown so we can respond gracefully */
			twApi_CreateTask(1, shutdownTask);
		}
		return TWX_NOT_FOUND;
	}
	return TWX_NOT_FOUND;
}
dumpResponse(unsigned char* response, int n)
{
	int i = 0;
	for (i = 0; i < n; i++)
	{
		printf("%d,", response[i]);
	}
}

enum msgCodeEnum AddSensorService(const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content, void * userdata)
{
	char * input = "";
	int ret,value;
	double Com, ID,Addr,Brate;
	unsigned char request[8];
	unsigned char response[256];
	unsigned int crc;
	int addressnumber = 0x00;
	int flag = 0,number=0;
	char buf[200] = { 0 };
	twInfoTableRow * row = NULL;
	twDataShape * ds = NULL;
	twInfoTable * itT = NULL;
	TW_LOG(TW_TRACE, "addNumbersService - Function called");
	if (!params || !content) {
		TW_LOG(TW_ERROR, "addNumbersService - NULL params or content pointer");
		return TWX_BAD_REQUEST;
	}


	ds = twDataShape_Create(twDataShapeEntry_Create("resu",NULL,TW_NUMBER));
	twDataShape_AddEntry(ds,twDataShapeEntry_Create("value",NULL,TW_NUMBER));
	itT = twInfoTable_Create(ds);

	twInfoTable_GetNumber(params, "COM", 0, &Com);
	twInfoTable_GetNumber(params, "ID", 0, &ID);
	twInfoTable_GetNumber(params, "Address", 0, &Addr);
	twInfoTable_GetNumber(params, "BaudRate", 0, &Brate);
	printf("COM service : %g\n", Com);
	printf("ID service : %g\n", ID);
	printf("Address service : %g\n", Addr);
	printf("BaudRate service : %g\n", Brate);
	int handle = 0, j = 0;
	char dev = '0';
	
	int baudRate[5] = { 9600, 19200, 38400, 57600, 115200 };
	dev = Com + 48;
	if ((handle = setupSerialPort(dev, (int)Brate)) < 0) {
		printf("Error in open /dev/ttyS%c in %d\n", dev, (int)Brate);
		syslog(LOG_ERR, "Error in open /dev/ttyS%c in %d\n", dev, (int)Brate);
		closelog();
		//exit(EXIT_SUCCESS);
	}
	else printf("open /dev/ttyS%c in %d\n", dev, (int)Brate);
	if (ID == 5)flag = 0;
	else if (ID == 6)flag = 1;
	request[0] = ID; request[1] = 0x03; request[2] = 0x00; request[3] = flag; request[4] = 0x00; request[5] = 1;
	crc = calculateCRC(request, 6);
	request[6] = crc & 0x00FF;
	request[7] = crc >> 8;
	ret = write(handle, request, sizeof(request)); //¶Ç¥Xrequest
	dumpRequest(request);
	printf("ret = %d\n", ret);
	usleep(100000);
	memset(response, 0, 256);
	ret = read(handle, response, 256);
	printf("%u ret = %d\n", (unsigned)time(NULL), ret);
 	if (ret > 0) {
		dumpResponse(response, ret);
		number = response[4];
	}
	 else number = 0;
	printf("\n\nnumber = %d\n\n", number);
	if (Addr<2 || Addr > (number*(flag+1))+2-(flag+1))
	{
		printf("\nNO sensor1\n");
		row = twInfoTableRow_Create(twPrimitive_CreateFromNumber(0));
		twInfoTableRow_AddEntry(row,twPrimitive_CreateFromNumber(0));
		printf("\nOK\n");
	}
	else {
		request[0] = ID; request[1] = 0x03; request[2] = 0x00; request[3] = Addr; request[4] = 0x00; request[5] = 1;

		crc = calculateCRC(request, 6);
		request[6] = crc & 0x00FF;
		request[7] = crc >> 8;
		ret = write(handle, request, sizeof(request)); //¶Ç¥Xrequest
		dumpRequest(request);
		printf("ret = %d\n", ret);
		usleep(100000);
		memset(response, 0, 256);
		ret = read(handle, response, 256);
		printf("%u ret = %d\n", (unsigned)time(NULL), ret);
		if (ret > 0)
		{
			printf("OK");
		FILE *record = fopen(FILE_NAME, "a");
		sprintf(buf, ",%g,%g,%g,%g", Com, ID, Addr, Brate);
		printf("\n\n%c%c%c%c%c\n\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
		fputs(buf, record);
		fflush(record);
		fclose(record);
		value = (response[3] << 8) + response[4];
		printf("\nanswer = %d\n", value);
		row = twInfoTableRow_Create(twPrimitive_CreateFromNumber(1));
		twInfoTableRow_AddEntry(row,twPrimitive_CreateFromNumber(value));
		printf("\nOK\n");
		}
		else
		{
			printf("\nNO sensor2\n");
			row = twInfoTableRow_Create(twPrimitive_CreateFromNumber(0));
			twInfoTableRow_AddEntry(row,twPrimitive_CreateFromNumber(0));

			printf("\nOK\n");	
		}
		
	}
	
	twInfoTable_AddRow(itT,row);
	*content = itT;
	if (*content) return TWX_SUCCESS;
	else return TWX_INTERNAL_SERVER_ERROR;
}

enum msgCodeEnum RemoveSensorService(const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content, void * userdata)
{
	
	int initial[256][4],i;
	char buffer[200];
	int flag=0;
	TW_LOG(TW_TRACE, "addNumbersService - Function called");
	if (!params || !content) {
		TW_LOG(TW_ERROR, "addNumbersService - NULL params or content pointer");
		return TWX_BAD_REQUEST;
	}
	double Config,ID,Addr;
	twInfoTable_GetNumber(params, "COM", 0, &Config);
	twInfoTable_GetNumber(params, "ID", 0, &ID);
	twInfoTable_GetNumber(params, "ADDR", 0, &Addr);
	printf("\n\nCOM  %g\nID   %g\nADDR  %g\n",Config,ID,Addr);
	int  length, j,n = 0;
	FILE *record = fopen(FILE_NAME, "r");
	memset(buffer, 0, sizeof(buffer));
	for (i = 0; !feof(record); i++)
	{
		buffer[i] = fgetc(record);
	}
	buffer[i - 1] = '\0';
	printf("%s", buffer);
	char *pch1 = strtok(buffer, ",");

	for (i = 0; pch1 != NULL; i++)
	{
		printf("%s\n", pch1);
		initial[n][i] = atoi(pch1);
		if (i == 3) {
			n++;
			i = -1;
		}pch1 = strtok(NULL, ",");
	}
	fflush(record);
	fclose(record);
	record = fopen(FILE_NAME, "w");
	for (i = 0; ; i++)
	{
		if (initial[i][0]== (int)Config && initial[i][1]==(int)ID && initial[i][2]==(int)Addr)
		{
			i++;
			flag=1;
			*content = twInfoTable_CreateFromNumber("result", 1);
		}
		if (i >= n)break;
		for (j = 0; j < 4; j++)
		{
			fprintf(record, ",%d", initial[i][j]);
		}
	}
	if(flag==0)*content = twInfoTable_CreateFromNumber("result", 0);
	fflush(record);
	fclose(record);
	
	if (*content) return TWX_SUCCESS;
	else return TWX_INTERNAL_SERVER_ERROR;
}

/*******************************************/
/*         Bind Event Callback             */
/*******************************************/
void BindEventHandler(char * entityName, char isBound, void * userdata) {
	if (isBound) TW_LOG(TW_FORCE, "BindEventHandler: Entity %s was Bound", entityName);
	else TW_LOG(TW_FORCE, "BindEventHandler: Entity %s was Unbound", entityName);
}

/*******************************************/
/*    OnAuthenticated Event Callback       */
/*******************************************/
void AuthEventHandler(char * credType, char * credValue, void * userdata) {
	if (!credType || !credValue) return;
	TW_LOG(TW_FORCE, "AuthEventHandler: Authenticated using %s = %s.  Userdata = 0x%x", credType, credValue, userdata);
	/* Could do a delayed bind here */
	/* twApi_BindThing(thingName); */
}







/*****************
Property Handler Callbacks
******************/
enum msgCodeEnum propertyHandler(const char * entityName, const char * propertyName, twInfoTable ** value, char isWrite, void * userdata) {
	char * asterisk = "*";
	if (!propertyName) propertyName = asterisk;
	TW_LOG(TW_TRACE, "propertyHandler - Function called for Entity %s, Property %s", entityName, propertyName);
	if (value) {
		if (isWrite && *value) {
			/* Property Writes */
			//if (strcmp(propertyName, "InletValve") == 0) twInfoTable_GetBoolean(*value, propertyName, 0, &properties.InletValve); 
			//else if (strcmp(propertyName, "FaultStatus") == 0) twInfoTable_GetBoolean(*value, propertyName, 0, &properties.FaultStatus);

			//else return TWX_NOT_FOUND;
			return TWX_SUCCESS;
		}
		else {
			/* Property Reads */
			//if (strcmp(propertyName, "InletValve") == 0) *value = twInfoTable_CreateFromBoolean(propertyName, properties.InletValve); 
			//else return TWX_NOT_FOUND;
		}
		return TWX_SUCCESS;
	}
	else {
		TW_LOG(TW_ERROR, "propertyHandler - NULL pointer for value");
		return TWX_BAD_REQUEST;
	}
}

/***************
Main Loop
****************/
/*
Solely used to instantiate and configure the API.
*/
int main(int argc, char** argv) {

	int handle = 0, j = 0;
	char dev = '0';

	int baudRate[5] = { 9600, 19200, 38400, 57600, 115200 };
	if ((handle = setupSerialPort(dev, baudRate[j])) < 0) {
		printf("Error in open /dev/ttyS%c in %d\n", dev, baudRate[j]);
		syslog(LOG_ERR, "Error in open /dev/ttyS%c in %d\n", dev, baudRate[j]);
		closelog();
		exit(EXIT_SUCCESS);
	}

	FILE *record = fopen(FILE_NAME, "a");
	fflush(record);
	fclose(record);

#if defined NO_TLS
	int16_t port = 80;
#else
	int16_t port = TW_PORT;
#endif
	twDataShape * ds = 0;
	twDataShape * dsout = 0;
	int err = 0;

#ifndef ENABLE_TASKER
	DATETIME nextDataCollectionTime = 0;
#endif
	
	twLogger_SetLevel(TW_INFO);
	twLogger_SetIsVerbose(0);
	TW_LOG(TW_FORCE, "Starting up");

	/* Initialize the API */
	err = twApi_Initialize(TW_HOST, port, TW_URI, TW_APP_KEY, NULL, MESSAGE_CHUNK_SIZE, MESSAGE_CHUNK_SIZE, TRUE);
	if (err) {
		TW_LOG(TW_ERROR, "Error initializing the API");
		exit(err);
	}

	/* Set up for connecting through an HTTP proxy: Auth modes supported - Basic, Digest and None */
	/****
	twApi_SetProxyInfo("10.128.0.90", 3128, "proxyuser123", "thingworx");
	****/

	/* Allow self signed certs */
	twApi_SetSelfSignedOk();
	/* Enable FIPS mode (Not supported by AxTLS, yuo MUST use OpenSSL compiled for FIPS mode) */
	/****
	err = twApi_EnableFipsMode();
	if (err) {
	TW_LOG(TW_ERROR, "Error enabling FIPS mode.  Error code: %d", err);
	exit(err);
	}
	****/

	/* Register our services that have inputs */
	/* Create DataShape */
	ds = twDataShape_Create(twDataShapeEntry_Create("Switch", NULL, TW_STRING));
	if (!ds) {
		TW_LOG(TW_ERROR, "Error Creating datashape.");
		exit(1);
	}
	/* Register the service */
	twApi_RegisterService(TW_THING, thingName, "ControlSwitch", NULL, ds, TW_NUMBER, NULL, SwitchService, NULL);
	/* API now owns that datashape pointer, so we can reuse it */

	/* API now owns that datashape pointer, so we can reuse it */
	ds = NULL;

	/* Register our services that don't have inputs */
	twApi_RegisterService(TW_THING, thingName, "GetBigString", NULL, NULL, TW_STRING, NULL, multiServiceHandler, NULL);
	twApi_RegisterService(TW_THING, thingName, "Shutdown", NULL, NULL, TW_NOTHING, NULL, multiServiceHandler, NULL);
	

	 


	ds = twDataShape_Create(twDataShapeEntry_Create("ID", NULL, TW_NUMBER));
	if (!ds) {
		TW_LOG(TW_ERROR, "Error Creating datashape.");
		exit(1);
	}
	twDataShape_AddEntry(ds, twDataShapeEntry_Create("COM", NULL, TW_NUMBER));
	twDataShape_AddEntry(ds, twDataShapeEntry_Create("Address", NULL, TW_NUMBER));
	twDataShape_AddEntry(ds, twDataShapeEntry_Create("BaudRate", NULL, TW_NUMBER));
//
dsout = NULL;
    /* Create a DataShape for the SteamSensorReadings service output */
    dsout = twDataShape_Create( twDataShapeEntry_Create("resu", NULL, TW_NUMBER)); 
	if (!dsout) {
		TW_LOG(TW_ERROR, "Error Creating datashape.");
		exit(1);
	}
    twDataShape_AddEntry(dsout, twDataShapeEntry_Create("value", NULL, TW_NUMBER));
   
    /* Name the DataShape for the SteamSensorReadings service output */
    twDataShape_SetName(dsout, "user");
//
	twApi_RegisterService(TW_THING, thingName, "AddSensor", NULL, ds, TW_INFOTABLE, dsout, AddSensorService, NULL);
	
	ds = NULL;
	ds = twDataShape_Create(twDataShapeEntry_Create("COM", NULL, TW_NUMBER));

	if (!ds) {
		TW_LOG(TW_ERROR, "Error Creating datashape.");
		exit(1);
	}
	twDataShape_AddEntry(ds, twDataShapeEntry_Create("ID", NULL, TW_NUMBER));
	twDataShape_AddEntry(ds, twDataShapeEntry_Create("ADDR", NULL, TW_NUMBER));
	twApi_RegisterService(TW_THING, thingName, "RemoveSensor", NULL, ds, TW_NUMBER, NULL, RemoveSensorService, NULL);


	/* Regsiter our properties */
	twApi_RegisterProperty(TW_THING, thingName, "Status", TW_STRING, NULL, "ALWAYS", 0, propertyHandler, NULL);
	twApi_RegisterProperty(TW_THING, thingName, "Status2", TW_STRING, NULL, "ALWAYS", 0, propertyHandler, NULL);


	/* Add any additoinal aspects to our properties */
	twApi_AddAspectToProperty(thingName, "TemperatureLimit", "defaultValue", twPrimitive_CreateFromNumber(50.5));

	/* Register an authentication event handler */
	twApi_RegisterOnAuthenticatedCallback(AuthEventHandler, NULL); /* Callbacks only when we have connected & authenticated */

																   /* Register a bind event handler */
	twApi_RegisterBindEventCallback(thingName, BindEventHandler, NULL); /* Callbacks only when thingName is bound/unbound */
																		/* twApi_RegisterBindEventCallback(NULL, BindEventHandler, NULL); *//* First NULL says "tell me about all things that are bound */

																																			/* Bind our thing - Can bind before connecting or do it when the onAuthenticated callback is made.  Either is acceptable */
	twApi_BindThing(thingName);

	/* Connect to server */
	err = twApi_Connect(CONNECT_TIMEOUT, twcfg.connect_retries);
	if (!err) {
		/* Register our "Data collection Task" with the tasker */
		twApi_CreateTask(DATA_COLLECTION_RATE_MSEC, dataCollectionTask);
	}
	else {
		TW_LOG(TW_ERROR, "main: Server connection failed after %d attempts.  Error Code: %d", twcfg.connect_retries, err);
	}

	while (1) {
		char in = 0;

#ifndef ENABLE_TASKER
		DATETIME now = twGetSystemTime(TRUE);
		twApi_TaskerFunction(now, NULL);
		twMessageHandler_msgHandlerTask(now, NULL);
		/*
		if (twTimeGreaterThan(now, nextDataCollectionTime)) {
		dataCollectionTask(now, NULL,handle);
		nextDataCollectionTime = twAddMilliseconds(now, DATA_COLLECTION_RATE_MSEC);
		}*/
#else
		in = getch();
		if (in == 'q') break;
		else printf("\n");
#endif
		twSleepMsec(5);
	}
	twApi_UnbindThing(thingName);
	twSleepMsec(100);
	/*
	twApi_Delete also cleans up all singletons including
	twFileManager, twTunnelManager and twLogger.
	*/
	twApi_Delete();
	exit(0);
}
