#include "headers.h"

#define NSPort 12345
int ClientPort = 12345;
int ssSock;
int tempSock[2];

/*
ERROR CODES
0: OK
-1: Can't access file
-2: Can't perform operation
-3: Socket error
-4: Socket closed by client
*/

/*
   Creating a seperate client-listener thread that is created when NS gives a message that client is redirected.
   Use different types of packets, because differentiating data from NS very hard. (Discuss today)
*/

int ssclientSock;
// defining the server address
struct sockaddr_in ssaddress;
sem_t NS_lock;
sem_t stream_lock;
void get_file_name(char *path, char *store)
{
    int i;
    for (i = strlen(path) - 1; path[i] != '/' && i>=0; i--)
    {
    }
    strcpy(store, (path + i + 1));
    printf("FILENAME EXTRACTED: %s\n",store);
    return;
}

void get_dir_name(char *path, char *store)
{
    int i;
    for (i = strlen(path) - 2; path[i] != '/' && i>=0; i--)
    {
    }
    strcpy(store, path + i + 1);
    return;
}
// Client-SS functions
int read_file(char *path, int socket_to_send)
{
    int status;
    int fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        perror("Unable to access file!\n");
        return -1;
    }
    Packet p;
    p.data_type = DATA_PACKET;
    int data_len = read(fd, p.data, 1024);
    if (data_len == -1)
    {
        perror("Error reading file\n");
        close(fd);
        return -2;
    }
    while (data_len)
    {
        p.data_length = data_len;
        status = send(socket_to_send, &p, sizeof(p), 0);
        if (status == -1)
        {
            perror("send");
            close(fd);
            return -3;
        }
        else if (status == 0)
        {
            perror("Client closed the connection!\n");
            close(fd);
            return -4;
        }
        data_len = read(fd, p.data, 1024);
    }
    close(fd);
    strcpy(p.data, "STOP");
    p.data_length = strlen(p.data);
    p.data_type = ACK_PACKET;
    status = send(socket_to_send, &p, sizeof(p), 0);
    if (status == -1)
    {
        perror("send");
        close(fd);
        return -3;
    }
    else if (status == 0)
    {
        perror("Client closed the connection!\n");
        close(fd);
        return -4;
    }
    return 0;
}
// Keeping it for strings only
int write_file(char *path, int socket_to_recv)
{
    int fd = open(path, O_RDWR | O_APPEND);
    if (fd == -1)
    {
        perror("Unable to access file!\n");
        return -1;
    }
    Packet p;
    int status = recv(socket_to_recv, &p, sizeof(p), 0);
    if (status == -1)
    {
        perror("Error in recieving data\n");
        close(fd);
        return -3;
    }
    else if (status == 0)
    {
        perror("Client closed the connection\n");
        close(fd);
        return -4;
    }
    while (strcmp(p.data, "STOP") != 0)
    {
        if (write(fd, p.data, strlen(p.data)) == -1)
        {
            perror("Error in writing file\n");
            close(fd);
            return -2;
        }
        status = recv(socket_to_recv, &p, sizeof(p), 0);
        if (status == -1)
        {
            perror("Error in recieving data\n");
            close(fd);
            return -3;
        }
        else if (status == 0)
        {
            perror("Client closed the connection\n");
            close(fd);
            return -4;
        }
    }
    close(fd);
    strcpy(p.data, "STOP");
    p.data_type = ACK_PACKET;
    status = send(socket_to_recv, &p, sizeof(p), 0);
    if (status == -1)
    {
        perror("send");
        return -3;
    }
    else if (status == 0)
    {
        perror("Client closed the connection!\n");
        return -4;
    }
    return 0;
}

