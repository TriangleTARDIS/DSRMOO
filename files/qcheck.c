/*
 * qstat 1.4 beta
 * by Steve Jankowski
 * steve@activesw.com
 * http://www.activesw.com/people/steve/qstat.html
 *
 * Inspired by QuakePing by Len Norton
 *
 * Copyright 1996 by Steve Jankowski
 *
 * Permission granted to use for any purpose you desire as long as
 * you maintain this file prolog in the source code and you
 * derive no monetary benefit from use of this source or resulting
 * program.
 */

/*
  QCheck v1.5 alpha by Michael Thompson (Shadow_RAM)
  
  Hmm... wonder what it's inspired by.
  
  Copyright (C) 1996 by Michael Thompson
 
  Same disclaimer as above.
*/ 

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <netinet/in.h> 
#include <time.h> 
/* #include <ncurses/curses.h> */
#include <netdb.h> 

#include "windef.h"

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

/*
#define MAKEWORD ( a, b )  ( ( unsigned word ) ( ( ( unsigned char ) ( a ) ) | ( ( unsigned word ) ( ( unsigned char ) ( b ) ) ) << 8 ) )
#define MAKELONG ( a, b )  ( ( unsigned long )( ( (  unsigned word ) ( a ) ) | ( ( unsigned word ) ( ( unsinged word ) ( b ) ) ) 
*/
#define getch() getchar()

#define DEFAULT_PORT		26000
#define DEFAULT_RETRIES		5
#define DEFAULT_RETRY_INTERVAL	600		/* milli-seconds */
#define DEFAULT_DELAY		5 			/* seconds */
#define MAXIP 2
#define hostname_lookup 1
#define PACKET_LEN 1600


char *DOWN = "DOWN";
char *SYSERROR = "ERROR";
char *TIMEOUT = "TIMEOUT";
char *ERRMSG = NULL;
char *NOERR ="NOERR";

int retry_interval = DEFAULT_RETRY_INTERVAL;
int n_retries = DEFAULT_RETRIES;

struct server
{
    char *arg;
    char *host_name;
    struct in_addr ip[MAXIP];
	int port;
	int fd;	    
    int retry;
};

struct qserver
{	
    char *q_server_address;
    char *q_host_name;
    char *q_level_name;
    unsigned char q_current_players;
    unsigned char q_max_players; 
	unsigned char q_net_protocol_version;
};

struct qplayer
{	      
     char     player_number;           
     char     *name;                   
     unsigned char colors;             
     char     unused1;                
     char     unused2;
     char     unused3;                 
     long     frags;                   
     unsigned long connect_time;       
     char     *client_address;         
};

struct qrule
{
	char *name;	 
			 
	char *value; 
};

typedef struct server SERVER;
typedef SERVER *SPTR;
typedef struct qserver QSERVER;
typedef QSERVER *QSPTR;
typedef struct qplayer QPLAYER;
typedef QPLAYER *QPPTR;
typedef struct qrule QRULE;
typedef QRULE *QRPTR;


unsigned char server_info_request[] = { 0x80, 0x00, 0x00, 0x0C, 0x02, 0x51,
                                 0x55, 0x41, 0x4B, 0x45, 0x00, 0x03 };
			

unsigned char player_info_request[] = { 0x80,0x00,0x00,0x06,0x03,0x00 };
			
unsigned char rule_info_request[] = { 0x80,0x00,0x00,0x06,0x04,0x00 };
				

void my_pause(void);
int my_delay(int s);
unsigned long endian_swap_long(unsigned long src);
unsigned short endian_swap_short(unsigned short src);
char * english_time(unsigned long t);
char * get_last_err(void);
char * strherror(int h_err);
unsigned char check_for_inp(void);
void do_help(void);
void usage(void);

char * net_protocol_version_name(unsigned char v);
char * color_name(char c);
char * fix_pname(char *name);
SPTR make_server(char *arg);
QSPTR make_qserver(void);
QPPTR make_qplayer(unsigned char pn);
QRPTR make_qrule(void);

