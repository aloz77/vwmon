/*
 * vwmon-client.c - read Vaillant Heatpump data from ebusd and sent them via HTTP
 * Alexey Ozerov (alexey@ozerov.de)
 * 2013-04-30 First running version
 * 2013-05-01 Check if the returned value is numeric, use ebusd hostname instead of IP
 * 2013-05-02 Fix isnumeric for negative values
 * 2013-05-04 Add socket read timeout
 * 2013-05-13 New name vwmon
 * 2013-05-22 Fix pause calculation
 * 2013-07-01 Fix stability issue within logger function (use strcpy instead of strcat), use getaddrinfo
 * 2013-09-21 Start ebusd from vwmon (and restart it in case of it hangs)
 * 2013-11-10 Run symlinkusb.sh prior to ebusd, check that ebusd reports at least one valid value, reset error counter when a good value found
 * 2013-12-23 First tests with vwmon-control and getcommand
 * 2013-12-25 Fix command feedback URL encoding, max. 3 ebusd errors, get multiple commands in one session
 * 2016-01-30 Add fhem file output
 
 * TODOs: ???
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <elf.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <netdb.h>
#include "http_fetcher.h"

#define PROGRAM_VERSION "0.11"
#define MAX_EBUSD_ERRORS 3

void strcatenc(char *out,char *text,unsigned char urlencode);
void signal_handler(int signal);
int server_submit(char *server_url, char** filebuf);

char* URLencode(char *str);
char* URLdecode(char *str);
char* trim(char* input);
int isnumeric(char *str);
long diff_time(const struct timeval *tact, const struct timeval *tlast);


struct hp_datafield
{	char *command;
	char *table;
	char *field;
	int period;
	void *next;
} *dfl = NULL;

typedef enum log_event
{	LOG_DEBUG=1,
	LOG_WARNING=2,
	LOG_ERROR=4,
	LOG_INFO=8
} log_event;

FILE *_log_debug=NULL,*_log_warning=NULL,*_log_error=NULL,*_log_info=NULL;
void logger(log_event event,char *function,char *msg,...);

char *errorstring="NULL"; 		// What to write if value is not available or out of range
char *ebusd_host="localhost";
char *ebusd_port="7777";
char *ebusd_command=NULL;
char *ebusd_kill="kill `pidof ebusd`";
char *vwmon_server_url=NULL, *vwmon_server_key=NULL, *vwmon_server_url_submit=NULL, *vwmon_server_url_getcommand=NULL, *vwmon_server_url_error=NULL, *vwmon_server_url_setfeedback=NULL;
char *vwmon_server_senddata=NULL, *error_email=NULL, *vwmon_server_commands=NULL;
int run_interval=0;			// in seconds, 0 means run once and exit, change it by -r option or RunInterval cfg
int ebusd_err_counter=0;		// restart ebusd when ebusd_err_counter reaches MAX_EBUSD_ERRORS

char *FHEM_errorstring="";
char *FHEM_file=NULL;

FILE *fd;

char *filebuf=NULL; 			// Will be allocated by html_fetcher

char *vwmon_server_url_submit_template   	= "%s?serverkey=%s&action=addrecord";
char *vwmon_server_url_error_template    	= "%s?serverkey=%s&action=submiterror&email=%s";
char *vwmon_server_url_getcommand_template   	= "%s?serverkey=%s&action=getcommand";
char *vwmon_server_url_setfeedback_template   	= "%s?serverkey=%s&action=setfeedback";

//***************************************************************
// MAIN
//***************************************************************

int main(int argc, char **argv)
{
	int rv=0,c,i,l,help=0,value_found;
	char *cp;

	int sock;
	struct sockaddr_in server;
	struct hostent *he;
	struct in_addr* ipAddress;
	struct hp_datafield *dfp;
	char buf[100];

	long pause;
	time_t starttime,curtime;
	struct timeval tact, tlast, tdiff;
	char *output=NULL;

	_log_error=stderr;
	_log_info=stderr;

// Handle signals, don't break on SIGHUP!

	signal(SIGTERM, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGFPE, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGBUS, signal_handler);

// Parse options

	while (rv==0 && (c=getopt(argc,argv,"hH?vxf:d:a:A:p:e:t:s:c:u:r:t:k:"))!=-1)
	{
		switch (c)
		{
			case 'c': // read configuration from file
				i=read_cfg(optarg);
				logger(LOG_DEBUG,"main","reading cfg from file %s returned %d",optarg,i);
				break;

			case 'r': // set continuous run with time interval
				sscanf(optarg,"%d",&run_interval);
				logger(LOG_DEBUG,"main","continuos run interval set to %d seconds",run_interval);
				break;

			case 'v': // verbose messages
				_log_debug=_log_warning=stderr;
				logger(LOG_DEBUG,"main","Verbose messaging turned on");
				break;

			case 's': // Server URL
				logger(LOG_DEBUG,"main","Server URL set to '%s'",optarg);
				vwmon_server_url=optarg;
				break;

			case 'k': // Server Key
				logger(LOG_DEBUG,"main","Server Key set to '%s'",optarg);
				vwmon_server_key=optarg;
				break;

			case 'e': // Error string
				logger(LOG_DEBUG,"main","Error string set to: '%s'",optarg);
				errorstring=optarg;
				break;

			case 'H':
			{
				help=1;
				printf("Vaillant Monitor Client (vwmon-client) for FRITZ!Box\n");
				printf("Alexey Ozerov (c) 2014 - ver. %s\n\n", PROGRAM_VERSION);
				break;
			}

			case '?':
			case 'h':
				help=1;
				printf("Vaillant Monitor Client (vwmon-client) for FRITZ!Box\n");
				printf("Alexey Ozerov (c) 2014 - ver. %s\n\n", PROGRAM_VERSION);
				printf("Options\n");
				printf(" -? -h            Display this help\n");
				printf(" -c <filename>    Read configuration from cfg file\n");
				printf(" -v               Verbose output, enable debug and warning messages\n");
				printf(" -r <sec>         Run continuosly with given interval in seconds\n");
				printf(" -s <url>         Vaillant Monitor server URL\n");
				printf(" -k <key>         Vaillant Monitor server key\n");
				printf(" -e <errstr>      Write this errstr if measured value is not available\n");
		}
	}

	if (rv==0 && help==0)
	{

// Make a pause for the Fritzbox to set time and connect to internet

		time(&starttime);

		while (starttime<10000)
		{
			logger(LOG_WARNING,"main","Time is still unset: %d, wait 10 secs...", starttime);
			sleep(10);
			time(&starttime);
		}


// Check ebusd_command and start ebusd

		if (ebusd_command!=NULL)
		{	if (system(ebusd_command)<0)
			{
				logger(LOG_ERROR,"main","Can't start ebusd_Command %s",ebusd_command);
				exit(-1);
			}
			logger(LOG_DEBUG,"main","ebusd started");
			sleep(5);
		}
		else
		{	logger(LOG_ERROR,"main","Missing ebusd_Command in cfg file");
			exit (-1);
		}


// Prepare vwmon-server URLs

		if (vwmon_server_url!=NULL && vwmon_server_key!=NULL)
		{	
			l=strlen(vwmon_server_url)+strlen(vwmon_server_key);
			
			if (strcasecmp(vwmon_server_senddata,"On")==0)
			{	vwmon_server_url_submit = malloc(l+strlen(vwmon_server_url_submit_template));
				if (!vwmon_server_url_submit)
					logger(LOG_ERROR,"main","Could not allocate %u bytes for vwmon-server URL",l+strlen(vwmon_server_url_submit_template));
				else
					sprintf(vwmon_server_url_submit,vwmon_server_url_submit_template,vwmon_server_url,vwmon_server_key);
			}
			if (error_email!=NULL)
			{	vwmon_server_url_error = malloc(l+strlen(vwmon_server_url_error_template)+strlen(error_email));
				if (!vwmon_server_url_error)
					logger(LOG_ERROR,"main","Could not allocate %u bytes for vwmon-server URL",l+strlen(vwmon_server_url_error_template)+strlen(error_email));
				else
					sprintf(vwmon_server_url_error,vwmon_server_url_error_template,vwmon_server_url,vwmon_server_key,error_email);
			}
			
			if (strcasecmp(vwmon_server_commands,"On")==0)
			{	vwmon_server_url_getcommand = malloc(l+strlen(vwmon_server_url_getcommand_template));
				if (!vwmon_server_url_getcommand)
					logger(LOG_ERROR,"main","Could not allocate %u bytes for vwmon-server URL",l+strlen(vwmon_server_url_getcommand_template));
				else
					sprintf(vwmon_server_url_getcommand,vwmon_server_url_getcommand_template,vwmon_server_url,vwmon_server_key);

				vwmon_server_url_setfeedback = malloc(l+strlen(vwmon_server_url_setfeedback_template));
				if (!vwmon_server_url_setfeedback)
					logger(LOG_ERROR,"main","Could not allocate %u bytes for vwmon-server URL",l+strlen(vwmon_server_url_setfeedback_template));
				else
					sprintf(vwmon_server_url_setfeedback,vwmon_server_url_setfeedback_template,vwmon_server_url,vwmon_server_key);
			}
		}

// Start main loop for run_interval repetitions

		do
		{
			gettimeofday(&tlast, NULL);
			time(&curtime);

			rv=0;				// reset errors
			value_found=0;
			dfp=dfl;

// Allocate output buffer memory (used for data package URL and feedback URL)
// TODO: Better size calculation???

			output=malloc(strlen(vwmon_server_url)+1500);
			if (!output)
			{	logger(LOG_ERROR,"main","Could not allocate %u bytes for output",strlen(vwmon_server_url)+1500);
				rv=1;
			}

// Start filling data package

			if (rv==0 && vwmon_server_url_submit!=NULL) 
			{
				strcpy(output,"");
				strcatenc(output,vwmon_server_url_submit,0);
				strcatenc(output,"&datetime=",0);
				strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M:%S",gmtime(&curtime));
				strcatenc(output,buf,1);
			}


// Create socket & connect to ebusd

			if (rv==0) 
			{	
				struct addrinfo hints, *res;
				
				memset(&hints, 0, sizeof(hints));
			 	hints.ai_socktype = SOCK_STREAM;
			 	hints.ai_family = AF_INET;
				
				if ((rv = getaddrinfo(ebusd_host, ebusd_port, &hints, &res)) != 0)
				{ 	
					logger(LOG_ERROR,"main","Could not resolve host %s",ebusd_host);
					rv=1;
				}
				else
				{
					sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
					if(sock == -1)
					{ 
						rv=1;
						logger(LOG_ERROR,"main","Could not create socket");
					}
					else
					{
						struct timeval tv;
						tv.tv_sec = 10;  	// Secs Timeout
						tv.tv_usec = 0;		// Not init'ing this can cause strange errors
						if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval)) < 0) 
						{	rv=1;
							logger(LOG_ERROR,"main","Failed to set socket timeout option");
						}

						else if(connect(sock, res->ai_addr, res->ai_addrlen) < 0) 
						{ 
						    logger(LOG_ERROR,"main","Connection to ebusd socket failed");
						    ebusd_err_counter++;
						    rv=1;						
						}
					}
					freeaddrinfo(res);
				}
			}

// Create fhem file

  		if (rv==0 && FHEM_file!=NULL)
  		{ fd = fopen (FHEM_file, "w");
  			if (fd == NULL)
  				logger(LOG_ERROR,"main","Error opening FHEM_File %s for writing", FHEM_file);
  		}

// Send commands to ebusd

			if (rv==0)
			{
				logger(LOG_DEBUG,"main","Connected to ebusd socket");
				 
				while (rv==0 && dfp!=NULL)
				{
					logger(LOG_DEBUG,"main","ebusd command: %s",dfp->command);

					if (send(sock, dfp->command, strlen(dfp->command), 0) < 0)
					{
						logger(LOG_ERROR,"main","Socket send failed for command %s", dfp->command);
						ebusd_err_counter++;
						rv=1;
						break;
					}
				     
					if ((c=recv(sock, buf, sizeof(buf), 0)) <= 0)
					{
				    		logger(LOG_ERROR,"main","Socket receive failed or timeout for command %s", dfp->command);
				    		ebusd_err_counter++;
				        	rv=1;
				    		break;
					}
					else
					{
						buf[c]='\0';  // Terminate received string
				     		logger(LOG_DEBUG,"main","ebusd reply: %s",trim(buf));
				     	}

// If not numeric value received put "NULL" instead and ignore this error

					if (!isnumeric(buf))
					{	strcpy(buf,errorstring);
						logger(LOG_WARNING,"main","Non numeric value %s received on command %s",buf,dfp->command);
					}

// Format the value and add it to output string, reset error counter

					else if (output!=NULL)
					{
						strcatenc(output,"&",0);
						strcatenc(output,dfp->field,1);
						strcatenc(output,"=",0);
						strcatenc(output,trim(buf),1);
						value_found=1;
						ebusd_err_counter=0;

						if (FHEM_file!=NULL && fd != NULL)
						{ logger(LOG_DEBUG,"main","Writing to FHEM file %s", FHEM_file);
  					  fprintf(fd,"%s:%s\n",dfp->field,trim(buf));
						}


					}
					
					dfp=dfp->next;
				}
				 
// If no one valid value found drop an error (ebusd has strange states sometimes)

				if (!value_found)
				{	logger(LOG_ERROR,"main","No valid values found on eBusd");
					ebusd_err_counter++;
					rv=1;
				}
			}

// Close fhem file

  		if (FHEM_file!=NULL && fd != NULL)
  		{ fclose(fd);
  		}

// Send data to vwmon-server

			if (rv==0 && vwmon_server_url_submit!=NULL && value_found)
			{
				logger(LOG_DEBUG,"main","Submitting to server URL: %s",output);
				
				rv=server_submit(output,&filebuf);

				if (rv!=0 || strncasecmp(filebuf,"OK",2)!=0) 
				{	logger(LOG_ERROR,"main","Error submitting to vwmon-server, check vwmonServerURL");
				}
			}

// Get commands from server

			while (rv==0 && vwmon_server_url_getcommand!=NULL)
			{
				logger(LOG_DEBUG,"main","Getting commands from server URL: %s",vwmon_server_url_getcommand);
				
				rv=server_submit(vwmon_server_url_getcommand,&filebuf);

				if (rv!=0)
				{	logger(LOG_ERROR,"main","Error getting commands from vwmon-server, check vwmonServerURL");
					break;
				}
				else if (strncasecmp(filebuf,"Not found",9)==0)
				{
					break; // Stop processing commands if none found
				}
				else
				{	char *ptr = strtok(filebuf,":");
					if (ptr==NULL || !isnumeric(ptr))
					{	logger(LOG_ERROR,"main","Error getting command id from response %s",filebuf);
						break;
					}
					else
					{	int command_id=atoi(ptr);
						ptr = strtok(NULL, ":");
						logger(LOG_DEBUG,"main","Got command id %d: %s",command_id,ptr);
						
						if (send(sock, ptr, strlen(ptr), 0) < 0)
						{	logger(LOG_ERROR,"main","Socket send failed for command %s", ptr);
							ebusd_err_counter++;
							rv=1;
							break;
						}
					     
						else if ((c=recv(sock, buf, sizeof(buf), 0)) <= 0)
						{	logger(LOG_ERROR,"main","Socket receive failed or timeout for command %s", ptr);
					    		ebusd_err_counter++;
					        	rv=1;
					        	break;
						}
						else
						{
							buf[c]='\0';  // Terminate received string
					     		logger(LOG_DEBUG,"main","ebusd reply: %s",trim(buf));

                        				strcpy(output,"");
                        				strcatenc(output,vwmon_server_url_setfeedback,0);
                        				strcatenc(output,"&feedback=",0);
                        				strcatenc(output,buf,1);
                        				strcatenc(output,"&id=",0);
                        				sprintf(buf,"%d",command_id);
                        				strcatenc(output,buf,1);

							logger(LOG_DEBUG,"main","Sending feedback to server URL: %s", output);

							rv=server_submit(output,&filebuf);
							if (rv!=0)
							{	logger(LOG_ERROR,"main","Error sending feedback to vwmon-server, check vwmonServerURL");
							}

					     	}
						
					}
				}
			}

// Close ebusd connection
			
			if(sock != -1) close(sock);

// Free memory
			
			if (output!=NULL) free(output);

// Restart ebusd if too much errors happen

			if (ebusd_err_counter >= MAX_EBUSD_ERRORS)
			{	logger(LOG_ERROR,"main","Restarting ebusd after %d errors...",ebusd_err_counter);
				system (ebusd_kill);
				sleep(2);
				system (ebusd_command);
				sleep(5);
				ebusd_err_counter=0;
			}

// Make a pause

			if (run_interval>0)
			{	
				gettimeofday(&tact, NULL);
				
				pause = run_interval*1000000 - diff_time(&tact, &tlast);
				if (pause > 0 && pause <= run_interval*1000000)
				{
					logger(LOG_DEBUG,"main","Sleeping until next run %d microseconds\n", pause);
					usleep(pause);
				}
			}
			
		} while (run_interval>0);

		logger(LOG_DEBUG,"main","Killing ebusd...");
		system(ebusd_kill);

	}

	return rv;
}

//***************************************************************
// Read configuration from the cfg file
//***************************************************************

struct cfgvar
{	char *key;
	char *type;
	void *value;
} cfgvar[] =
{
	{"ErrorString","%s",&errorstring},
	{"VWMonServer_URL","%s",&vwmon_server_url},
	{"VWMonServer_Key","%s",&vwmon_server_key},
	{"VWMonServer_SendData","%s",&vwmon_server_senddata},
	{"VWMonServer_Commands","%s",&vwmon_server_commands},
	{"Error_Email","%s",&error_email},
	{"ebusd_Host","%s",&ebusd_host},
	{"ebusd_Port","%s",&ebusd_port},
	{"ebusd_Command","%s",&ebusd_command},
	{"RunInterval","%d",&run_interval},
	{"FHEM_File","%s",&FHEM_file},
	{"FHEM_ErrorString","%s",&FHEM_errorstring}
};

int read_cfg(char *fname)
{
	FILE	*fp;
	char	temp[1024], *key, *value, keyfound, advkey[100];
	int 	i;
	struct 	hp_datafield *dfp = dfl;


	if( ( fp = fopen( fname, "r" ) ) == NULL )
	{	logger(LOG_ERROR,"read_cfg","Could not open cfg file %s",fname);
		return 1;
	}

	while( fgets( temp, 1024, fp ) != 0 )
	{

// Skip empty and comment lines

		if( (temp[0] == '\n') || (temp[0] == '#') || (temp[0] == '\r' && temp[1] == '\n')) continue;

// Separate key and value from the input line

		int p = strcspn(temp, " \t\n\r");				// Find the first occurance of spaces and replace them with \0
		if (p>0) temp[p]='\0'; 
		else continue;

		key = temp;
		value = temp + p + 1;

		while (*value=='\t' || *value=='\n' || *value=='\r' || *value==' ') value++;	// Skip further spaces and remove the trailing \n or \r
		if (strlen(value)>0 && value[strlen(value)-1]=='\n') value[strlen(value)-1]='\0';
		if (strlen(value)>0 && value[strlen(value)-1]=='\r') value[strlen(value)-1]='\0';


// Look for different cfgvar keys and save the values

		keyfound=0;

		for(i=0;i<sizeof(cfgvar)/sizeof(cfgvar[0]);i++)
		{	if(strcasecmp(cfgvar[i].key, key) == 0)
			{	if (strcasecmp(cfgvar[i].type,"%s")==0)
				{	*(char **)(cfgvar[i].value)=malloc(strlen(value)+1);
					if (!*(char **)(cfgvar[i].value))
						logger(LOG_WARNING,"read_cfg","Could not allocate memory for cfg string %s",value);
					else
					{	strcpy(*(char **)(cfgvar[i].value), value);
						logger(LOG_DEBUG,"read_cfg","Key '%s' is set to '%s'",key,value);
					}

				}
				else
				{ 	sscanf(value,cfgvar[i].type,cfgvar[i].value);
					logger(LOG_DEBUG,"read_cfg","Key '%s' is set to '%s'",key,value);
				}
				keyfound++;
				break;
			}
		}

// If key is unknown handle it as field/command pair

		if (!keyfound)
		{	logger(LOG_DEBUG,"read_cfg","Field key '%s' set to '%s'",key,value);
			if (dfp==NULL)
			{	dfp=malloc(sizeof(struct hp_datafield));
				dfl=dfp;
			}
			else
			{	dfp->next=malloc(sizeof(struct hp_datafield));
				dfp=dfp->next;
			}
				
			if (dfp==NULL)
			{	logger(LOG_ERROR,"read_cfg","Could not allocate %d bytes for datafield node",sizeof(struct hp_datafield));
				return 1;
			}

			dfp->field=malloc(strlen(key)+1);
			strcpy(dfp->field,key);
			dfp->command=malloc(strlen(value)+1);
			strcpy(dfp->command,value);				
			dfp->period=1;
			dfp->next=NULL;
		}

	}
 
	fclose(fp); 
	return 0;
} 


//***************************************************************
// Parse, format and submit values
//***************************************************************

/*
static size_t write_callback(char *buffer, size_t size,size_t nitems,void *output)
{
//	logger(LOG_DEBUG,"write_callback","Received data size %d nitems %d content: %s", size, nitems,buffer);
	strncpy(output,buffer,255);
	return size*nitems;
}
*/

