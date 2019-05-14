#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <curses.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

static struct addrinfo *addr_lander, *addr_dash;
static int socket_lander, socket_dash ;

static int input_key = 0;
static int control_throttle = 0;
static int control_rotation = 0;

FILE *datalog;

static sem_t sem_server, sem_command, sem_inputcmd, sem_inputlog, sem_datalog;
static bool input_toggle = 1;

void *thread_lander_condition(void *arg);
void *thread_lander_command(void *arg);
void *thread_input(void *arg);
void *thread_datalog(void *arg);

struct addrinfo *get_connection();

int bind_socket();

void initcurses();
void control_clean();
void set_input();

char *dash_protocol();

int main(int argc, char *argv[])
{	
	//LNCurses 
	initcurses();
	//Datalogger start
	datalog = fopen("datalog.txt", "w");
	//Error message if datalog can't be accessed	
	if(!datalog){
		fprintf(stderr,"can't open file %s \n", strerror(errno) );
		exit(EXIT_FAILURE);
	}
	//Semaphores
	sem_init(&sem_server, 0, 1);
	sem_init(&sem_inputlog,0 , 1);	
	sem_init(&sem_inputcmd,0 , 1);
	sem_init(&sem_datalog, 0, 0);
	sem_init(&sem_command, 0, 0);
	

	pthread_t t_input, t_lander_condition, t_lander_command, t_datalog;	

	addr_lander = get_connection("127.0.1.1", "65200");
	addr_dash = get_connection("127.0.1.1", "65250");
	socket_lander = bind_socket();
	socket_dash = bind_socket();
	
	pthread_create(&t_input, NULL, thread_input, NULL);
	pthread_create(&t_lander_condition, NULL, thread_lander_condition, NULL);
	pthread_create(&t_lander_command, NULL, thread_lander_command, NULL);
	pthread_create(&t_datalog, NULL, thread_datalog, NULL);
	pthread_join(t_input, NULL);
	pthread_join(t_lander_condition, NULL);
	pthread_join(t_lander_command, NULL);	
	pthread_join(t_datalog, NULL);
	
	endwin();

	fclose(datalog);
	pthread_cancel(t_input);
	pthread_cancel(t_lander_condition);
	pthread_cancel(t_lander_command);
	pthread_cancel(t_datalog);
	sem_unlink("sem_server");
	sem_unlink("sem_command");
	sem_unlink("sem_input");
	sem_unlink("sem_datalog");
	freeaddrinfo(addr_lander);
	freeaddrinfo(addr_dash);
	exit(0);
}

void initcurses() 
{	
	filter();	
   	initscr();
    cbreak();
    echo();
    intrflush(stdscr, TRUE);
    keypad(stdscr, TRUE);
}

void *thread_lander_command(void *arg)
{
	printf("Lander command thread running\r\n");
	const size_t buffer_size = 1024;
	char command[buffer_size], temp[buffer_size];
	
	while(input_toggle)
	{	
		sem_wait(&sem_command);
		memset(command, 0, sizeof(command));
		strcat(command, "command:!\nmain-engine: ");
		sprintf(temp, "%d\nrcs-roll: %i", control_throttle, control_rotation);
		strcat(command, temp); 
		printf("Command is : %s\r\n", command);			
		sem_wait(&sem_server);
		sendto(socket_lander, command, buffer_size, 0 , addr_lander->ai_addr, addr_lander->ai_addrlen);	
		sem_post(&sem_server);
		sem_post(&sem_inputcmd);
	}

	return NULL;
}
//Input listener
void *thread_input(void *arg)
{
            
    while(input_toggle)
    {	
		bool f_flag = false;	
		sem_wait(&sem_inputcmd);
		sem_wait(&sem_inputlog);
        input_key = getch();
        switch(input_key)
        {
            case KEY_UP : 
                control_throttle += 10;
                break;
            case KEY_DOWN :
               	control_throttle -= 10;
                break;
            case KEY_LEFT :
               	control_rotation -= 1;
                break;
            case KEY_RIGHT:
                control_rotation += 1;
                break;
			default :
				f_flag = true;
        }
		control_clean();

	if(!f_flag) 
	     {
		sem_post(&sem_command);
		sem_post(&sem_datalog);
		}
	else 	
	     {
		sem_post(&sem_inputcmd);
		sem_post(&sem_inputlog);
		}
    }
	control_throttle = 0;
	sem_post(&sem_command);
	sem_post(&sem_datalog);	
    return NULL;
}