int bind_server(SPTR server);
int bind_socket(SPTR server);
void set_fds( fd_set *fds, SPTR server);
int connection_refused();
int flush_server(SPTR server);
char * make_rule_packet(char *rn);
int send_packet(SPTR server, const char *pkt,int pktlen);
int recv_server_info_packet(SPTR server,QSPTR qserver);
int recv_player_info_packet(SPTR server,QPPTR player);
int recv_rule_info_packet(SPTR server,QRPTR rule);
void display_server(SPTR server, QSPTR qserver);
void do_players(SPTR server,QSPTR qserver);
void do_rules(SPTR server);
void display_player(QPPTR player);
void cleanup(SPTR server);

/* temp kludges */

int kbhit(void)
{
  return 1;
}

void clear(void)
{
  char i;
  
  for (i = 0;i < 25;i++)
    printf("\n");
}



int main(int argc, char *argv[])
{		
	unsigned char *servername,done = 0,qu = 0,*quf;
	int ds = DEFAULT_DELAY,arg;
    SPTR server = NULL;			
    
	if (!(argc > 1)) 
	{
		usage();
	    return 1;
    }
	
	servername = strdup(argv[1]);

	for (arg = 2;arg < argc;arg++)  
	{
	    if (strcmp( argv[arg], "-r") == 0)  
	    {
     	    arg++;

	        if ( arg >= argc)  
	        {
	   	        printf("missing argument for -r\n\n");
		        usage();				
	        }
	        
	        n_retries = atoi(argv[arg]);

	        if ( n_retries <= 0)  
	        {
		        printf("retries must be greater than 0\n");
		        exit(1);
	        }
	    }
	    else if ( strcmp( argv[arg], "-i") == 0)  
	    {
	        double value= 0.0;

	        arg++;

	        if ( arg >= argc)  
	        {
		        printf("missing argument for -i\n\n");
		        usage();
	        }

	        sscanf( argv[arg], "%lf", &value);			

	        if ( value < 0.1)  
	        {
		        printf("retry interval must be greater than 0.1\n");
		        exit(1);
	        }

	        retry_interval= (int)(value * 1000);
	    }
		else if ( strcmp( argv[arg], "-d") == 0)  
	    { 
	        arg++;

	        if ( arg >= argc)  
	        {
		        printf("missing argument for -d\n\n");
		        usage();
	        }
	        
	        ds = atoi(argv[arg]);

	        if ( ds < 1)  
	        {
		        printf("delay must be greater than 0\n");
		        exit(1);
	        }	        
	    }
		else if ( strcmp( argv[arg], "-q") == 0)  
	    { 
	        qu = 1; 
	    }
		else if ( strcmp( argv[arg], "-qf") == 0)  
	    { 
	        qu = 2; 
	        arg++;

	        if ( arg >= argc)  
	        {
		        printf("missing argument for -qf\n\n");
		        usage();
	        }
	        
	        quf = strdup(argv[arg]); 
	    }    
    }


	
	server = make_server(servername);
	if (server == NULL) 
	{
	   printf("Unable to find host : %s",servername);
	   return 1;
	}


    if (bind_socket(server) != 0) 
	{
	   printf("Unabled to bind/connect socket.\n");
	   return 1;
	}  
    
if (!qu)
    clear();

	while (!done) 
	{	
	   unsigned char cp = 1,sab = 0;
	   
	   int ping1,ping2,ping3;	   
	   
	   QSPTR qserver = NULL;
	   
	   
if (!qu)
	   clear();
	
   if (!qu)
	   {
	      printf("Flushing buffers...");
	      flush_server(server);
	   }

	   qserver = make_qserver();
       if (qserver == NULL) 
	   {
     	 printf("Out of memory.");
	     return 1;
	   }

	if (!qu)
	   clear();

       if (send_packet(server,server_info_request,sizeof(server_info_request)) != 0)
	   {	     
  	     printf("Failed on initial server info send : %s\n",get_last_err());	   
		 cp = 0;  	     
	   }  
	
	   if (recv_server_info_packet(server,qserver) != 0)
	   {
     	 printf("Failed on initial server info recv : %s\n",get_last_err());	   
		 cp = 0;
	   }

	   
	   if (cp)
	   {
		  display_server(server,qserver);	
		  
		  ping1 = ping2 = ping3 = 0;
		  printf("Ping averages (NOT IMPLEMENTED):   %d,  %d,  %d\n",ping1,ping2,ping3);	      
		  if (!qu) 
		     printf("Delay : %us\n",ds);
		  
	   }

	   if (!qu)
	   {
	      switch (toupper(check_for_inp()))
	      {
   	         case 'Q' : done = 1;
		                sab = 1;
		                break;
		     case 'H' : do_help();
		                sab = 1;
		                break;
		     case '?' : do_help();
		                sab = 1;
		                break;
		     case 'D' : clear();
		                printf("Old Delay : %u\n\nNew Delay? ",ds);		             
		                getch();					 
		                scanf("%d",&ds);
				        ds = (ds < 1) ? ds = 1 : ds;		             
					    sab = 1;
				        break;
		     case 'R' : do_rules(server);
					    clear();					 
					    sab = 1; 					 
					    break;
	      }
	   }

	   if (cp && !sab)
	   {
		  /*
		  printf("\nPID PLAYERNAME      FRAGS SHIRT    PANTS            TIME               ADDRESS\n\n");

	      ts = malloc(sizeof(player_info_request));
	      if (ts == NULL)
	      {
	        printf("Out of memory.\n");
	        return 1;
	      }
	      memcpy(ts,&player_info_request,sizeof(player_info_request)); 
   	
           for (pn = 0; pn < qserver->q_current_players; pn++)
   	       {
   	   
     	     player = make_qplayer(pn);
	         *(ts+5) = player->player_number;   
	   
	         if (send_packet(server,ts,sizeof(player_info_request)) != 0)
	         {
        	     printf("#%2u %-15s\n",pn,get_last_err()); 	  
    		     free(player);
	             player = NULL;
	             continue;
	         }
	
	         if (recv_player_info_packet(server,player) != 0)
	         {	  
        	     if (!((es = get_last_err()) == NOERR))
	    	     {
	                printf("#%2u %-15s\n",pn,es);
		         }
		         free(player);
	             player = NULL;
	             continue;
	          }

    	      printf("#%2u %-15s %5d",pn,player->name,player->frags);       	         	
	          printf(" %-8s",color_name((char)((player->colors >> 4) & 0xF)));
			  printf(" %-8s",color_name((char)(player->colors & 0xF)));
			  printf(" %12s",english_time(player->connect_time));
	          printf("  %20s\n",player->client_address);

	   	   
              free(player);
	          player = NULL;
	      }	*/
		  do_players(server,qserver);
	   }

	   free(qserver);
	   qserver = NULL;
	   	    
	   /*
	   if (!qu)
	   {
	      switch (toupper(check_for_inp()))
	      {
   	         case 'Q' : done = 1;		             
		  			    sab = 1;
		                break;
		     case 'H' : do_help();		             
		                sab = 1;
		                break;
		     case '?' : do_help();		             
		                sab = 1;
		                break;
		     case 'D' : clear();
		                printf("Old Delay : %u\n\nNew Delay? ",ds);
		                getch();					 
		                scanf("%d",&ds);
				        ds = (ds < 1) ? ds = 1 : ds;		             
					    sab = 1;
				        break;
	         case 'R' : do_rules(server);
					    clear();
					    sab = 1;
					    break;
	      }	   	   
	   }

	   if (!sab && !qu)
	      _sleep(1000*ds);

	   */
	   if (!qu && !sab)
	   { 
	     if (my_delay(ds))
		 {
		     switch (toupper(check_for_inp()))
	         { 
   	            case 'Q' : done = 1;		             
		  			       sab = 1;
		                   break;
		        case 'H' : do_help();		             
		                   sab = 1;
		                   break;
		        case '?' : do_help();		             
		                   sab = 1;
		                   break;
		        case 'D' : clear();
		                   printf("Old Delay : %u\n\nNew Delay? ",ds);
		                   getch();					 
		                   scanf("%d",&ds);
				           ds = (ds < 1) ? ds = 1 : ds;		             
					       sab = 1;
				           break;
	            case 'R' : do_rules(server);
					       clear();
					       sab = 1;
					       break;
	        }		   
		 }
	   }
	   
	   if (qu) 
	      done = 1;

	} 
	
	cleanup(server);
	 
    return 0;
}