int server_submit(char *server_url, char** filebuf)
{
	if (*filebuf) 
	{	free(*filebuf);
		*filebuf=NULL;
	}

	http_setTimeout(15);
	int l=http_fetch(server_url, filebuf);
	
	if (l>=0)
	{	logger(LOG_DEBUG,"server_submit","http_fetcher performed OK content: %s", *filebuf);
		return 0;
	}
	else
	{	logger(LOG_WARNING,"server_submit","http_fetcher failed with message \"%s\"", http_strerror());
		return 1;
	}
}


// Format wrecord w according to format string into out

void strcatenc(char *out,char *text,unsigned char urlencode)
{
	char *encoded=NULL;
	if (urlencode)
	{	encoded=URLencode(text);
		if (!encoded)
		{	logger(LOG_ERROR,"main","URLencode could not allocate memory for '%s'",text);
		}
		else
		{	strcat(out,encoded);
			free(encoded);
		}
	}
	else
	{	strcat(out,text);
	}
	return;
}

//***************************************************************
// Signal handling
//***************************************************************

void signal_handler(int signal)
{
	if (signal == SIGTERM)
		logger(LOG_DEBUG,"signal_handler","SIGTERM received, exiting...");
	else if (signal == SIGQUIT)
		logger(LOG_ERROR,"signal_handler","SIGQUIT received, exiting...");
	else if (signal == SIGINT)
		logger(LOG_ERROR,"signal_handler","SIGINT received, exiting...");
	else if (signal == SIGFPE)
		logger(LOG_ERROR,"signal_handler","SIGFPE received, exiting...");
	else if (signal == SIGILL)
		logger(LOG_ERROR,"signal_handler","SIGILL received, exiting...");
	else if (signal == SIGSEGV)
		logger(LOG_ERROR,"signal_handler","SIGSEGV received, exiting...");
	else if (signal == SIGBUS)
		logger(LOG_ERROR,"signal_handler","SIGBUS received, exiting...");
	else		
		logger(LOG_ERROR,"signal_handler","Unknown signal received, exiting...");

// TODO: Need to close open devices here?

	logger(LOG_DEBUG,"signal_handler","Killing ebusd...");
	system(ebusd_kill);

	exit(1);
}

