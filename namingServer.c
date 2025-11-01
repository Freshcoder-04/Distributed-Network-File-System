/*******
 * NAMING SERVER CODE
 *
 * It creates a seperate thread for each incoming SUCCESFULLY accepted connection for clients as well as storage servers
 *
 * Further, in each connection, it allows sending and recieving of multiple messages (requests)
 *
 * It also marks the limit of number of connections that could be made
 *
 * It uses a 2D array to maintain socket file descriptors as well as their availability (boolean)
 * So, it allots the first available slot in the sockets to the incoming Client connection
 *
 * Structure to store details about incoming SS is implemented
 *
 ******/

#include "headers.h"

// structure definitions
struct SS_DETAILS
{
    struct in_addr myIP;
    int NSPort;
    int ClientPort;
} typedef SS;

struct TRIE_NODE
{
    char data;
    bool terminal;
    // terminal true for one character AFTER the last character
    struct TRIE_NODE *children[TOTAL_ASCII];
    int belongsToSS;
    bool writeLock;
} typedef TrieNode;

// global variables
int clientSockets[MAX_CLIENTS + 1][2];
int storageSockets[MAX_SS + 1][2];
int currClients;
int currSS;
SS allSS[MAX_SS + 1];
TrieNode *MOUNTED_ROOT;
TrieNode *FIRST_BACKUP;
TrieNode *SECOND_BACKUP;
// prefix is for testing purposes
char *prefix;
pthread_mutex_t LOCK;

/*
























*/

// function definitions
TrieNode *init_node()
{
    TrieNode *ret = (TrieNode *)malloc(sizeof(struct TRIE_NODE));

    // setting all values to NULL
    for (int n = 0; n < TOTAL_ASCII; n++)
    {
        ret->children[n] = NULL;
    }
    ret->writeLock = false;

    return ret;
}

void insert(TrieNode *node, char *address, int SS_number)
{
    if (*address == '\0')
    {
        node->terminal = true;
        node->belongsToSS = SS_number;
        // storing in which SS is that terminal file or folder is stored
        return;
    }

    int index = *address;
    if (node->children[index] == NULL)
    {
        node->children[index] = init_node();
        node->children[index]->data = *address;
        node->children[index]->terminal = false;
    }
    insert(node->children[index], address + 1, SS_number);
}

int search(TrieNode *node, char *word)
{
    /***
     * returns -1 if address not found anywhere
     *
     * otherwise returns the SS number it belongs to
     *
     **/

    if (*word == '\0')
    {
        if (node->terminal)
        {
            // it exists and belongs to the given SS number
            return node->belongsToSS;
        }
        else
        {
            return -1;
        }
    }

    int index = *word;
    if (node->children[index] == NULL)
    {
        return -1;
    }

    return search(node->children[index], word + 1);
}

int search_and_LOCK(TrieNode *node, char *word)
{
    /****
     * return -1 means write lock is already taken
     * return 1 means write lock was free so taken
     ***/

    // reached address end
    if (*word == '\0')
    {
        if (node->writeLock)
        {
            // write lock is already taken
            return -1;
        }
        else if (node->terminal)
        {
            node->writeLock = true;
            return 1;
        }
    }

    int index = *word;
    search_and_LOCK(node->children[index], word + 1);
}

void writeUNLOCK(TrieNode *node, char *word)
{
    // reached address end
    if (*word == '\0')
    {
        node->writeLock = false;
        return;
    }

    int index = *word;
    writeUNLOCK(node->children[index], word + 1);
}