/* misc util stuff*/
void my_pause(void)
{
   printf("Press any key to continue.");
   while (!kbhit());
   getch();  
   printf("\n");
}

int my_delay(int s) 
{
   unsigned char d;
   
   for (d = 0;d < s*5;d++)
   {
      if (kbhit()) return 1;
	  sleep(200);
   }
   return 0;
}

unsigned long endian_swap_long(unsigned long src)
/* Thanks to Dr.J@BayMOO
 Intel : Little-Endian ??
*/
{

    return  (
            (src >> 24) +
            ((src >> 8)  & 0x0000ff00) +
            ((src << 8)  & 0x00ff0000) +
            (src << 24)
            );
}

unsigned short endian_swap_short(unsigned short src)
{

    return  (            
            ((src >> 8)  & 0x00ff) +
            ((src << 8)  & 0xff00)            
            );
}

char * get_last_err(void)
{
   char *p;

   p = (ERRMSG == NULL) ? NOERR : ERRMSG;
   ERRMSG = NOERR;

   return p;
}


char * english_time(unsigned long t)
{
	unsigned long tt;
	unsigned int d,h,m,s;
	char *ts,b[80];	


	ts = (char *)malloc(80);
	if (ts == NULL)
	{
	   printf("Out of memory.");
	   exit(1);
	}


	tt = t;
	d = tt/86400;    
	tt = tt % 86400;
	h = tt/3600;	  	
	tt = tt % 3600;
	m = tt/60;	  	
	s = tt % 60;

	
	if (t >= 86400) 
	{
	   sprintf(b,"%ud %02u:%02u:%02u",d,h,m,s);
	}
	else if (t >= 3600)
	{	   
	   sprintf(b,"%02u:%02u:%02u",h,m,s);
	}
	else if (t >= 60)
	{	   
	   sprintf(b,"%02u:%02u",m,s);
	}
	else
	{
	   sprintf(b,"0:00");   	   
	}

	memcpy(ts,&b,80);
	return ts;
}