//***************************************************************
// Common functions
//***************************************************************

void logger(log_event event,char *function,char *msg,...)
{
	va_list args;

	va_start(args,msg);
	switch (event)
	{
		case LOG_DEBUG:
			if (_log_debug)
			{
				fprintf(_log_debug,"message: vwmon-client.%s - ",function);
				vfprintf(_log_debug,msg,args);
				fprintf(_log_debug,"\n");
			}
			break;

		case LOG_WARNING:
			if (_log_warning)
			{
				fprintf(_log_warning,"warning: vwmon-client.%s - ",function);
				vfprintf(_log_warning,msg,args);
				fprintf(_log_warning,"\n");
			}
			break;

		case LOG_ERROR:
			if (_log_error)
			{
				fprintf(_log_error,"error: vwmon-client.%s - ",function);
				vfprintf(_log_error,msg,args);
				fprintf(_log_error,"\n");

// Send error message to vwmon-server

				if (vwmon_server_url_error!=NULL)
				{
					char *last_error_text = malloc(strlen(msg)+500);
					if (last_error_text) 
					{	vsprintf(last_error_text,msg,args); 
						char *server_url_error_full = malloc(strlen(vwmon_server_url_error)+strlen(last_error_text)+100);
						if (server_url_error_full)
						{	strcpy(server_url_error_full,vwmon_server_url_error);
							strcat(server_url_error_full,"&errortext=");
							strcatenc(server_url_error_full,last_error_text,1);
							logger(LOG_DEBUG,"main","Submit error message to server URL: %s", server_url_error_full);
							server_submit(server_url_error_full,&filebuf);
							free(server_url_error_full);
						}
						free(last_error_text);
					}
				}
			}
			break;

		case LOG_INFO:
			if (_log_info)
			{
				fprintf(_log_info,"info: vwmon-client.%s - ",function);
				vfprintf(_log_info,msg,args);
				fprintf(_log_info,"\n");
			}
			break;
	}
	va_end(args);
}