//Port and Connection settings
struct addrinfo *get_connection(char* host, char* port)
{
    struct addrinfo *address;
    const struct addrinfo connection_hints =
    {
        .ai_family = AF_INET ,
        .ai_socktype = SOCK_DGRAM,
    };
    int connection_error;
    connection_error = getaddrinfo(host, port, &connection_hints, &address);
    if(connection_error == 0)
	{       
 		printf("Connected to : %s on %s port\r\n", host, port);
		return address;
   	}
	else
	{	
		printf("Error: %s\n\r", gai_strerror(connection_error));
       	exit(1);
	}
}

int bind_socket()
{
    int connection_socket;
    connection_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if(connection_socket == -1)
	{
		printf("Error : %s\n\r", strerror(errno));
		return 0;
	}	
    return connection_socket;
}
char *dash_protocol(char *outgoing, size_t size)
{	
	
	for (int x = 0; x<size ; x++) 
	{	
		if(outgoing[x] == '%')	
		{	
			for(int y = x; y<size; y++)
			{
				outgoing[x] = outgoing[x+1];
			}
			outgoing[size-1] = '\0';	
		}
	}
		
	return outgoing;
}

//Logged Data from inputs thread
void *thread_datalog(void *arg)
{
	while(input_toggle)
	{
		sem_wait(&sem_datalog);
		char * key;
		switch(input_key) {
			case KEY_UP :
				key = "Up/ Increase Throttle by 10";
				break;
			case KEY_DOWN :
				key = "Down/ Reduce Throttle by 10";
				break;
			case KEY_LEFT :
				//key = "Left/ Offset rotation left by 1";
				break;
			case KEY_RIGHT :
				//key = "Right/ Offset rotation right by 1";
				break;
	}

		fprintf(datalog, "User Pressed : %s\r\n\nThrottle : %d\r\nRotation : %i\r\n\n", key, control_throttle, control_rotation);	
		sem_post(&sem_inputlog);
	}

	return NULL;
}

void control_clean()
{
	if(control_throttle > 100) control_throttle = 100;
	else if (control_throttle < 0) control_throttle = 0;
	if(control_rotation > 2.0) control_rotation = 2.0;
	else if (control_rotation < -2.0) control_rotation = -2.0;
}
void set_input(char * condition)
{
	char * crashed;
	crashed = strstr(condition, "crashed");
	if(crashed != NULL || input_key == 81 || input_key == 113)
	input_toggle = 0;

}


//Dashboard Information Thread
void *thread_lander_condition(void *arg)
{
    int refreshRate= 100000;
    const size_t buffer_size = 1024;
    char server_outgoing[buffer_size], dash_outgoing[buffer_size];
    size_t msgsize;
    strcpy( server_outgoing, "condition:?\0");
    
	while(input_toggle)
    {
		sem_wait(&sem_server);
        sendto(socket_lander, server_outgoing, buffer_size, 0 , addr_lander->ai_addr,addr_lander->ai_addrlen);
        
	msgsize = recvfrom(socket_lander, dash_outgoing, buffer_size, 0 , addr_lander->ai_addr, &addr_lander->ai_addrlen);
        dash_outgoing[msgsize] = '\0';
        
strcpy(dash_protocol(dash_outgoing, buffer_size), dash_outgoing);	
        sendto(socket_dash, dash_outgoing, buffer_size, 0, addr_dash->ai_addr, addr_lander->ai_addrlen);
        
	set_input(dash_outgoing);
	sem_post(&sem_server);		
        usleep(refreshRate);
    }
	return NULL;
}

