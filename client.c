/******
 * CLIENT CODE
 *
 *****/

#include "headers.h"

int NS_Sock;
char message[1024];
Packet NS_Packet;

struct SS_DETAILS
{
    struct in_addr myIP;
    int NSPort;
    int ClientPort;
    sem_t SSlock;
} typedef SS;

void read_file(int SS_Sock)
{
    printf("\nThe contents of your requested file are-\n\n");
    Packet buffer;
    int status = recv(SS_Sock, &buffer, sizeof(buffer), 0);
    if (status == -1)
    {
        perror("Error in recieving data\n");
        close(SS_Sock);
        return;
    }
    else if (status == 0)
    {
        perror("Storage Server closed the connection\n");
        close(SS_Sock);
        return;
    }
    //continuously receive data packets=>print until ack packet is received 
    while (buffer.data_type != ACK_PACKET)
    {
        printf("%s", buffer.data);
        status = recv(SS_Sock, &buffer, sizeof(buffer), 0);
        if (status == -1)
        {
            perror("Error in recieving data\n");
            close(SS_Sock);
            return;
        }
        else if (status == 0)
        {
            perror("Storage Server closed the connection\n");
            close(SS_Sock);
            return;
        }
    }
    printf("\n");
    //print if there is any error,i.e, any other message than STOP in ack packet
    if (strcmp(buffer.data, "STOP") != 0)
    {
        printf("%s\n", buffer.data);
    }
    return;
}

void write_file(int SS_Sock)
{
    printf("\nPlease type in whatever you want to write in the file of your choice('STOP' to stop writing)-\n");
    Packet buffer;
    char *status = fgets(buffer.data, sizeof(buffer.data), stdin);
    buffer.data_type = DATA_PACKET;
    int send_stat, ack_stat;
    //continuously send data packets until "STOP" is entered by user
    while (strcmp(buffer.data, "STOP\n") != 0)
    {
        send_stat = send(SS_Sock, &buffer, sizeof(buffer), 0);
        if (send_stat == -1)
        {
            perror("Error in sending");
            close(SS_Sock);
        }
        else if (send_stat == 0)
        {
            perror("Storage Server closed the connection\n");
            close(SS_Sock);
            return;
        }
        status = fgets(buffer.data, sizeof(buffer.data), stdin);
    }
    strcpy(buffer.data, "STOP");
    //send "STOP"
    send_stat = send(SS_Sock, &buffer, sizeof(buffer), 0);
    if (send_stat == -1)
    {
        perror("Error in sending");
        close(SS_Sock);
    }
    else if (send_stat == 0)
    {
        perror("Storage Server closed the connection\n");
        close(SS_Sock);
        return;
    }
    //receive ack packet from SS
    ack_stat = recv(SS_Sock, &buffer, sizeof(buffer), 0);
    if (ack_stat == -1)
    {
        perror("Error in recieving data\n");
        close(SS_Sock);
        return;
    }
    else if (ack_stat == 0)
    {
        perror("Storage Server closed the connection\n");
        close(SS_Sock);
        return;
    }
    //print if there is any error,i.e, any other message than STOP in ack packet
    if (strcmp(buffer.data, "STOP") != 0)
    {
        printf("%s\n", buffer.data);
    }
    return;
}
void retrieve(int SS_Sock)
{
    printf("\nDetails of your requested file are-\n\n");
    Packet buffer;
    //receiving and printing size
    int status = recv(SS_Sock, &buffer, sizeof(buffer), 0);
    if (status == -1)
    {
        perror("Error in recieving data\n");
        close(SS_Sock);
        return;
    }
    else if (status == 0)
    {
        perror("Storage Server closed the connection\n");
        close(SS_Sock);
        return;
    }
    printf("%s\n", buffer.data);

    //receiving and printing permissions
    status = recv(SS_Sock, &buffer, sizeof(buffer), 0);
    if (status == -1)
    {
        perror("Error in recieving data\n");
        close(SS_Sock);
        return;
    }
    else if (status == 0)
    {
        perror("Storage Server closed the connection\n");
        close(SS_Sock);
        return;
    }
    printf("%s\n\n", buffer.data);

    //receiving and printing ack if any error,i.e, other than "STOP"
    status = recv(SS_Sock, &buffer, sizeof(buffer), 0);
    if (status == -1)
    {
        perror("Error in recieving data\n");
        close(SS_Sock);
        return;
    }
    else if (status == 0)
    {
        perror("Storage Server closed the connection\n");
        close(SS_Sock);
        return;
    }
    if (strcmp(buffer.data, "STOP") != 0)
    {
        printf("%s\n", buffer.data);
    }
    return;
}