char * strherror(int h_err)
{
    static char msg[100];
    switch (h_err)  
    {
       case HOST_NOT_FOUND:	return "host not found";
       case TRY_AGAIN:		return "try again";
       case NO_RECOVERY:	return "no recovery";
       case NO_ADDRESS:		return "no address";	   
       default:	sprintf( msg, "%d", h_err); return msg;
    }
}

unsigned char check_for_inp(void)
{
   unsigned char k;

   if (kbhit())
      k = getch();
   else
      k = 0;

   return k;
}

void do_help(void)
{
   clear();
   printf("QCheck Commands :\n\n");
   printf("h or ? : this screen\n");
   printf("d      : change delay between updates\n");
   printf("r      : get a rules listing from the current server\n");
   printf("q      : exit program\n\n\n");

   my_pause();
}

void usage(void)
{
	printf("Usage : qcheck host [-r retries] [-i interval] [-d delay] [-q] [-qf file]\n\n");
	printf("-r\tnumber of retries, default is %d\n", DEFAULT_RETRIES);
    printf("-i\tinterval between retries, default is %.2lf seconds\n",DEFAULT_RETRY_INTERVAL / 1000.0);	
	printf("-d\tamount of delay between updates, default is %d seconds\n", DEFAULT_DELAY);
	printf("-q\tdo one quick update, ignore input, and exit\n");
	printf("-qf\tdo one quick update, ignore input, dump to file and exit\n");
	printf("\t(for www pages, etc.)\n");
	exit(0);
}



/* misc quake specific util*/

char * net_protocol_version_name(unsigned char v)
{
    switch (v)  
    {
       case 1  :	return "Qtest 1";
       case 2  :	return "Unknown (Ver 2)";
       case 3  :	return "Quake v0.91 to 1.06";
       default :	return "Unknown ???";
    }   
}

char * color_name(char c)
{
	switch (c)
	{
	   case 0  : return "White";
	   case 1  : return "Olive";
	   case 2  : return "Lt. Blue";
	   case 3  : return "Green";
	   case 4  : return "Red";
	   case 5  : return "Mustard";
	   case 6  : return "Pink";
	   case 7  : return "Flesh";
	   case 8  : return "Purple";
	   case 9  : return "Purple";
	   case 10 : return "Gray";
	   case 11 : return "Green";
	   case 12 : return "Yellow";
	   case 13 : return "Blue";
	}
}