int deleteFromTrie(TrieNode *node, char *word)
{
    /***
     * returns -1 from first letter removal onwards as deletion process
     *
     * returns 0 once deletion has been done
     *
     **/

    // end of address reached
    if (*word == '\0')
    {
        return -1;
    }

    // otherwise check next letter
    int index = *word;
    if (node->children[index] == NULL)
    {
        // did not find so the word anyways does not exist
        return 0;
    }

    // first, delete next letter
    int retVal = deleteFromTrie(node->children[index], word + 1);

    // deleting current letter depending upon retVal
    if (retVal == -2)
    {
        // removing letter
        if ((node->children['\0'] != NULL && !node->children['\0']->terminal) || node->children['\0'] == NULL)
        {
            free(node);
            return -1;
        }
        else
        {
            return 0;
        }
    }
    else if (retVal == -1 && node->data == '/')
    {
        // change retVal to 0 from now on since no more deletion
        return 0;
    }
    else if (retVal == -1 && node->data != '/')
    {
        // removal of the letter with retVal as -1
        if ((node->children['\0'] != NULL && !node->children['\0']->terminal) || node->children['\0'] == NULL)
        {
            free(node);
            return -1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }
}

/*































*/

// creating all ops functions

void NS_READ(Packet operation, int clientNum)
{
    char *token;
    char path[1024];
    int SS_for_OP;
    token = strtok(operation.data, " \n");
    if (token != NULL)
    {
        token = strtok(NULL, " \n");
        if (token != NULL)
        {
            // getting the path
            strcpy(path, token);

            // checking if the path exists
            SS_for_OP = search(MOUNTED_ROOT, path);

            // sending ACK bit
            if (SS_for_OP == -1)
            {
                int ACKBIT = -1;
                int status = send(clientSockets[clientNum - 1][0], &ACKBIT, sizeof(ACKBIT), 0);
                if (status == -1)
                {
                    perror("Error in sending");
                    close(clientSockets[clientNum - 1][0]);
                }
                printf("Sending error message\n");
                Packet errorMsg;
                strcpy(errorMsg.data, "Could not find the given path!");
                status = send(clientSockets[clientNum - 1][0], &errorMsg, sizeof(errorMsg), 0);
                if (status == -1)
                {
                    perror("Error in sending");
                    close(clientSockets[clientNum - 1][0]);
                }
                else if (status == 0)
                {
                    perror("Naming Server closed the connection\n");
                    close(clientSockets[clientNum - 1][0]);
                }
                return;
            }
            else
            {
                int ACKBIT = ACK_PACKET;
                int status = send(clientSockets[clientNum - 1][0], &ACKBIT, sizeof(ACKBIT), 0);
                if (status == -1)
                {
                    perror("Error in sending");
                    close(clientSockets[clientNum - 1][0]);
                }
                else if (status == 0)
                {
                    perror("Naming Server closed the connection\n");
                    close(clientSockets[clientNum - 1][0]);
                }
            }
        }
    }

    // sending operation request as it is to respective SS
    Packet alert;
    strcpy(alert.data, CLIENT_CONNECTION);
    int status = send(storageSockets[SS_for_OP - 1][0], &alert, sizeof(operation), 0);
    if (status == -1)
    {
        perror("Error in sending");
        close(storageSockets[SS_for_OP - 1][0]);
    }
    else if (status == 0)
    {
        perror("Naming Server closed the connection\n");
        close(storageSockets[SS_for_OP - 1][0]);
    }

    // sending SS details to the Client
    Packet SS_IPData;
    int SS_clientPort = allSS[SS_for_OP - 1].ClientPort;
    strcpy(SS_IPData.data, inet_ntoa(allSS[SS_for_OP - 1].myIP));
    status = send(clientSockets[clientNum - 1][0], &SS_IPData, sizeof(SS_IPData), 0);
    if (status == -1)
    {
        perror("Error in sending");
        close(clientSockets[clientNum - 1][0]);
    }
    else if (status == 0)
    {
        perror("Naming Server closed the connection\n");
        close(clientSockets[clientNum - 1][0]);
    }
    status = send(clientSockets[clientNum - 1][0], &SS_clientPort, sizeof(SS_clientPort), 0);
    if (status == -1)
    {
        perror("Error in sending");
        close(clientSockets[clientNum - 1][0]);
    }
    else if (status == 0)
    {
        perror("Naming Server closed the connection\n");
        close(clientSockets[clientNum - 1][0]);
    }
}

/*

































*/

void NS_WRITE(Packet operation, int clientNum)
{
    char *token;
    char path[1024];
    int SS_for_OP;
    token = strtok(operation.data, " ");
    if (token != NULL)
    {
        token = strtok(NULL, " \n");
        if (token != NULL)
        {
            // getting the path
            strcpy(path, token);

            // checking if the path exists (inside lock so nobody else takes it first)
            pthread_mutex_lock(&LOCK);
            SS_for_OP = search(MOUNTED_ROOT, path);

            // sending ACK bit
            if (SS_for_OP == -1)
            {
                int ACKBIT = -1;
                int status = send(clientSockets[clientNum - 1][0], &ACKBIT, sizeof(ACKBIT), 0);
                if (status == -1)
                {
                    perror("Error in sending");
                    close(clientSockets[clientNum - 1][0]);
                }
                else if (status == 0)
                {
                    perror("Naming Server closed the connection\n");
                    close(clientSockets[clientNum - 1][0]);
                }

                // sending error message also
                Packet errorMsg;
                strcpy(errorMsg.data, "Could not find the given path!");
                status = send(clientSockets[clientNum - 1][0], &errorMsg, sizeof(errorMsg), 0);
                if (status == -1)
                {
                    perror("Error in sending");
                    close(clientSockets[clientNum - 1][0]);
                }
                else if (status == 0)
                {
                    perror("Naming Server closed the connection\n");
                    close(clientSockets[clientNum - 1][0]);
                }
                pthread_mutex_unlock(&LOCK);
                return;
            }
            else
            {
                if (search_and_LOCK(MOUNTED_ROOT, path) == -1)
                {
                    // already in writing process
                    int ACKBIT = -1;
                    int status = send(clientSockets[clientNum - 1][0], &ACKBIT, sizeof(ACKBIT), 0);
                    if (status == -1)
                    {
                        perror("Error in sending");
                        close(clientSockets[clientNum - 1][0]);
                    }
                    else if (status == 0)
                    {
                        perror("Naming Server closed the connection\n");
                        close(clientSockets[clientNum - 1][0]);
                    }

                    // sending error message also
                    Packet errorMsg;
                    strcpy(errorMsg.data, "SS already performing a WRITE on the given path!");
                    status = send(clientSockets[clientNum - 1][0], &errorMsg, sizeof(errorMsg), 0);
                    if (status == -1)
                    {
                        perror("Error in sending");
                        close(clientSockets[clientNum - 1][0]);
                    }
                    else if (status == 0)
                    {
                        perror("Naming Server closed the connection\n");
                        close(clientSockets[clientNum - 1][0]);
                    }
                    pthread_mutex_unlock(&LOCK);
                    return;
                }
                else
                {
                    // not in writing process
                    int ACKBIT = ACK_PACKET;
                    int status = send(clientSockets[clientNum - 1][0], &ACKBIT, sizeof(ACKBIT), 0);
                    if (status == -1)
                    {
                        perror("Error in sending");
                        close(clientSockets[clientNum - 1][0]);
                    }
                    else if (status == 0)
                    {
                        perror("Naming Server closed the connection\n");
                        close(clientSockets[clientNum - 1][0]);
                    }
                }
            }
            pthread_mutex_unlock(&LOCK);
        }
    }

    // sending operation request as it is to respective SS
    Packet alert;
    strcpy(alert.data, CLIENT_CONNECTION);
    int status = send(storageSockets[SS_for_OP - 1][0], &alert, sizeof(operation), 0);
    if (status == -1)
    {
        perror("Error in sending");
        close(storageSockets[SS_for_OP - 1][0]);
    }
    else if (status == 0)
    {
        perror("Naming Server closed the connection\n");
        close(storageSockets[SS_for_OP - 1][0]);
    }

    // sending SS details to the Client
    Packet SS_IPData;
    int SS_clientPort = allSS[SS_for_OP - 1].ClientPort;
    strcpy(SS_IPData.data, inet_ntoa(allSS[SS_for_OP - 1].myIP));
    status = send(clientSockets[clientNum - 1][0], &SS_IPData, sizeof(SS_IPData), 0);
    if (status == -1)
    {
        perror("Error in sending");
        close(clientSockets[clientNum - 1][0]);
    }
    else if (status == 0)
    {
        perror("Naming Server closed the connection\n");
        close(clientSockets[clientNum - 1][0]);
    }
    status = send(clientSockets[clientNum - 1][0], &SS_clientPort, sizeof(SS_clientPort), 0);
    if (status == -1)
    {
        perror("Error in sending");
        close(clientSockets[clientNum - 1][0]);
    }
    else if (status == 0)
    {
        perror("Naming Server closed the connection\n");
        close(clientSockets[clientNum - 1][0]);
    }

    // recieving done bit from SS to free the lock
    int yo;
    recv(storageSockets[SS_for_OP - 1][0], &yo, sizeof(yo), 0);
    pthread_mutex_lock(&LOCK);
    writeUNLOCK(MOUNTED_ROOT, path);
    pthread_mutex_unlock(&LOCK);

    printf("Client%d calls write operation for SS%d", clientNum, SS_for_OP);
}

/*




























*/

void NS_RETRIEVE(Packet operation, int clientNum)
{
    char *token;
    char path[1024];
    int SS_for_OP;
    token = strtok(operation.data, " ");
    if (token != NULL)
    {
        token = strtok(NULL, " \n");
        if (token != NULL)
        {
            // getting the path
            strcpy(path, token);

            // checking if the path exists
            SS_for_OP = search(MOUNTED_ROOT, path);

            // sending ACK bit
            if (SS_for_OP == -1)
            {
                int ACKBIT = -1;
                int status = send(clientSockets[clientNum - 1][0], &ACKBIT, sizeof(ACKBIT), 0);
                if (status == -1)
                {
                    perror("Error in sending");
                    close(clientSockets[clientNum - 1][0]);
                }
                else if (status == 0)
                {
                    perror("Naming Server closed the connection\n");
                    close(clientSockets[clientNum - 1][0]);
                }

                // sending error message also
                Packet errorMsg;
                strcpy(errorMsg.data, "Could not find the given path!");
                status = send(clientSockets[clientNum - 1][0], &errorMsg, sizeof(errorMsg), 0);
                if (status == -1)
                {
                    perror("Error in sending");
                    close(clientSockets[clientNum - 1][0]);
                }
                else if (status == 0)
                {
                    perror("Naming Server closed the connection\n");
                    close(clientSockets[clientNum - 1][0]);
                }
                return;
            }
            else
            {
                int ACKBIT = ACK_PACKET;
                int status = send(clientSockets[clientNum - 1][0], &ACKBIT, sizeof(ACKBIT), 0);
                if (status == -1)
                {
                    perror("Error in sending");
                    close(clientSockets[clientNum - 1][0]);
                }
                else if (status == 0)
                {
                    perror("Naming Server closed the connection\n");
                    close(clientSockets[clientNum - 1][0]);
                }
            }
        }
    }

    // sending operation request as it is to respective SS
    Packet alert;
    strcpy(alert.data, CLIENT_CONNECTION);
    int status = send(storageSockets[SS_for_OP - 1][0], &alert, sizeof(operation), 0);
    if (status == -1)
    {
        perror("Error in sending");
        close(storageSockets[SS_for_OP - 1][0]);
    }
    else if (status == 0)
    {
        perror("Naming Server closed the connection\n");
        close(storageSockets[SS_for_OP - 1][0]);
    }

    // sending SS details to the Client
    Packet SS_IPData;
    int SS_clientPort = allSS[SS_for_OP - 1].ClientPort;
    strcpy(SS_IPData.data, inet_ntoa(allSS[SS_for_OP - 1].myIP));
    status = send(clientSockets[clientNum - 1][0], &SS_IPData, sizeof(SS_IPData), 0);
    if (status == -1)
    {
        perror("Error in sending");
        close(clientSockets[clientNum - 1][0]);
    }
    else if (status == 0)
    {
        perror("Naming Server closed the connection\n");
        close(clientSockets[clientNum - 1][0]);
    }
    status = send(clientSockets[clientNum - 1][0], &SS_clientPort, sizeof(SS_clientPort), 0);
    if (status == -1)
    {
        perror("Error in sending");
        close(clientSockets[clientNum - 1][0]);
    }
    else if (status == 0)
    {
        perror("Naming Server closed the connection\n");
        close(clientSockets[clientNum - 1][0]);
    }
}
/*


























*/

void NS_CREATE(Packet operation, int clientNum)
{
    char *token;
    char path[1024];
    int SS_for_OP;

    // copying somewhere
    char temp[1024];
    strcpy(temp, operation.data);

    token = strtok(temp, " ");
    if (token != NULL)
    {
        token = strtok(NULL, " \n");
        if (token != NULL)
        {
            // getting the path
            strcpy(path, token);

            // checking if the path exists
            SS_for_OP = search(MOUNTED_ROOT, path);

            // checking
            if (SS_for_OP == -1)
            {
                // checking if parent folder exists
                char parentPath[1024];
                for (int p = strlen(path) - 2; p > -1; p--)
                {
                    if (path[p] == '/')
                    {
                        strcpy(parentPath, path);
                        parentPath[p + 1] = '\0';
                        break;
                    }
                }

                // searching for parent folder
                int parentSS_for_OP = search(MOUNTED_ROOT, parentPath);
                if (parentSS_for_OP == -1)
                {
                    // even the parent folder does not exist
                    if (clientNum >= 0)
                    {
                        int ACKBIT = -1;
                        int status = send(clientSockets[clientNum - 1][0], &ACKBIT, sizeof(ACKBIT), 0);
                        if (status == -1)
                        {
                            perror("Error in sending");
                            close(clientSockets[clientNum - 1][0]);
                        }
                        else if (status == 0)
                        {
                            perror("Naming Server closed the connection\n");
                            close(clientSockets[clientNum - 1][0]);
                        }

                        // sending error message also
                        Packet errorMsg;
                        strcpy(errorMsg.data, "Path for given parent folder does not exist!");
                        status = send(clientSockets[clientNum - 1][0], &errorMsg, sizeof(errorMsg), 0);
                        if (status == -1)
                        {
                            perror("Error in sending");
                            close(clientSockets[clientNum - 1][0]);
                        }
                        else if (status == 0)
                        {
                            perror("Naming Server closed the connection\n");
                            close(clientSockets[clientNum - 1][0]);
                        }
                    }
                    return;
                }
                else
                {
                    // sending ACK to Client
                    int newACK = 1;
                    if (clientNum > -1)
                        send(clientSockets[clientNum - 1][0], &newACK, sizeof(newACK), 0);

                    // sending operation to respective SS
                    operation.data_type = 0;
                    send(storageSockets[parentSS_for_OP - 1][0], &operation, sizeof(operation), 0);

                    // recieving ACK for the same from SS
                    recv(storageSockets[parentSS_for_OP - 1][0], &newACK, sizeof(newACK), 0);
                    if (newACK == 0)
                    {
                        // sending ACK to Client for the operation
                        newACK = 1;
                        if (clientNum > -1)
                            send(clientSockets[clientNum - 1][0], &newACK, sizeof(newACK), 0);

                        // adding path to trie
                        insert(MOUNTED_ROOT, path, parentSS_for_OP);
                    }
                    else if (newACK == -1)
                    {
                        // sending ACK to Client for the operation
                        Packet err_pack;
                        recv(storageSockets[parentSS_for_OP - 1][0], &err_pack, sizeof(err_pack), 0);
                        if (clientNum > -1)
                        {
                            send(clientSockets[clientNum - 1][0], &newACK, sizeof(newACK), 0);
                            // recieving error
                            send(clientSockets[clientNum - 1][0], &err_pack, sizeof(err_pack), 0);
                        }
                    }
                }
            }
            else
            {
                // path exists already
                if (clientNum > -1)
                {

                    int ACKBIT = -1;
                    int status = send(clientSockets[clientNum - 1][0], &ACKBIT, sizeof(ACKBIT), 0);
                    if (status == -1)
                    {
                        perror("Error in sending");
                        close(clientSockets[clientNum - 1][0]);
                    }
                    else if (status == 0)
                    {
                        perror("Naming Server closed the connection\n");
                        close(clientSockets[clientNum - 1][0]);
                    }

                    // sending error message also
                    Packet errorMsg;
                    strcpy(errorMsg.data, "Path already exists!");
                    status = send(clientSockets[clientNum - 1][0], &errorMsg, sizeof(errorMsg), 0);
                    if (status == -1)
                    {
                        perror("Error in sending");
                        close(clientSockets[clientNum - 1][0]);
                    }
                    else if (status == 0)
                    {
                        perror("Naming Server closed the connection\n");
                        close(clientSockets[clientNum - 1][0]);
                    }
                }
                return;
            }
        }
    }

    printf("Client%d calls create operation for SS%d", clientNum, SS_for_OP);
}

/*






















*/

void NS_DELETE(Packet operation, int clientNum)
{
    int status;
    char *token;
    char path[1024];
    int SS_for_OP;
    char temp[1024];
    strcpy(temp, operation.data);
    token = strtok(temp, " ");
    if (token != NULL)
    {
        token = strtok(NULL, " \n");
        if (token != NULL)
        {
            // getting the path
            strcpy(path, token);

            // checking if the path exists
            SS_for_OP = search(MOUNTED_ROOT, path);

            // sending ACK bit
            if (SS_for_OP == -1)
            {
                // path does not exist
                int ACKBIT = -1;
                status = send(clientSockets[clientNum - 1][0], &ACKBIT, sizeof(ACKBIT), 0);
                if (status == -1)
                {
                    perror("Error in sending");
                    close(clientSockets[clientNum - 1][0]);
                }
                else if (status == 0)
                {
                    perror("Naming Server closed the connection\n");
                    close(clientSockets[clientNum - 1][0]);
                }

                // sending error message also
                Packet errorMsg;
                strcpy(errorMsg.data, "Path does not exist to delete!");
                status = send(clientSockets[clientNum - 1][0], &errorMsg, sizeof(errorMsg), 0);
                if (status == -1)
                {
                    perror("Error in sending");
                    close(clientSockets[clientNum - 1][0]);
                }
                else if (status == 0)
                {
                    perror("Naming Server closed the connection\n");
                    close(clientSockets[clientNum - 1][0]);
                }
                return;
            }
            else
            {
                // sending ACK to Client
                int newACK = 1;
                status = send(clientSockets[clientNum - 1][0], &newACK, sizeof(newACK), 0);
                if (status == -1)
                {
                    perror("Error in sending");
                    close(clientSockets[clientNum - 1][0]);
                }
                else if (status == 0)
                {
                    perror("Naming Server closed the connection\n");
                    close(clientSockets[clientNum - 1][0]);
                }

                // sending command to SS
                send(storageSockets[SS_for_OP - 1][0], &operation, sizeof(operation), 0);

                // recieving newACK
                recv(storageSockets[SS_for_OP - 1][0], &newACK, sizeof(newACK), 0);

                if (newACK == 0)
                {
                    // sending ACK to Client for the operation
                    newACK = 1;
                    status = send(clientSockets[clientNum - 1][0], &newACK, sizeof(newACK), 0);
                    if (status == -1)
                    {
                        perror("Error in sending");
                        close(clientSockets[clientNum - 1][0]);
                    }
                    else if (status == 0)
                    {
                        perror("Naming Server closed the connection\n");
                        close(clientSockets[clientNum - 1][0]);
                    }

                    // deleting path from trie
                    deleteFromTrie(MOUNTED_ROOT, path);
                }
                else if (newACK == -1)
                {
                    // sending ACK to Client for the operation
                    status = send(clientSockets[clientNum - 1][0], &newACK, sizeof(newACK), 0);
                    if (status == -1)
                    {
                        perror("Error in sending");
                        close(clientSockets[clientNum - 1][0]);
                    }
                    else if (status == 0)
                    {
                        perror("Naming Server closed the connection\n");
                        close(clientSockets[clientNum - 1][0]);
                    }
                    // recieving error
                    Packet err_pack;
                    recv(storageSockets[SS_for_OP - 1][0], &err_pack, sizeof(err_pack), 0);
                    send(clientSockets[clientNum - 1][0], &err_pack, sizeof(err_pack), 0);
                }
            }
        }
    }

    printf("Client%d calls delete operation for SS%d", clientNum, SS_for_OP);
}
/*


































*/
void NS_COPY_FILE(int SS_for_OP_paste, int SS_for_OP_copy, int clientNum, char *fourthWord, TrieNode *ROOT)
{
    // recieving file name
    Packet p;
    recv(storageSockets[SS_for_OP_copy - 1][0], &p, sizeof(p), 0);

    // creating final path
    char finalPath[1024];
    strcpy(finalPath, fourthWord);
    strcat(finalPath, p.data);

    // creating new address to create
    Packet newP;
    strcpy(newP.data, "CREATE ");
    strcat(newP.data, finalPath);

    // sending command to create new file first
    printf("DATA SENT TO CREATE: %s\n", newP.data);
    NS_CREATE(newP, -1);
    strcpy(newP.data, "PASTEFILE ");
    strcat(newP.data, finalPath);
    send(storageSockets[SS_for_OP_paste - 1][0], &newP, sizeof(newP), 0);
    // recieving data
    while (1)
    {
        recv(storageSockets[SS_for_OP_copy - 1][0], &newP, sizeof(newP), 0);
        newP.data_type = DATA_PACKET;
        send(storageSockets[SS_for_OP_paste - 1][0], &newP, sizeof(newP), 0);
        if (strcmp(newP.data, "STOP") == 0)
        {
            break;
        }
    }

    // recieving ACK for the same from SS
    int newACK;
    recv(storageSockets[SS_for_OP_paste - 1][0], &newACK, sizeof(newACK), 0);
    if (newACK == 0)
    {
        // sending ACK to Client for the operation
        newACK = 1;
        if (clientNum > -1)
            send(clientSockets[clientNum - 1][0], &newACK, sizeof(newACK), 0);

        // adding path to trie
        insert(ROOT, finalPath, SS_for_OP_paste);
    }
    else if (newACK == -1)
    {
        // sending ACK to Client for the operation
        if (clientNum > -1)
            send(clientSockets[clientNum - 1][0], &newACK, sizeof(newACK), 0);
        // recieving error
        Packet err_pack;
        recv(storageSockets[SS_for_OP_paste - 1][0], &err_pack, sizeof(err_pack), 0);
        if (clientNum > -1)
            send(clientSockets[clientNum - 1][0], &err_pack, sizeof(err_pack), 0);
    }
}

/*




























*/

void NS_COPY_FOLDER(int SS_for_OP_paste, int SS_for_OP_copy, int clientNum, char *fourthWord, TrieNode *ROOT)
{

    // Recieve dirname
    Packet p;
    recv(storageSockets[SS_for_OP_copy - 1][0], &p, sizeof(p), 0);

    // creating final path
    char finalPath[1024];
    strcpy(finalPath, fourthWord);
    strcat(finalPath, p.data);

    // creating new address to create
    Packet newP;
    strcpy(newP.data, "CREATE ");
    strcat(newP.data, finalPath);

    // sending command to create new file first
    printf("DATA SENT TO CREATE: %s\n", newP.data);
    NS_CREATE(newP, -1);

    strcpy(newP.data, "CREATE ");
    strcat(newP.data, finalPath);
    strcat(newP.data, "archive.tar");
    NS_CREATE(newP, -1);
    printf("DATA SENT TO CREATE: %s\n", newP.data);

    strcpy(newP.data, "PASTE ");
    strcat(finalPath, "archive.tar");
    strcat(newP.data, finalPath);
    printf("Final path: %s\n", finalPath);
    send(storageSockets[SS_for_OP_paste - 1][0], &newP, sizeof(newP), 0);

    // recieving data
    while (1)
    {
        recv(storageSockets[SS_for_OP_copy - 1][0], &newP, sizeof(newP), 0);
        newP.data_type = DATA_PACKET;
        send(storageSockets[SS_for_OP_paste - 1][0], &newP, sizeof(newP), 0);
        if (strcmp(newP.data, "STOP") == 0)
        {
            break;
        }
    }

    // recieving ACK for the same from SS
    int newACK;
    recv(storageSockets[SS_for_OP_paste - 1][0], &newACK, sizeof(newACK), 0);
    if (newACK == 0)
    {
        // sending ACK to Client for the operation
        newACK = 1;
        if (clientNum > -1)
            send(clientSockets[clientNum - 1][0], &newACK, sizeof(newACK), 0);

        // adding path to trie
        // insert(ROOT, finalPath, SS_for_OP_paste);
    }
    else if (newACK == -1)
    {
        // sending ACK to Client for the operation
        // recieving error
        Packet err_pack;
        recv(storageSockets[SS_for_OP_paste - 1][0], &err_pack, sizeof(err_pack), 0);
        if (clientNum > -1)
        {
            send(clientSockets[clientNum - 1][0], &newACK, sizeof(newACK), 0);
            send(clientSockets[clientNum - 1][0], &err_pack, sizeof(err_pack), 0);
        }
    }
}
/*






























*/

void NS_COPY(Packet operation, int clientNum, TrieNode *ROOT)
{
    char *token;
    char path[1024];
    int SS_for_OP_copy, SS_for_OP_paste;

    // copying somewhere
    char temp[1024];
    char opCOPY[1024], firstWord[1024], secondWord[1024];
    strcpy(temp, operation.data);

    token = strtok(temp, " \n");
    if (token != NULL)
    {
        strcpy(firstWord, token);
        token = strtok(NULL, " \n");
        if (token != NULL)
        {
            strcpy(secondWord, token);
            // concatenation
            strcpy(opCOPY, firstWord);
            strcat(opCOPY, " ");
            strcat(opCOPY, secondWord);

            // getting the path
            strcpy(path, token);

            // getting path where it is to be pasted
            char opPASTE[1024], thirdWord[1024], fourthWord[1024];
            token = strtok(NULL, " \n");
            strcpy(thirdWord, token);
            token = strtok(NULL, " \n");
            strcpy(fourthWord, token);
            strcpy(opPASTE, thirdWord);
            strcat(opPASTE, " ");
            strcat(opPASTE, fourthWord);

            // checking if the path exists
            SS_for_OP_copy = search(MOUNTED_ROOT, path);
            SS_for_OP_paste = search(ROOT, fourthWord);

            // checking
            if (SS_for_OP_copy == -1 || SS_for_OP_paste == -1)
            {
                // path does not exist
                if (clientNum > -1)
                {
                    int ACKBIT = -1;
                    send(clientSockets[clientNum - 1][0], &ACKBIT, sizeof(ACKBIT), 0);

                    // sending error message also
                    Packet errorMsg;
                    strcpy(errorMsg.data, "One of the given paths does not exist!");
                    send(clientSockets[clientNum - 1][0], &errorMsg, sizeof(errorMsg), 0);
                }
                return;
            }
            else if (SS_for_OP_copy == SS_for_OP_paste)
            {
                // self copy case
                int ACKBIT = 1;
                if (clientNum > -1)
                    send(clientSockets[clientNum - 1][0], &ACKBIT, sizeof(ACKBIT), 0);
                Packet p;
                snprintf(p.data, sizeof(p.data), "SCOPY %s PASTE %s", path, fourthWord);
                send(storageSockets[SS_for_OP_copy - 1][0], &p, sizeof(p), 0);
                int newACK;
                recv(storageSockets[SS_for_OP_paste - 1][0], &newACK, sizeof(newACK), 0);
                if (newACK == 0)
                {
                    // sending ACK to Client for the operation
                    newACK = 1;
                    if (clientNum > -1)
                        send(clientSockets[clientNum - 1][0], &newACK, sizeof(newACK), 0);
                    char finalPath[1024];
                    strcpy(finalPath, fourthWord);
                    strcat(finalPath, p.data);
                    // adding path to trie
                    insert(ROOT, finalPath, SS_for_OP_paste);
                }
                else if (newACK == -1)
                {
                    // recieving error
                    Packet err_pack;
                    recv(storageSockets[SS_for_OP_paste - 1][0], &err_pack, sizeof(err_pack), 0);
                    if (clientNum > -1)
                    {
                        // sending ACK to Client for the operation
                        send(clientSockets[clientNum - 1][0], &newACK, sizeof(newACK), 0);
                        send(clientSockets[clientNum - 1][0], &err_pack, sizeof(err_pack), 0);
                    }
                }
            }
            else
            {
                // path exists
                int ACKBIT = 1;
                if (clientNum > -1)
                {
                    send(clientSockets[clientNum - 1][0], &ACKBIT, sizeof(ACKBIT), 0);
                }

                // sending operation to SS
                // checking if to-be-copied path is a file or folder
                if (path[strlen(path) - 1] == '/')
                {
                    Packet p;
                    strcpy(p.data, opCOPY);
                    send(storageSockets[SS_for_OP_copy - 1][0], &p, sizeof(p), 0);
                    NS_COPY_FOLDER(SS_for_OP_paste, SS_for_OP_copy, clientNum, fourthWord, ROOT);
                    char copy[1024];
                    int i;
                    for (i = strlen(path) - 2; path[i] != '/' && i >= 0; i--)
                    {
                    }
                    char extrapath[1024];
                    strcpy(extrapath, fourthWord);
                    strcat(extrapath, path + i + 1);
                    insert(ROOT, extrapath, SS_for_OP_paste);
                }
                else
                {
                    Packet p;
                    strcpy(p.data, "COPYFILE ");
                    strcat(p.data, path);
                    send(storageSockets[SS_for_OP_copy - 1][0], &p, sizeof(p), 0);
                    NS_COPY_FILE(SS_for_OP_paste, SS_for_OP_copy, clientNum, fourthWord, ROOT);
                }
            }
        }
    }
}

/*





























*/

// int CREATE_BACKUP_FOLDER(int SS_for_backup, TrieNode *ROOT)
// {
//     printf("ENTERED BACKUP FOLDER\n");

//     /****
//      * creates BACKUP folder in the given SS as well as its root of trie
//      *
//      * return of -1 means BACKUP folder already exists in the given SS
//      *
//      * return of 1 means ran normally
//      *
//      ***/

//     // copying somewhere
//     char path[1024];
//     strcpy(path, "BACKUP/");

//     // checking if the path exists
//     int temp = search(ROOT, path);
//     if (temp == -1)
//     {
//         // BACKUP does not exist so creating one
//         Packet operation;
//         strcpy(operation.data, "CREATE BACKUP/");

//         // sending operation to SS
//         int newACK;
//         operation.data_type = 0;
//         send(storageSockets[SS_for_backup - 1][0], &operation, sizeof(operation), 0);

//         // recieving ACK for the same from SS
//         recv(storageSockets[SS_for_backup - 1][0], &newACK, sizeof(newACK), 0);
//         if (newACK == 0)
//         {
//             // adding path to trie
//             insert(ROOT, path, SS_for_backup);
//         }
//         else if (newACK == -1)
//         {
//             // recieving error
//             Packet err_pack;
//             recv(storageSockets[SS_for_backup - 1][0], &err_pack, sizeof(err_pack), 0);
//             printf("%s\n", err_pack.data);
//         }
//     }
//     else
//     {
//         // BACKUP exists already
//         return -1;
//     }
// }

/*




























*/

// void ADD_PATH_TO_BACKUP(char *path)
// {
//     // pinging SS 1
//     // Packet PingPack;
//     // PingPack.data_type = PING_PACKET;
//     // char extrapath[1024];
//     // int status = send(storageSockets[0][0], &PingPack, sizeof(PingPack), 0);
//     // if (status != 0 && status != -1)
//     // {
//     //     status = recv(storageSockets[0][0], &PingPack, sizeof(PingPack), 0);
//     //     if (status != 0 && status != -1)
//     //     {
//     //         // connection alive
//     //         Packet backupOP;
//     //         strcpy(backupOP.data, "COPY ");
//     //         strcat(backupOP.data, path);
//     //         strcat(backupOP.data, " PASTE BACKUP/");

//     //         strcpy(extrapath, "BACKUP/");
//     //         if(path[strlen(path)-1]!='/')
//     //           strcat(extrapath,strrchr(path,'/')+1);
//     //         else
//     //         {
//     //             int i;
//     //             for(i=strlen(path)-2;path[i]!='/' && i>=0;i--)
//     //             {
//     //             }
//     //             strcat(extrapath,path+i+1);
//     //         }
//     //         insert(FIRST_BACKUP, extrapath, 1);
//     //         //NS_COPY(backupOP, -1, FIRST_BACKUP);
//     //         printf("First backup done\n");
//     //     }
//     // }

//     // // pinging SS 2
//     // status = send(storageSockets[1][0], &PingPack, sizeof(PingPack), 0);
//     // if (status != 0 && status != -1)
//     // {
//     //     // connection alive
//     //     status = recv(storageSockets[1][0], &PingPack, sizeof(PingPack), 0);
//     //     if (status != 0 && status != -1)
//     //     {
//     //         Packet backupOP;
//     //         strcpy(backupOP.data, "COPY ");
//     //         strcat(backupOP.data, path);
//     //         strcat(backupOP.data, " PASTE BACKUP/");
//     //         strcpy(extrapath, "BACKUP/");
//     //         strcat(extrapath, path);
//     //         insert(SECOND_BACKUP, extrapath, 2);
//     //         //NS_COPY(backupOP, -1, SECOND_BACKUP);
//     //         printf("Second backup done\n");
//     //     }
//     // }
//     return;
// }

/*




























*/

// clientRoutine function
void *clientRoutine(void *arg)
{
    // type conversion
    int client_no = (*(int *)arg) + 1;

    // looping
    while (1)
    {
        // receving message
        Packet msg;
        int status = recv(clientSockets[client_no - 1][0], &msg, sizeof(msg), 0);
        if (status == -1)
        {
            perror("Error in receiving");
            close(clientSockets[client_no - 1][0]);
            clientSockets[client_no - 1][1] = 0;
            currClients--;
            return NULL;
        }
        else if (status == 0)
        {
            printf("Connection has been closed by Client%d!\n", client_no);
            close(clientSockets[client_no - 1][0]);
            clientSockets[client_no - 1][1] = 0;
            currClients--;
            return NULL;
        }
        else
        {
            printf("Client%d : %s", client_no, msg.data);
        }

        // checking which command is given by Client
        if (strstr(msg.data, READ))
        {
            NS_READ(msg, client_no);
        }
        else if (strstr(msg.data, WRITE))
        {
            NS_WRITE(msg, client_no);
        }
        else if (strstr(msg.data, GET_INFO))
        {
            NS_RETRIEVE(msg, client_no);
        }
        else if (strstr(msg.data, COPY) && strstr(msg.data, PASTE))
        {
            // copy & paste
            NS_COPY(msg, client_no, MOUNTED_ROOT);
        }
        else if (strstr(msg.data, CREATE))
        {
            NS_CREATE(msg, client_no);
        }
        else if (strstr(msg.data, DELETE))
        {
            NS_DELETE(msg, client_no);
        }
        else
        {
            // error in given commands
            printf("Could not recognise the command given!\n");
        }
    }

    return NULL;
}

/*

























*/

int pingSS(int ss_no)
{
    Packet pingPacket;
    Packet temp_ping;
    strcpy(pingPacket.data, "PING");
    pingPacket.data_type = PING_PACKET;
    while (1)
    {
        int sendstat = 0;
        sleep(PING_INTERVAL);
        sendstat = send(storageSockets[ss_no - 1][0], &pingPacket, sizeof(pingPacket), 0);
        if (sendstat == -1)
        {
            return 1;
        }
        else if (sendstat == 0)
        {
            printf("Storage server%d closed connection\n", ss_no);
            close(storageSockets[ss_no - 1][0]);
        }
        else
        {
            int status = recv(storageSockets[ss_no - 1][0], &temp_ping, sizeof(temp_ping), 0);
            if (status == -1)
            {
                perror("Error in sending");
                close(storageSockets[ss_no - 1][0]);
            }
            else if (status == 0)
            {
                perror("Naming Server closed the connection\n");
                close(storageSockets[ss_no - 1][0]);
            }
        }
    }
}

/*


























*/

void *storageRoutine(void *arg)
{
    // type conversion
    int ss_no = (*(int *)arg) + 1;

    // input about SS info
    Packet msg;

    // getting NS port
    int msgINT;
    int status = recv(storageSockets[ss_no - 1][0], &msgINT, sizeof(msgINT), 0);
    if (status == -1)
    {
        perror("Error in sending");
        close(storageSockets[ss_no - 1][0]);
    }
    else if (status == 0)
    {
        perror("Naming Server closed the connection\n");
        close(storageSockets[ss_no - 1][0]);
    }
    allSS[ss_no - 1].NSPort = msgINT;
    printf("SS%d has %d as the port for NS\n", ss_no, allSS[ss_no - 1].NSPort);

    // getting NS port
    status = recv(storageSockets[ss_no - 1][0], &msgINT, sizeof(msgINT), 0);
    if (status == -1)
    {
        perror("Error in sending");
        close(storageSockets[ss_no - 1][0]);
    }
    else if (status == 0)
    {
        perror("Naming Server closed the connection\n");
        close(storageSockets[ss_no - 1][0]);
    }
    allSS[ss_no - 1].ClientPort = msgINT;
    printf("SS%d has %d as the port for Client\n", ss_no, allSS[ss_no - 1].ClientPort);

    // // checking where to add backup folder
    // if (ss_no == 1)
    // {
    //     // creating a BACKUP folder
    //     CREATE_BACKUP_FOLDER(currSS, FIRST_BACKUP);
    //     // insert(FIRST_BACKUP, "BACKUP/", 1);
    // }
    // else if (ss_no == 2)
    // {
    //     // creating a BACKUP folder
    //     CREATE_BACKUP_FOLDER(currSS, SECOND_BACKUP);
    //     // insert(SECOND_BACKUP, "BACKUP/", 2);
    // }

    // looping to get all the exposed paths
    while (1)
    {
        // receving message
        status = recv(storageSockets[ss_no - 1][0], &msg, sizeof(msg), 0);
        if (status == -1)
        {
            perror("Error in receiving");
            close(storageSockets[ss_no - 1][0]);
            storageSockets[ss_no - 1][1] = 0;
            currSS--;
            return NULL;
        }
        else if (status == 0)
        {
            printf("Connection has been closed by SS%d!\n", ss_no);
            close(storageSockets[ss_no - 1][0]);
            storageSockets[ss_no - 1][1] = 0;
            currSS--;
            return NULL;
        }

        // checking the message
        if (msg.data[0] == -1)
        {
            printf("All paths recieved from SS%d!\n", ss_no);
            break;
        }
        else
        {
            printf("Storage Server%d : %s\n", ss_no, msg.data);
            insert(MOUNTED_ROOT, msg.data, ss_no);

            // // also copying to BACKUPS
            // if (ss_no != 1 && ss_no != 2)
            // {
            //     ADD_PATH_TO_BACKUP(msg.data);
            // }
        }
    }

    int ping_stat = pingSS(ss_no);
    if (ping_stat == 1)
    {
        close(storageSockets[ss_no - 1][0]);
        printf("Storage Server%d has gone offline\n", ss_no);
        currSS--;
    }

    return NULL;
}

/*



























*/

int main()
{
    // creating a socket
    signal(SIGPIPE, SIG_IGN);
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock == -1)
    {
        perror("Error with socket");
    }

    // defining the server address
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;

    // binding socket to address
    serverAddress.sin_port = htons(12345);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        perror("Binding error");
        close(serverSock);
    }

    // listening for requests from clients on this socket
    if (listen(serverSock, MAX_SS + MAX_CLIENTS + 4) == -1)
    {
        perror("Error in listening");
        close(serverSock);
    }
    printf("Server is ready to receive data!\n");

    // creating array of threads for clients and storage servers
    pthread_t clientThreads[MAX_CLIENTS + 1];
    pthread_t storageThreads[MAX_SS + 1];

    // intializing global variables
    currClients = 0;
    currSS = 0;
    for (int i = 0; i <= MAX_CLIENTS; i++)
    {
        clientSockets[i][1] = 0;
    }
    for (int i = 0; i <= MAX_SS; i++)
    {
        storageSockets[i][1] = 0;
    }
    MOUNTED_ROOT = init_node();
    MOUNTED_ROOT->data = '~';
    MOUNTED_ROOT->terminal = false;
    FIRST_BACKUP = init_node();
    FIRST_BACKUP->data = '~';
    FIRST_BACKUP->terminal = false;
    SECOND_BACKUP = init_node();
    SECOND_BACKUP->data = '~';
    SECOND_BACKUP->terminal = false;
    char *prefix = calloc(1024, sizeof(char));

    // accepting connections in a loop
    int temp = -1;
    int temp_sock;
    while (1)
    {
        // accepting connection
        temp_sock = accept(serverSock, NULL, NULL);
        if (temp_sock == -1)
        {
            // checking if the error was caused by a non-blocking connection
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // waiting for connections in that case
                continue;
            }
            else
            {
                // otherwise there was some actual error
                perror("Error in accepting");
                return 0;
            }
        }
        // recieving first message
        Packet msg;
        int status = recv(temp_sock, &msg, sizeof(msg), 0);
        if (status == -1)
        {
            perror("Error in receiving");
            close(temp_sock);
            continue;
        }
        int i;
        if (strstr(msg.data, "Storage") != NULL)
        {
            // getting IP address of the SS
            Packet m;
            int status = recv(temp_sock, &m, sizeof(m), 0);
            for (i = 0; i <= MAX_SS; i++)
            {
                if (!storageSockets[i][1] || strcmp(inet_ntoa(allSS[i].myIP), m.data) == 0)
                {
                    break;
                }
            }
            storageSockets[i][0] = temp_sock;
            storageSockets[i][1] = 1;
            inet_aton(m.data, &allSS[i].myIP);
            currSS++;

            // checking connection limit
            if (currSS > MAX_SS)
            {
                currSS--;
                Packet reply;
                strcpy(reply.data, "Number of storage servers limit exceeded! Please try again after some time.");
                int status = send(storageSockets[i][0], &reply, sizeof(reply), 0);

                // closing the connection
                close(storageSockets[i][0]);
                storageSockets[i][1] = 0;

                // continuing in this case
                continue;
            }

            printf("Storage Server%d : %s", i + 1, msg.data);

            // entering new thread otherwise
            Packet reply;
            sprintf(reply.data, "%d", i + 1);
            status = send(storageSockets[i][0], &reply, sizeof(reply), 0);
            temp = i;
            if (pthread_create(&storageThreads[i], NULL, &storageRoutine, &temp) != 0)
            {
                perror("Error in creating a new thread for the storage server");
            }
        }
        else
        {
            for (i = 0; i <= MAX_CLIENTS; i++)
            {
                if (!clientSockets[i][1])
                {
                    break;
                }
            }
            clientSockets[i][0] = temp_sock;
            clientSockets[i][1] = 1;
            currClients++;

            // checking connection limit
            if (currClients > MAX_CLIENTS)
            {
                currClients--;
                Packet reply;
                strcpy(reply.data, "Number of clients limit exceeded! Please try again after some time.");
                int status = send(clientSockets[i][0], &reply, sizeof(reply), 0);

                // closing the connection
                close(clientSockets[i][0]);
                clientSockets[i][1] = 0;

                // continuing in this case
                continue;
            }

            printf("Client%d : %s", i + 1, msg.data);

            // entering new thread otherwise
            Packet reply;
            strcpy(reply.data, "SERVER : Server here!");
            status = send(clientSockets[i][0], &reply, sizeof(reply), 0);
            temp = i;
            if (pthread_create(&clientThreads[i], NULL, &clientRoutine, &temp) != 0)
            {
                perror("Error in creating a new thread for the client");
            }
        }
    }

    // joining all the threads
    for (int x = 0; x < MAX_CLIENTS; x++)
    {
        pthread_join(clientThreads[x], NULL);
    }
    for (int x = 0; x < MAX_SS; x++)
    {
        pthread_join(storageThreads[x], NULL);
    }
    // closing server socket
    if (close(serverSock) == -1)
    {
        perror("Error in closing");
    }

    return 0;
}