/*


































*/
int main()
{
    // creating a socket
    NS_Sock = socket(AF_INET, SOCK_STREAM, 0);
    if (NS_Sock == -1)
    {
        perror("Error with socket");
    }

    // defining server address
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(12345);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    // connecting to the server
    if (connect(NS_Sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        perror("Error in connecting");
        close(NS_Sock);
    }

    // sending message
    Packet msg;
    strcpy(msg.data, "Client here!\n");
    msg.data_type = COMMAND_PACKET;
    int status = send(NS_Sock, &msg, sizeof(msg), 0);
    if (status == -1)
    {
        perror("Error in sending");
        close(NS_Sock);
    }
    else if (status == 0)
    {
        perror("Storage Server closed the connection\n");
        close(NS_Sock);
        return 0;
    }

    // recieving once
    Packet reply;
    status = recv(NS_Sock, &reply, sizeof(reply), 0);
    if (status == -1)
    {
        perror("Error in recieving data\n");
        close(NS_Sock);
        return 0;
    }
    else if (status == 0)
    {
        perror("Naming Server closed the connection\n");
        close(NS_Sock);
        return 0;
    }
    printf("%s\n", reply.data);

    // checking if connection limit exceeded
    if (strcmp("Number of clients exceeded! Please try again after some time.", reply.data) == 0)
    {
        close(NS_Sock);
        return 0;
    }
    // sending commands in loop
    while (1)
    {
        printf("==============================================================================================\n"
               "Here is an exhaustive list of all operations supported:-\n\n"
               "1. READ          -      To read data from a file.\n"
               "                        Syntax- READ <path to file>.\n\n"
               "2. WRITE         -      To write data into a file.\n"
               "                        Syntax- WRITE <path to file>.\n\n"
               "3. COPY          -      To copy and paste files and folders from source to destination.\n"
               "                        Syntax- COPY <path to source> PASTE <path to destination>\n\n"
               "4. CREATE        -      To create files or directories\n"
               "                        Syntax- CREATE <path to the new file/directory> <name>\n\n"
               "5. DELETE        -      To delete files or directories\n"
               "                        Syntax- DELETE <path to the file/directory>\n\n"
               "6. RETRIEVE      -      To get size and permission information of files\n"
               "                        Syntax- RETRIEVE <path of file>\n"
               "==============================================================================================\n");

        printf("Do you want to perform any operation(Y/N)?\n");
        char send_message[1024];
        fgets(send_message, sizeof(send_message), stdin);
        if (send_message[0] == 'N' || send_message[0] == 'n')
        {
            printf("Ok, closing the connection. BYE!\n");
            close(NS_Sock);
            break;
        }
        else if (send_message[0] == 'Y' || send_message[0] == 'y')
        {
            printf("Please type the operation you want to perform.\n");
            fgets(message, sizeof(message), stdin);
            char temp[1024];
            strcpy(temp, message);
            char *comm = strtok(temp, " \n");
            if (strcmp(comm, READ) == 0 || strcmp(comm, WRITE) == 0 || strcmp(comm, GET_INFO) == 0 || strcmp(comm, CREATE) == 0 || strcmp(comm, DELETE) == 0 || strcmp(comm, COPY) == 0)
            {
                strcpy(NS_Packet.data, message);
                NS_Packet.data_type = COMMAND_PACKET;
                int send_status = send(NS_Sock, &NS_Packet, sizeof(NS_Packet), 0);
                if (send_status == -1)
                {
                    perror("Error in sending");
                    close(NS_Sock);
                }
                else if (status == 0)
                {
                    perror("Storage Server closed the connection\n");
                    close(NS_Sock);
                    return 0;
                }
                // Set up fd_set for select
                fd_set read_fds;
                FD_ZERO(&read_fds);
                FD_SET(NS_Sock, &read_fds);

                // Set timeout of 5 seconds
                struct timeval timeout;
                timeout.tv_sec = 5; // 5 seconds timeout
                timeout.tv_usec = 0;

                // Wait for data or timeout
                int ready = select(NS_Sock + 1, &read_fds, NULL, NULL, &timeout);
                if (ready == -1)
                {
                    perror("Error in select");
                    exit(EXIT_FAILURE);
                }
                else if (ready == 0)
                {
                    // Timeout
                    system("clear");
                    printf("Timeout: No acknowledgment received from the server\n");
                    continue;
                }
                else
                {
                    // getting initial ACK bit for request confirmation
                    int Initial_ACK;
                    status = recv(NS_Sock, &Initial_ACK, sizeof(Initial_ACK), 0);
                    if (status == -1)
                    {
                        perror("Error in recieving data\n");
                        close(NS_Sock);
                        return 0;
                    }
                    else if (status == 0)
                    {
                        perror("Naming Server closed the connection\n");
                        close(NS_Sock);
                        return 0;
                    }
                    if (Initial_ACK == -1)
                    {
                        printf("Request not accepted by Naming Server\n");
                        Packet errorMsg;
                        status = recv(NS_Sock, &errorMsg, sizeof(errorMsg), 0);
                        if (status == -1)
                        {
                            perror("Error in recieving data\n");
                            close(NS_Sock);
                            return 0;
                        }
                        else if (status == 0)
                        {
                            perror("Naming Server closed the connection\n");
                            close(NS_Sock);
                            return 0;
                        }
                        system("clear");
                        printf("%s\n", errorMsg.data);
                        continue;
                    }
                    else
                    {
                        printf("Request accepted by Naming Server\n");
                    }
                }

                /****
                 * if op is READ_file/WRITE_file/RETRIEVE then send the op as it is along with path to NS
                 *
                 * then wait to get the IP of the respective SS
                 *
                 * further send the command to that SS and make a new connection for it
                 *
                 * finally display the result STOP packet recieved
                 *
                 ***/

                if (strcmp(comm, READ) == 0 || strcmp(comm, WRITE) == 0 || strcmp(comm, GET_INFO) == 0)
                {
                    // getting IP of SS
                    Packet SS_IP;
                    status = recv(NS_Sock, &SS_IP, sizeof(SS_IP), 0);
                    if (status == -1)
                    {
                        perror("Error in recieving data\n");
                        close(NS_Sock);
                        return 0;
                    }
                    else if (status == 0)
                    {
                        perror("Naming Server closed the connection\n");
                        close(NS_Sock);
                        return 0;
                    }
                    struct in_addr ss_ip;
                    inet_aton(SS_IP.data, &ss_ip);

                    // getting new port of SS for itself
                    int ss_port;
                    status = recv(NS_Sock, &ss_port, sizeof(ss_port), 0);
                    if (status == -1)
                    {
                        perror("Error in recieving data\n");
                        close(NS_Sock);
                        return 0;
                    }
                    else if (status == 0)
                    {
                        perror("Naming Server closed the connection\n");
                        close(NS_Sock);
                        return 0;
                    }

                    // creating a socket
                    int SS_Sock = socket(AF_INET, SOCK_STREAM, 0);
                    if (SS_Sock == -1)
                    {
                        perror("Error with socket");
                    }

                    // defining server address
                    struct sockaddr_in ss_serverAddress;
                    ss_serverAddress.sin_family = AF_INET;
                    ss_serverAddress.sin_port = htons(ss_port);
                    inet_pton(AF_INET, SS_IP.data, &ss_serverAddress.sin_addr);

                    // connecting to the SS
                    if (connect(SS_Sock, (struct sockaddr *)&ss_serverAddress, sizeof(ss_serverAddress)) == -1)
                    {
                        perror("Error in connecting");
                        close(SS_Sock);
                    }
                    else
                    {
                        // connection successful
                        printf("SS : Connection made\n");
                    }
                    //send command as it is to SS
                    int send_ss = send(SS_Sock, &NS_Packet, sizeof(NS_Packet), 0);
                    if (send_ss == -1)
                    {
                        perror("Error in sending");
                        close(SS_Sock);
                    }
                    else if (status == 0)
                    {
                        perror("Storage Server closed the connection\n");
                        close(SS_Sock);
                        return 0;
                    }
                    if (strcmp(comm, READ) == 0)
                    {
                        read_file(SS_Sock);
                    }
                    else if (strcmp(comm, WRITE) == 0)
                    {
                        write_file(SS_Sock);
                    }
                    else if (strcmp(comm, GET_INFO) == 0)
                    {
                        retrieve(SS_Sock);
                    }
                    close(SS_Sock);
                }

                // else if op was to create/delete/copy files and folders then just wait for the ACK bit/'STOP' packet as
                // implementation is done by SS

                else if (strcmp(comm, CREATE) == 0 || strcmp(comm, DELETE) == 0 || strcmp(comm, COPY) == 0)
                {
                    int Final_ACK;
                    status = recv(NS_Sock, &Final_ACK, sizeof(Final_ACK), 0);
                    if (status == -1)
                    {
                        perror("Error in recieving data\n");
                        close(NS_Sock);
                        return 0;
                    }
                    else if (status == 0)
                    {
                        perror("Naming Server closed the connection\n");
                        close(NS_Sock);
                        return 0;
                    }

                    // handle what to do after ack bit.
                    if (Final_ACK == -1)
                    {
                        Packet Err_Pack;
                        status = recv(NS_Sock, &Err_Pack, sizeof(Err_Pack), 0);
                        if (status == -1)
                        {
                            perror("Error in recieving data\n");
                            close(NS_Sock);
                            return 0;
                        }
                        else if (status == 0)
                        {
                            perror("Naming Server closed the connection\n");
                            close(NS_Sock);
                            return 0;
                        }
                        system("clear");
                        printf("%s\n", Err_Pack.data);
                        continue;
                    }
                    else if (Final_ACK == 1)
                    {
                        printf("Request processed by Naming Server\n");
                    }
                }
            }
            else
            {
                printf("Invalid operation :(\n");
            }
        }
        else
        {
            printf("Please type Y/y if you want to perform any operation or N/n if you don't want to ;)\n");
        }
        char enter[2];
        printf("Press 'Enter' to continue...\n");
        fgets(enter, sizeof(enter), stdin);
        while (strcmp(enter, "\n") != 0)
        {
            fgets(enter, sizeof(enter), stdin);
        }
        system("clear");
    }
    close(NS_Sock);

    return 0;
}