int get_file_info(char *path, int socket_to_send)
{
    struct stat file_info;
    int s = stat(path, &file_info);
    if (s == -1)
    {
        perror("Unable to get info about the file\n");
        return -2;
    }
    Packet p;
    p.data_type = DATA_PACKET;
    snprintf(p.data, 1024, "Size: %ld", file_info.st_size);
    s = send(socket_to_send, &p, sizeof(p), 0);
    if (s == -1)
    {
        perror("Error in sending\n");
        return -3;
    }
    else if (s == 0)
    {
        perror("Client closed the connection\n");
        return -4;
    }
    snprintf(p.data, 1024, "Permissions: %d", file_info.st_mode);
    s = send(socket_to_send, &p, sizeof(p), 0);
    if (s == -1)
    {
        perror("Error in sending\n");
        return -3;
    }
    else if (s == 0)
    {
        perror("Client closed the connection\n");
        return -4;
    }
    strcpy(p.data, "STOP");
    p.data_type = ACK_PACKET;
    s = send(socket_to_send, &p, sizeof(p), 0);
    if (s == -1)
    {
        perror("Error in sending\n");
        return -3;
    }
    else if (s == 0)
    {
        perror("Client closed the connection\n");
        return -4;
    }
    return 0;
}

// NS-SS functions
/*
Mode: 0 -> File
      1 -> Directory
*/
int create_file_or_dir(char *path, int mode)
{
    // Check whether file or directory
    if (mode == 0)
    {
        int fd = open(path, O_CREAT | O_RDWR, 0777);
        if (fd == -1)
        {
            perror("Unable to create the file\n");
            return -1;
        }
        if (fchmod(fd, 0777) == -1)
        {
            perror("Unable to modify permissions\n");
            return -1;
        }
    }
    else
    {
        if (mkdir(path, 0777) == -1)
        {
            perror("Unable to create directory\n");
            return -1;
        }
    }
    return 0;
}
int delete_file_or_dir(char *path)
{
    int mode;
    struct stat file_info;
    int s = stat(path, &file_info);
    if (S_ISDIR(file_info.st_mode))
    {
        mode = 1;
    }
    else
    {
        mode = 0;
    }
    if (mode == 0)
    {
        if (remove(path) == -1)
        {
            perror("Unable to delete file\n");
            return -1;
        }
        return 0;
    }
    // Recursive delete all file and subfolders, then delete the folder itself
    else
    {
        DIR *dir = opendir(path);
        if (dir == NULL)
        {
            perror("Unable to open directory\n");
            return -1;
        }
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            char fullpath[PATH_MAX];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
            if (delete_file_or_dir(fullpath) == -1)
            {
                closedir(dir);
                return -1;
            }
        }
        closedir(dir);
        return rmdir(path);
    }
}