// Converts a hex character to its integer value

char from_hex(char ch) {
  return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

// Converts an integer value to its hex character

char to_hex(char code) 
{
	static char hex[] = "0123456789abcdef";
	return hex[code & 15];
}

// Returns a url-encoded version of str
// IMPORTANT: be sure to free() the returned string after use

char* URLencode(char *str) 
{
	char *pstr = str, *buf = malloc(strlen(str) * 3 + 1), *pbuf = buf;
	if (buf!=NULL)
	{
        	while (*pstr)
          	{
            		if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~') 
              			*pbuf++ = *pstr;
            		else if (*pstr == ' ') 
              			*pbuf++ = '+';
            		else 
              			*pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
            		pstr++;
          	}
          	*pbuf = '\0';
	}
  	return buf;
}

// Returns a url-decoded version of str 
// IMPORTANT: be sure to free() the returned string after use

char* URLdecode(char *str)
{
	char *pstr = str, *buf = malloc(strlen(str) + 1), *pbuf = buf;
	if (buf!=NULL)
	{
		while (*pstr) 
		{
			if (*pstr == '%') 
			{
      				if (pstr[1] && pstr[2]) 
      				{
        				*pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
        				pstr += 2;
      				}
    			} 
    			else if (*pstr == '+') 
    			{ 
      				*pbuf++ = ' ';
    			} 
    			else 
    			{
      				*pbuf++ = *pstr;
    			}
    			pstr++;
  		}
  		*pbuf = '\0';
  	}
	return buf;
}


char* trim(char* str) 
{
   char * start = str;
   char * end = start + strlen(str);

   while (--end >= start) {   /* trim right */
      if (!isspace(*end))
         break;
   }
   *(++end) = '\0';

   while (isspace(*start))    /* trim left */
      start++;

   if (start != str)          /* there is a string */
      memmove(str, start, end - start + 1);

   return str;
}

int isnumeric(char *str)
{
  while(*str)
  {
    if(!isdigit(*str) && *str!='.' && *str!='-')
      return 0;
    str++;
  }
  return 1;
}

long diff_time(const struct timeval *tact, const struct timeval *tlast)
{
    long diff;

    diff = (tact->tv_usec + 1000000 * tact->tv_sec) -
	   (tlast->tv_usec + 1000000 * tlast->tv_sec);
	   
    return diff;
}