char * fix_pname(char *name) 
{
   char *tn,*n2;
   unsigned char c,c2,tc;

   tn = strdup(name);   
   n2 = strdup(name);
   for (c = 0, c2 = 0; c <= strlen(tn)-1; c++, c2++)
   {
      tc = *(tn+c);
	  if (tc >= 32 && tc <= 126) 
	    *(n2+c2) = tc;
	  else if (tc >= 160 && tc <= 254)
	    *(n2+c2) = tc - 128;
	  else if (tc >= 146 && tc <= 155)
	    *(n2+c2) = tc - 98;
	  else if (tc >= 18 && tc <= 27)
	    *(n2+c2) = tc + 30;	  
	  else if ((tc >= 6 && tc <= 9) || (tc >= 1 && tc <= 4) || (tc >= 134 && tc <= 137))
	    *(n2+c2) = 35;	  
	  else if (tc == 5 || tc == 14 || tc == 15 || tc == 28 || tc == 133 || tc == 142 || tc == 143)
	  	*(n2+c2) = '.';	    
	  else if (tc == 128 || tc == 144 || tc == 157)
		*(n2+c2) = '[';	    
	  else if (tc == 129 || tc == 158)
		*(n2+c2) = '=';	    
	  else if (tc == 130 || tc == 145 || tc == 159)
		*(n2+c2) = ']';	    
	  else        
		c2--;
   }
   *(n2+c2) = 0;
   return n2;
}

SPTR make_server(char *arg)
{
    struct hostent *ent= NULL;
    struct hostent *name_ent= NULL;
    SPTR server;
    int i, port= DEFAULT_PORT;
    char **a;
    unsigned long ipaddr;
	
 
    server = (SPTR)malloc(sizeof(SERVER));
	if (server == NULL) {
	  printf("Out of memory.\n");
	  return NULL;
	}

	ipaddr = inet_addr(arg);
	
	if (ipaddr == INADDR_NONE)
	   ent = gethostbyname(arg);	
	   
	if (ent == NULL && ipaddr != INADDR_NONE) 
	   ent = gethostbyaddr((char *)&ipaddr,4,PF_INET); 

	if (ent == NULL)	
      return NULL;

	name_ent = (ent != NULL) ? ent : NULL;

	
    server->arg= strdup(arg);
    server->host_name= strdup((name_ent != NULL) ? name_ent->h_name : ent->h_name);
    for ( a= ent->h_addr_list, i= 0; *a != NULL && i < MAXIP; a++, i++)
    {
	   memcpy( &server->ip[i].s_addr, *a, sizeof(server->ip[i].s_addr));    
    }
    server->port = port;    
    server->fd = -1;
    server->retry = n_retries;

    return server;     
}

QSPTR make_qserver(void)
{
    QSPTR qserver;

 
    qserver = (QSPTR)malloc(sizeof(QSERVER));
	if (qserver == NULL) {
	  printf("Out of memory.\n");
	  return NULL;
	}
    qserver->q_host_name = NULL;
    qserver->q_level_name = NULL;
    qserver->q_current_players = 0;
	qserver->q_net_protocol_version = 0;

    return qserver;    
}

QPPTR make_qplayer(unsigned char pn)
{
    QPPTR player;

	player = (QPPTR)malloc(sizeof(QPLAYER));    
	if (player == NULL) {
	  printf("Out of memory.\n");
	  return NULL;
	}    
  
	player->player_number = pn;
	player->name = NULL;
	player->colors = player->unused1 = player->unused2 = player->unused3 = 0;
	player->frags = player->connect_time = 0;
	player->client_address = NULL;
	
    return player;     
}

QRPTR make_qrule(void)
{
    QRPTR qrule;

	qrule = (QRPTR)malloc(sizeof(QRULE));    
	if (qrule == NULL) {
	  printf("Out of memory.\n");
	  return NULL;
	}    

	qrule->name = "";
	qrule->value = "";  
	
    return qrule;    
}