int copy_file(char *path, int ns_socket)
{
    Packet p;
    p.data_type = DATA_PACKET;
    get_file_name(path, p.data);
    printf("%s",p.data);
    if(send(ns_socket, &p, sizeof(p), 0)==-1)
    {
        return -1;
    }
    int s = read_file(path, ns_socket);
    return s;
}
int copy_tar(char *path, int ns_socket)
{
    struct stat path_stat;
    char tar_path[1024];

    char create_command[1024];
    snprintf(create_command, sizeof(create_command), "tar -cvf archive.tar -C \"%s\" .", path);

    // Use system() to execute the tar creation command
    int status = system(create_command);

    // Check if the system() call was successful
    if (status == 0)
    {
        printf("Tar archive created successfully.\n");
        snprintf(tar_path, 1024, "./archive.tar");
    }
    else
    {
        fprintf(stderr, "Error creating tar archive.\n");
        return -1;
    }
    Packet p;
    p.data_type = DATA_PACKET;
    get_dir_name(path, p.data);
    if (send(ns_socket, &p, sizeof(p), 0) == -1)
    {
        return -1;
    }
    int s = read_file(tar_path, ns_socket);
    if (s == -1)
    {
        return -1;
    }
    if (remove(tar_path) == 0)
    {
        printf("Tar file deleted successfully.\n");
    }
    else
    {
        fprintf(stderr, "Error deleting tar file.\n");
        return -1;
    }
    return s;
}
int paste_tar(char *path, int write_stream)
{
    printf("PASTE getting called\n");
    int fd = open(path, O_RDWR | O_APPEND);
    if (fd == -1)
    {
        perror("Unable to access file!\n");
        return -1;
    }
    char buffer[1024];
    sem_wait(&stream_lock);
    int rlen;
    if (read(write_stream, &rlen, sizeof(rlen)) == -1)
    {
        perror("Error in recieving data\n");
        close(fd);
        return -3;
    }
    int status = read(write_stream, buffer, rlen);
    if (status == -1)
    {
        perror("Error in recieving data\n");
        close(fd);
        return -3;
    }
    while (strcmp(buffer, "STOP") != 0)
    {
        if (write(fd, buffer, status) == -1)
        {
            perror("Error in writing file\n");
            close(fd);
            return -2;
        }
        sem_wait(&stream_lock);
        read(write_stream, &rlen, sizeof(rlen));
        status = read(write_stream, buffer, rlen);
        if (status < 1024)
        {
            buffer[rlen] = '\0';
        }
        if (status == -1)
        {
            perror("Error in recieving data\n");
            close(fd);
            return -3;
        }
    }
    printf("PASTE completed\n");
    close(fd);
    char tar_file_path[1024];
    snprintf(tar_file_path, sizeof(tar_file_path), "%s", path);

    // Compose the tar extraction command using the tar file path and -C option
    char extract_command[1024];
    snprintf(extract_command, sizeof(extract_command), "tar -xvf %s -C $(dirname %s) > /dev/null 2>&1", tar_file_path, tar_file_path);

    // Use system() to execute the tar extraction command
    status = system(extract_command);

    // Check if the system() call was successful
    if (status == 0)
    {
        printf("Tar archive extracted successfully.\n");
        if (remove(tar_file_path) == 0)
        {
            printf("Tar file deleted successfully.\n");
        }
        else
        {
            fprintf(stderr, "Error deleting tar file.\n");
            return -1;
        }
    }
    else
    {
        fprintf(stderr, "Error extracting tar archive.\n");
        return -1;
    }
    return 0;
}
int paste_file(char *path, int write_stream)
{
    printf("PASTE FILE getting called\n");
    int fd = open(path, O_RDWR | O_APPEND);
    if (fd == -1)
    {
        perror("Unable to access file!\n");
        return -1;
    }
    char buffer[1024];
    sem_wait(&stream_lock);
    int rlen;
    if (read(write_stream, &rlen, sizeof(rlen)) == -1)
    {
        perror("Error in recieving data\n");
        close(fd);
        return -3;
    }
    int status = read(write_stream, buffer, rlen);
    if (status == -1)
    {
        perror("Error in recieving data\n");
        close(fd);
        return -3;
    }
    while (strcmp(buffer, "STOP") != 0)
    {
        if (write(fd, buffer, status) == -1)
        {
            perror("Error in writing file\n");
            close(fd);
            return -2;
        }
        sem_wait(&stream_lock);
        if (read(write_stream, &rlen, sizeof(rlen)) == -1)
        {
            perror("Error in recieving data\n");
            close(fd);
            return -3;
        }
        status = read(write_stream, buffer, rlen);
        if (status < 1024)
        {
            buffer[rlen] = '\0';
        }
        if (status == -1)
        {
            perror("Error in recieving data\n");
            close(fd);
            return -3;
        }
    }
    printf("Stop received\n");
    close(fd);
    return 0;
}

int self_copy(char *src, char *dest)
{
    char copyCommand[1024];
    snprintf(copyCommand, sizeof(copyCommand), "cp -r \"%s\" \"%s\"", src, dest);
    int status = system(copyCommand);
    if (status != 0)
    {
        fprintf(stderr, "Error copying file or directory.\n");
        return -1;
    }
    else
    {
        return 0;
    }
}
// Client routine for READ/WRITE ops
void *Client_Listener_Thread(void *arg)
{
    int status;
    int clientSock = accept(ssclientSock, NULL, NULL);
    if (clientSock == -1)
    {
        perror("Error in accepting connection\n");
        close(clientSock);
        return NULL;
    }
    Packet p;
    if (recv(clientSock, &p, sizeof(p), 0) <= 0)
    {
        perror("Error in recieving request\n");
        close(clientSock);
        return NULL;
    }
    char temp[1024];
    strcpy(temp, p.data);
    char *comm = strtok(temp, " \n");
    if (strcmp(comm, READ) == 0)
    {
        char *path = strtok(NULL, " \n");
        read_file(path, clientSock);
    }
    else if (strcmp(comm, WRITE) == 0)
    {
        char *path = strtok(NULL, " \n");
        status = write_file(path, clientSock);
        if(send(ssSock, &status, sizeof(status), 0)==-1)
        {
            return NULL;
        }
    }
    else if (strcmp(comm, GET_INFO) == 0)
    {
        char *path = strtok(NULL, " \n");
        get_file_info(path, clientSock);
    }
    else
    {
        Packet p;
        p.data_type = ACK_PACKET;
        strcpy(p.data, "Invalid command");
        if(send(clientSock, &p, sizeof(p), 0)==-1)
        {
            return NULL;
        }
    }
    close(clientSock);

    return NULL;
}

