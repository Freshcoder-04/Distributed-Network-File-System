#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <signal.h>

#define PATH_MAX 4096

#define MAX_CLIENTS 3
#define MAX_SS 5

#define TOTAL_ASCII 128
#define PING_INTERVAL 5

// Definitions for commands shared by everyone
#define CLIENT_CONNECTION "CLIENTCONN"
#define READ "READ"
#define WRITE "WRITE"
#define COPY "COPY"
#define PASTE "PASTE"
#define CREATE "CREATE"
#define DELETE "DELETE"
#define GET_INFO "RETRIEVE"
#define SELF_COPY "SCOPY"
#define COPY_FILE "COPYFILE"
#define PASTE_FILE "PASTEFILE"

struct PACKET
{
    /****
     * CODES (please refer MACROS)
     * 0-> command data type
     * 1-> ACK bit type data
     * 2-> normal data (string)
     * 3-> PING type
     *
     */
    char data[1024];
    int data_type;
    int data_length;
} typedef Packet;

#define COMMAND_PACKET 0
#define ACK_PACKET 1
#define DATA_PACKET 2
#define PING_PACKET 3