/* network stuff*/
int bind_server(SPTR server)
{
    struct sockaddr_in addr;

    if (( server->fd = socket( PF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) 
    {
        perror("socket");			    
		ERRMSG = SYSERROR;
        return 1;
    }

    addr.sin_family = PF_INET;
    addr.sin_port = htons(0);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    memset( &(addr.sin_zero), 0, sizeof(addr.sin_zero) );

    if (bind(server->fd, (struct sockaddr *)&addr,sizeof(struct sockaddr)) == SOCKET_ERROR) 
    {
        perror("bind");		     	
		ERRMSG = SYSERROR;
        return 1;
    }

    addr.sin_family = PF_INET;
    addr.sin_port = htons((short)server->port);
    addr.sin_addr = server->ip[0];
    memset( &(addr.sin_zero), 0, sizeof(addr.sin_zero) );

    if ( connect( server->fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)  
    {
      perror("connect");	  	  
	  ERRMSG = SYSERROR;
	  return 1;
    }
	
	return 0;
}

int bind_socket(SPTR server)
{    
	if (server->fd == -1)  
	{
	    return bind_server(server);	   
	}	
	else
	   return 1;
}

void set_fds( fd_set *fds, SPTR server)
{  
	if (server->fd != -1)
	    FD_SET( server->fd, fds);   
}

int connection_refused()
{    
   
  return errno = ECONNREFUSED;
}

int flush_server(SPTR server) 
{
    int pktlen, rc;
    char pkt[PACKET_LEN];
	fd_set read_fds;
    fd_set error_fds;
    struct timeval timeout;
	int retry_interval= DEFAULT_RETRY_INTERVAL;    


	FD_ZERO( &read_fds);
	FD_ZERO( &error_fds);
	set_fds( &read_fds, server);
	set_fds( &error_fds, server);
	
	timeout.tv_sec= retry_interval / 1000;
	timeout.tv_usec= (retry_interval % 1000) * 1000;
	
	rc = select(64, &read_fds, NULL, &error_fds, &timeout); 

	if (rc == SOCKET_ERROR)
	    return 1;	
	else if (rc == 0)
	   return 1; 	

	if (server->fd == -1 || (!FD_ISSET(server->fd, &read_fds)))
	   return 1;
	
	
	if ((pktlen= recv(server->fd, pkt, sizeof(pkt), 0)) == SOCKET_ERROR)  
	   return 0;

}

char * make_rule_packet(char *rn)
{
   unsigned char *t,*t2;   

   t2 = strdup(rn);
   t = malloc(sizeof(rule_info_request)+strlen(t2));
   if (t == NULL)
   {
      printf("Out of memory.\n");
      exit(1);
   }
   memcpy(t,&rule_info_request,sizeof(rule_info_request)-1); 
   memcpy((t+5),t2,strlen(t2)+1);
   *(t+3) = (6 + strlen(t2));
   free(t2);

   return t;
}

int send_packet(SPTR server, const char *pkt,int pktlen) 
{

	char sent=0;
	int oretry;
	    
	oretry = server->retry;
	
	if (server->fd != -1)  
	{
		while (!sent && (server->retry >= 0))
		{
		   if (send(server->fd,pkt,pktlen,0) == SOCKET_ERROR)
		   {			  
		      perror("send");
		      server->retry--;
		   }
		   else 
		     sent = 1;   	   
	     }
		
		if (server->retry <= 0) 
		  ERRMSG = TIMEOUT;		
	}	

	

	/*printf("Sent %d bytes.\n",pktlen);
	for (i = 0; i <= pktlen; i++)
	{
	   printf("Sent byte[%d] : %X\n",i,*(pkt+i));
	}
	printf("press a key\n");
	getch();
	*/
	server->retry = oretry;
	return (!sent);
}

int recv_server_info_packet(SPTR server,QSPTR qserver)
{
    short int pktlen, rc;
    unsigned char pkt[PACKET_LEN], *p;
	fd_set read_fds;
    fd_set error_fds;
    struct timeval timeout;
	
	

	FD_ZERO( &read_fds);
	FD_ZERO( &error_fds);
	set_fds( &read_fds, server);
	set_fds( &error_fds, server);

	timeout.tv_sec= retry_interval / 1000;
	timeout.tv_usec= (retry_interval % 1000) * 1000;

	rc = select(64, &read_fds, NULL, &error_fds, &timeout); 

	if (rc == SOCKET_ERROR)
	{
	    ERRMSG = SYSERROR;
	    perror("select");
	    return 1;
	}
	else if (rc == 0)
	{	
	   ERRMSG = TIMEOUT;
	   return 1; 
	}


	if (server->fd == -1 || (!FD_ISSET(server->fd, &read_fds)))
	{
	   ERRMSG = SYSERROR;
	   return 1;
	}
	
	
	if ((pktlen = recv(server->fd, pkt, sizeof(pkt), 0)) == SOCKET_ERROR)  
	{			   
	   ERRMSG = DOWN;
	   return 1;
	}
	
	
	if (pkt[4] == 0x83 && pkt[0] == 0x80 && pkt[1] == 0x00 && (pktlen == MAKEWORD(pkt[3],pkt[2])))
	{
	   p = &pkt[5];
       qserver->q_server_address = strdup(p);
	   p += strlen(p) + 1;
	   qserver->q_host_name = strdup(p);
	   p += strlen(p) + 1;
	   qserver->q_level_name = strdup(p);
	   qserver->q_current_players = pkt[pktlen-3];
	   qserver->q_max_players = pkt[pktlen-2];	
	   qserver->q_net_protocol_version = pkt[pktlen-1];
	}

	return 0;

}

int recv_player_info_packet(SPTR server,QPPTR player) 
{
    int pktlen, rc;
    unsigned char pkt[PACKET_LEN], *p;
	fd_set read_fds;
    fd_set error_fds;
    struct timeval timeout;
	


	FD_ZERO( &read_fds);
	FD_ZERO( &error_fds);
	set_fds( &read_fds, server);
	set_fds( &error_fds, server);

	
	timeout.tv_sec= retry_interval / 1000;
	timeout.tv_usec= (retry_interval % 1000) * 1000;
	
	

	rc = select(64, &read_fds, NULL, &error_fds, &timeout); 

	if (rc == SOCKET_ERROR)
	{
	    
		ERRMSG = SYSERROR;
	    
	    perror("select");
	    return 1;
	}
	else if (rc == 0)
	{
	   
	   ERRMSG = TIMEOUT;
	   
	   
	   return 2; 
	}


	if (server->fd == -1 || (!FD_ISSET(server->fd, &read_fds)))
	   return 3;
	
	
	if ((pktlen = recv(server->fd, pkt, sizeof(pkt), 0)) == SOCKET_ERROR)  
	{		
	   
	   ERRMSG = DOWN;
	   
	   
	   return 4;
	}
	
	if (pkt[4] == 0x84 && pkt[0] == 0x80 && pkt[1] == 0x00 && (pktlen == MAKEWORD(pkt[3],pkt[2])))
	{
	   player->player_number = pkt[5];
	   p = &pkt[6];
	   player->name = fix_pname(p);	   
	   p += strlen(p) + 1;
	   player->colors = *p;
	   p++;
	   player->unused1 = *p;
	   p++;
	   player->unused2 = *p;
	   p++;
	   player->unused3 = *p;
	   p++;	
	   player->frags = MAKELONG(MAKEWORD(*p,*(p+1)),MAKEWORD(*(p+2),*(p+3)));	
	   p += 4;	
	   player->connect_time = MAKELONG(MAKEWORD(*p,*(p+1)),MAKEWORD(*(p+2),*(p+3)));	
	   p += 4;
	   player->client_address = strdup(p);	  
   }
   return 0;
}

int recv_rule_info_packet(SPTR server,QRPTR rule) 
{
    int pktlen, rc;
    unsigned char pkt[PACKET_LEN], *p;
	fd_set read_fds;
    fd_set error_fds;
    struct timeval timeout;
	


	FD_ZERO( &read_fds);
	FD_ZERO( &error_fds);
	set_fds( &read_fds, server);
	set_fds( &error_fds, server);

	
	timeout.tv_sec= retry_interval / 1000;
	timeout.tv_usec= (retry_interval % 1000) * 1000;
	

	rc = select(64, &read_fds, NULL, &error_fds, &timeout); 

	if (rc == SOCKET_ERROR)
	{	    
		ERRMSG = SYSERROR;	 
	    perror("select");
	    return 1;
	}
	else if (rc == 0)
	{	   
	   ERRMSG = TIMEOUT;	 	
	   return 2; 
	}


	if (server->fd == -1 || (!FD_ISSET(server->fd, &read_fds)))
	   return 3;
	
	
	if ((pktlen = recv(server->fd, pkt, sizeof(pkt), 0)) == SOCKET_ERROR)  
	{			   
	   ERRMSG = DOWN;
	   return 4;
	}
	
	if (pkt[4] == 0x85 && pkt[0] == 0x80 && pkt[1] == 0x00 && (pktlen == MAKEWORD(pkt[3],pkt[2])))
	{  
	   if (pktlen != 5)
	   {
	       p = &pkt[5];
	       rule->name = strdup(p);	  		   
		   p += strlen(p) + 1;
		   rule->value = strdup(p); 
	   }
   }
   return 0;
}

void display_server(SPTR server, QSPTR qserver)
{	
    if (server != NULL && qserver != NULL) 
    {	  	  
	  printf("Server     : %s at %s (%s)\n",qserver->q_host_name,qserver->q_server_address,server->host_name);
	  printf("Level Name : %s\n",qserver->q_level_name);
	  printf("Players    : %u of %u\n",qserver->q_current_players,qserver->q_max_players);
	  printf("Net Proto. Ver. : %s\n",net_protocol_version_name(qserver->q_net_protocol_version)); 
	}
	else
	  printf("Not a valid server.\n");
}

void do_players(SPTR server,QSPTR qserver)
{
   QPPTR player = NULL;  
   char *ts = NULL,*es,pn;
	   

   printf("\nPID PLAYERNAME      FRAGS SHIRT    PANTS            TIME               ADDRESS\n\n");

   ts = malloc(sizeof(player_info_request));
   if (ts == NULL)
   {
      printf("Out of memory.\n");
	  return;
   }
   memcpy(ts,&player_info_request,sizeof(player_info_request)); 
   	
   for (pn = 0; pn < qserver->q_current_players; pn++)
   {
      
      player = make_qplayer(pn);
	  *(ts+5) = player->player_number;   
	  
	  if (send_packet(server,ts,sizeof(player_info_request)) != 0)
	  {
         printf("#%2u %-15s\n",pn,get_last_err()); 	  
    	 free(player);
	     player = NULL;
	     continue;
	   }
	  
	  if (recv_player_info_packet(server,player) != 0)
	  {	  
         if (!((es = get_last_err()) == NOERR))
	     {
	        printf("#%2u %-15s\n",pn,es);
		 }
		 free(player);
	     player = NULL;
	     continue;
	  }

      printf("#%2u %-15s %5d",pn,player->name,player->frags);       	         	
	  printf(" %-8s",color_name((char)((player->colors >> 4) & 0xF)));
      printf(" %-8s",color_name((char)(player->colors & 0xF)));
	  printf(" %12s",english_time(player->connect_time));
	  printf("  %20s\n",player->client_address);
	   	   
      free(player);         
    }
}


void do_rules(SPTR server)
{
    unsigned char *t,d = 0,*rn = NULL,rv;
	QRULE qrule;
	QRPTR qrp;
		
	clear();
	printf("Rules for %s\n\n",server->host_name);
	
	
	qrp = &(qrule);
	
	
	while (!d)
	{
		qrp->name = qrp->value = "";
	    t = make_rule_packet(rn);    		
    	send_packet(server,t,*(t+3));
		if ((rv = recv_rule_info_packet(server,qrp)) == 0)
		{
			if (strcmp((qrp->name),"") != 0)
		        printf("%-11s : %4s\n",qrp->name,qrp->value);
		}
		else
		    printf("\nRules list retrieve failed : %s\n",get_last_err());

		rn = strdup(qrp->name);		
		d = (strcmp((qrp->name),"") == 0 || rv != 0) ? 1 : 0;
		
	}		   

	printf("\n\n");
	my_pause();
	clear();
}


void cleanup(SPTR server)
{
	close(server->fd);
	server->fd= -1;
	
}