/*
































*/

void *NS_Listener_Thread(void *arg)
{
    Packet p = *(Packet *)arg;
    sem_post(&NS_lock);
    char *comm = strtok(p.data, " \n");
    int status;
    if (strcmp(comm, CREATE) == 0)
    {
        char *path = strtok(NULL, " \n");
        if (path[strlen(path) - 1] == '/')
        {
            status = create_file_or_dir(path, 1);
            if(send(ssSock, &status, sizeof(status), 0)==-1)
            {
                return NULL;
            }
        }
        else
        {
            printf("CREATE FOR FILE\n");
            status = create_file_or_dir(path, 0);
            if(send(ssSock, &status, sizeof(status), 0)==-1)
            {
                return NULL;
            }
        }
        if (status == -1)
        {
            Packet errdesc;
            errdesc.data_type = ACK_PACKET;
            strcpy(errdesc.data, "Unable to create the file");
            if(send(ssSock, &errdesc, sizeof(errdesc), 0)==-1)
            {
                return NULL;
            }
        }
    }
    else if (strcmp(comm, DELETE) == 0)
    {
        char *path = strtok(NULL, " \n");
        status = delete_file_or_dir(path);
        send(ssSock, &status, sizeof(status), 0);
        if (status == -1)
        {
            Packet errdesc;
            errdesc.data_type = ACK_PACKET;
            strcpy(errdesc.data, "Unable to delete the file");
            if(send(ssSock, &errdesc, sizeof(errdesc), 0)==-1)
            {
                return NULL;
            }
        }
    }
    else if (strcmp(comm, COPY) == 0)
    {
        char *path = strtok(NULL, " \n");
        copy_tar(path, ssSock);
    }
    else if (strcmp(comm, SELF_COPY) == 0)
    {
        char *src = strtok(NULL, " \n");
        strtok(NULL, " \n");
        char *dest = strtok(NULL, " \n");
        status = self_copy(src, dest);
        if(send(ssSock, &status, sizeof(status), 0)==-1)
        {
            return NULL;
        }
        if (status == -1)
        {
            Packet errdesc;
            errdesc.data_type = ACK_PACKET;
            strcpy(errdesc.data, "Unable to paste the file");
            if(send(ssSock, &errdesc, sizeof(errdesc), 0)==-1)
            {
                return NULL;
            }
        }
    }
    else if (strcmp(comm, COPY_FILE) == 0)
    {
        char *path = strtok(NULL, " \n");
        copy_file(path, ssSock);
    }
    else if (strcmp(comm, PASTE) == 0)
    {
        char *path = strtok(NULL, " \n");
        status = paste_tar(path, tempSock[1]);
        if(send(ssSock, &status, sizeof(status), 0)==-1)
        {
            return NULL;
        }
        if (status == -1)
        {
            Packet errdesc;
            errdesc.data_type = ACK_PACKET;
            strcpy(errdesc.data, "Unable to paste the file");
            if(send(ssSock, &errdesc, sizeof(errdesc), 0)==-1)
            {
                return NULL;
            }
        }
    }
    else if (strcmp(comm, PASTE_FILE) == 0)
    {
        char *path = strtok(NULL, " \n");
        status = paste_file(path, tempSock[1]);
        if(send(ssSock, &status, sizeof(status), 0)==-1)
        {
            return NULL;
        }
        if (status == -1)
        {
            Packet errdesc;
            errdesc.data_type = ACK_PACKET;
            strcpy(errdesc.data, "Unable to paste the file");
            send(ssSock, &errdesc, sizeof(errdesc), 0);
        }
    }
    return NULL;
}

/*































*/

int main(int argc, char *argv[])
{
    // Creating local sockets
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, tempSock) == -1)
    {
        perror("socketpair");
        return -1;
    }
    // Initializing semaphores
    sem_init(&NS_lock, 0, 0);
    sem_init(&stream_lock, 0, 0);

    // creating a socket for NS-SS interaction
    ssSock = socket(AF_INET, SOCK_STREAM, 0);
    if (ssSock == -1)
    {
        perror("Error with socket");
    }

    // defining the server address
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;

    serverAddress.sin_port = htons(NSPort);
    inet_pton(AF_INET, argv[1], &serverAddress.sin_addr);

    // connecting to the server
    if (connect(ssSock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        perror("Error in connecting");
        close(ssSock);
    }

    // sending message
    Packet p;
    p.data_type = DATA_PACKET;
    strcpy(p.data, "Storage Server here!\n");
    int status = send(ssSock, &p, sizeof(p), 0);
    if (status == -1)
    {
        perror("Error in sending");
        close(ssSock);
    }
    strcpy(p.data, argv[1]);
    if(send(ssSock, &p, sizeof(p), 0)==-1)
    {
        return -1;
    }
    // recieving once
    Packet reply;
    if (recv(ssSock, &reply, sizeof(reply), 0) == -1)
    {
        perror("Error in receiving message\n");
        close(ssSock);
    }

    // checking if connection limit exceeded
    if (strcmp("Number of storage servers limit exceeded! Please try again after some time.", reply.data) == 0)
    {
        close(ssSock);
        return 0;
    }
    // Get the offset for Client-SS interaction and open it for communication
    printf("Storage Server number: %s\n", reply.data);
    int offset = atoi(reply.data);
    ClientPort -= offset;
    // For SS-Client interaction
    ssclientSock = socket(AF_INET, SOCK_STREAM, 0);
    if (ssclientSock == -1)
    {
        perror("Error with socket");
    }
    ssaddress.sin_family = AF_INET;

    // binding socket to address
    ssaddress.sin_port = htons(ClientPort);
    ssaddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(ssclientSock, (struct sockaddr *)&ssaddress, sizeof(ssaddress)) == -1)
    {
        perror("Binding error");
        close(ssclientSock);
    }
    // listening for requests from clients on this socket
    if (listen(ssclientSock, MAX_CLIENTS) == -1)
    {
        perror("Error in listening");
        close(ssclientSock);
    }
    // sending essentials from SS to NS

    int msgINT = NSPort;
    if(send(ssSock, &msgINT, sizeof(msgINT), 0)==-1)
    {
        return -1;
    }
    msgINT = ClientPort;
    if(send(ssSock, &msgINT, sizeof(msgINT), 0)==-1)
    {
        return -1;
    }

    // taking input for exposed paths
    printf("Enter the paths to be exposed to the Clients (-1 to stop) : \n");
    while (1)
    {
        scanf("%s", p.data);
        if (strcmp(p.data, "-1") == 0)
        {
            strcpy(p.data, "");
            p.data[0] = -1;
            send(ssSock, &p, sizeof(p), 0);
            break;
        }
        struct stat s;
        if (stat(p.data, &s) == -1)
        {
            printf("The given path doesn't exist\n");
            continue;
        }
        send(ssSock, &p, sizeof(p), 0);
    }

    // ready to take commands now
    printf("\nOK! Now ready to take commands from NS\n");
    pthread_t thread;
    while (1)
    {
        if (recv(ssSock, &reply, sizeof(reply), MSG_DONTWAIT) == -1)
        {
            continue;
        }
        if (reply.data_type == ACK_PACKET)
        {
            continue;
        }
        else if (reply.data_type == PING_PACKET)
        {
            if(send(ssSock, &reply, sizeof(reply), 0)==-1){
                return -1;
            }
        }
        else if (reply.data_type != DATA_PACKET)
        {
            if (strstr(reply.data, CLIENT_CONNECTION) != NULL)
            {
                pthread_create(&thread, NULL, Client_Listener_Thread, NULL);
            }
            else
            {
                pthread_create(&thread, NULL, NS_Listener_Thread, &reply);
                sem_wait(&NS_lock);
            }
        }
        else
        {
            printf("TRANSFERRING %s TO WRITE STREAM\n", reply.data);
            printf("%d\n", reply.data_length);
            write(tempSock[0], &reply.data_length, sizeof(reply.data_length));
            write(tempSock[0], reply.data, reply.data_length);
            sem_post(&stream_lock);
        }
    }
    close(ssclientSock);
    return 0;
}