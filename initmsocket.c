#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <errno.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include "msocket.h"

#ifdef DEBUG
#define myprintf printf
#else
#define myprintf
#endif

#define T 5
#define KEY 1234
#define KEY2 1000
#define MAX_MTP_SOCK 25
#define IP_ADDRESS_MAX_LENGTH 46
// #define SEND_WND 10
// // #define RECEIVE_WND 5
// #define MAX_RECEIVE_BUFF 5
// #define MAX_SEND_BUFF 10
#define MAX_PAYLOAD_SIZE 1024
#define SOCK_MTP 3
#define MAX_MESSAGES 10
#define KEY_SEM 2

float p = 0.75;
// typedef struct SOCK_INFO
// {
//     int sock_id;
//     struct sockaddr_in IP;
//     int port;
//     int errorno;
// } SOCK_INFO;

// typedef struct send_window
// {
//     int size;
//     char seq_nums[SEND_WND];
//     int nospace;
// } send_window;

// typedef struct receive_window
// {
//     int size;
//     char seq_nums[RECEIVE_WND];
//     int nospace;
// } receive_window;

// typedef struct MTPMessage
// {
//     char data[MAX_PAYLOAD_SIZE];
//     int seq_num;
//     int ack_num;
//     int is_last;
//     int is_ack;
//     int is_nack;
// } MTPMessage;

// typedef struct MTPSocketEntry
// {
//     int is_free;
//     pid_t process_id;
//     int udp_socket_id;
//     // int dest_port;
//     // struct sockaddr_in dest_IP;
//     char other_end_ip[16];
//     uint16_t other_end_port;
//     char receive_buff[MAX_RECEIVE_BUFF][MAX_PAYLOAD_SIZE];
//     char send_buff[MAX_SEND_BUFF][MAX_PAYLOAD_SIZE];
//     send_window swnd;
//     receive_window rwnd;
// } MTPSocketEntry;

int check_msg(char *msg)
{
    if (msg[0] == '0' && msg[1] == '0' && msg[2] == '0' && msg[3] == '0')
    {
        return 1;
    }
    else if (msg[0] == '0' && msg[1] == '0' && msg[2] == '0' && msg[3] == '1')
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

void *R_func(void *arg)
{
    myprintf("R_func running\n");

    MTPSocketEntry *SM = (MTPSocketEntry *)arg;
    fd_set readfds;
    FD_ZERO(&readfds);
    int maxfd = -1;
    for (int i = 0; i < MAX_MTP_SOCK; i++)
    {
        if (SM[i].udp_socket_id != 0)
        {
            FD_SET(SM[i].udp_socket_id, &readfds);
            if (SM[i].udp_socket_id > maxfd)
            {
                maxfd = SM[i].udp_socket_id;
            }
        }
    }
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;

    while (1)
    {
        // check if any new MTP socket created if yes update

        int activity = select(maxfd + 1, &readfds, NULL, NULL, &timeout);
        if (activity == -1)
        {
            perror("select");
        }
        else if (activity == 0)
        {
            myprintf("Timeout reached\n");

            for (int i = 0; i < MAX_MTP_SOCK; i++)
            {
                if (SM[i].udp_socket_id != 0)
                {
                    FD_SET(SM[i].udp_socket_id, &readfds);
                    if (SM[i].udp_socket_id > maxfd)
                    {
                        maxfd = SM[i].udp_socket_id;
                    }
                }

                // if available space in receive window is zero set nospace to 1
                if (strlen(SM[i].rwnd.seq_nums) >= MAX_RECEIVE_BUFF)
                {
                    SM[i].rwnd.nospace = 1;
                }
                else
                {
                    // after we know space is there we send back an ACK with the last acknowledged seq number
                    // and the rwnd size
                    char ack_msg[MAX_PAYLOAD_SIZE];
                    sprintf(ack_msg, "%d", SM[i].rwnd.seq_nums[0]);
                    strcat(ack_msg, "0001");
                    sendto(SM[i].udp_socket_id, ack_msg, sizeof(ack_msg), 0, NULL, 0);
                }
            }
        }
        else
        {
            myprintf("Found active socket\n");
            for (int i = 0; i < MAX_MTP_SOCK; i++)
            {
                if (FD_ISSET(SM[i].udp_socket_id, &readfds))
                {

                    // store in the receiver side message buffer
                    char buffer[MAX_PAYLOAD_SIZE];
                    struct sockaddr_in client;
                    int len = sizeof(client);
                    recvfrom(SM[i].udp_socket_id, buffer, sizeof(buffer), 0, (struct sockaddr *)&client, &len);

                    if (dropMessage(p))
                    {
                        myprintf("Dropping Message\n");
                        continue;
                    }
                    else
                    {
                        myprintf("Received: %s\n", buffer);
                        // check the message type if it is a data message store in the receive buffer and send an ack back
                        if (check_msg(buffer) == 1)
                        {
                            // if msg type is data message
                            // if the seq_num is in order then store in receive buffer

                            strcpy(SM[i].receive_buff[MAX_RECEIVE_BUFF - SM[i].rwnd.size], buffer);
                            SM[i].rwnd.size--;
                            // send an ack back with the last acknowledged seq number and rwnd size
                            char ack_msg[MAX_PAYLOAD_SIZE];
                            sprintf(ack_msg, "%d", SM[i].rwnd.seq_nums[0]);
                            strcat(ack_msg, "0001");
                            sendto(SM[i].udp_socket_id, ack_msg, sizeof(ack_msg), 0, (struct sockaddr *)&client, len);
                        }
                        else if (check_msg(buffer) == 0)
                        {
                            // if msg type is ack message
                            // if the ack number is the last sent seq number then remove the message from the send buffer
                            // and update the send window
                            if (SM[i].swnd.seq_nums[0] == buffer[0])
                            {
                                SM[i].swnd.size++;
                                for (int j = 0; j < MAX_SEND_BUFF; j++)
                                {
                                    if (SM[i].swnd.seq_nums[j] == buffer[0])
                                    {
                                        SM[i].swnd.seq_nums[j] = -1;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        return NULL;
    }

    // while adding in the send buffer or while making the send to call we take care of the seq_num
}
void convert_msg(char *buffer, char *msg, int seq_num)
{
    char seq_num_str[10];
    sprintf(seq_num_str, "%d", seq_num);
    strcpy(buffer, seq_num_str);
    strcat(buffer, msg);
}

void *garbage_func(void *arg)
{
    MTPSocketEntry *SM = (MTPSocketEntry *)arg;
    while (1)
    {
        // Iterate through the MTP socket table
        for (int i = 0; i < MAX_MTP_SOCK; i++)
        {
            // Check if the socket entry is in use and its associated process is not running
            if (!SM[i].is_free && kill(SM[i].process_id, 0) == -1)
            {

                SM[i].is_free = 1;
                SM[i].process_id = 0;
                SM[i].udp_socket_id = 0;
                SM[i].other_end_ip[0] = '\0';
                SM[i].other_end_port = 0;
                memset(SM[i].receive_buff, 0, sizeof(SM[i].receive_buff));
                memset(SM[i].send_buff, 0, sizeof(SM[i].send_buff));
            }
        }

        // Sleep for a specific interval before performing the next cleanup
        sleep(6); // Sleep for 6 seconds (adjust as needed)
    }

    return NULL;
}

void *S_func(void *arg)
{
    myprintf("S_func running\n");
    time_t last_sent;
    MTPSocketEntry *SM = (MTPSocketEntry *)arg;
    while (1)
    {
        sleep(T / 2);
        // get current time and store it in a variable
        time_t current_time;
        time(&current_time);
        if (current_time - last_sent >= T)
        {
            // send the message
            myprintf("Sending message\n");
            for (int i = 0; i < MAX_MTP_SOCK; i++)
            {
                // if there is pending message in the send buffer sending it using sendto function

                for (int j = 0; j < MAX_SEND_BUFF; j++)
                {
                    // create mutex for when using SM
                    // we add the message to the send buffer and then send it using sendto
                    if (strlen(SM[i].send_buff[SM[i].swnd.seq_nums[j] % MAX_SEND_BUFF]) > 0 && SM[i].swnd.size > 0)
                    {
                        // convert the message in send_buff[j] to have the sequence number
                        char buffer[MAX_PAYLOAD_SIZE];
                        convert_msg(buffer, SM[i].send_buff[j], SM[i].swnd.seq_nums[j]);
                        sendto(SM[i].udp_socket_id, buffer, sizeof(buffer), 0, NULL, 0);
                        SM[i].swnd.size--;
                    }
                }
            }
            time(&last_sent);
        }
    }
    return NULL;
}

int main()
{

    printf("init working\n");
    int semid1, semid2;
    key_t key1 = ftok(".", 1);
    key_t key2 = ftok(".", 2);
    key_t key3 = ftok(".", 3);
    key_t key4 = ftok(".", 4);
    key_t key5 = ftok(".", 5);

    semid1 = semget(key3, 1, 0777 | IPC_CREAT);
    semid2 = semget(key4, 1, 0777 | IPC_CREAT);

    semctl(semid1, 0, SETVAL, 0);
    semctl(semid2, 0, SETVAL, 0);

    // struct sembuf vop, pop;

    // vop.sem_num = 0;
    // vop.sem_op = 1;
    // vop.sem_flg = 0;

    // pop.sem_num = 0;
    // pop.sem_op = -1;
    // pop.sem_flg = 0;

    int shmid1, shmid2;

    SOCK_INFO *sockinfo;
    MTPSocketEntry *SM;

    shmid1 = shmget(key5, sizeof(int), 0777 | IPC_CREAT);

    if (shmid1 == -1)
    {
        printf("Unable to create shared mem\n");
    }

    int *mtp_errno = (int *)shmat(shmid1, 0, 0);

    if (mtp_errno == (void *)-1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    shmid2 = shmget(key1, sizeof(SOCK_INFO), IPC_CREAT | 0666);

    if (shmid2 == -1)
    {
        printf("Unable to create shared mem\n");
    }

    sockinfo = (SOCK_INFO *)shmat(shmid2, NULL, 0);

    if (sockinfo == (void *)-1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // sockinfo->errorno = 0;
    // sockinfo->port = 0;
    // sockinfo->sock_id = 0;
    // sockinfo->IP.sin_addr.s_addr = INADDR_ANY;
    int SM_id;
    SM_id = shmget(key2, MAX_MTP_SOCK * sizeof(MTPSocketEntry), IPC_CREAT | 0666);

    if (SM_id == -1)
    {
        printf("Unable to create shared mem\n");
    }

    SM = (MTPSocketEntry *)shmat(SM_id, NULL, 0);
    if (SM == (void *)-1)
    {
        perror("shmat not working");
        exit(EXIT_FAILURE);
    }
    // SM[24].arr[24] = 100;
    // printf("output: %d\n", SM[24].arr[24]);

    for (int i = 0; i < MAX_MTP_SOCK; ++i)
    { // initialize socketEntry to 1 and rest to 0
        MTPSocketEntry *socketEntry = &SM[i];

        socketEntry->is_free = 1;
        socketEntry->process_id = -1;    // Assuming -1 indicates an uninitialized process ID
        socketEntry->udp_socket_id = -1; // Assuming -1 indicates an uninitialized UDP socket ID

        // memset(socketEntry->other_end_ip, '\0', sizeof(socketEntry->other_end_ip));
        socketEntry->other_end_port = 0;

        // Initialize send and receive buffers
        // memset(socketEntry->send_buffer, '\0', sizeof(socketEntry->send_buffer));

        // memset(socketEntry->receive_buffer, '\0', sizeof(socketEntry->receive_buffer));

        for (int j = 0; j < MAX_RECEIVE_BUFF; ++j)
        {
            for (int k = 0; k < MAX_PAYLOAD_SIZE; ++k)
            {
                SM[i].receive_buff[j][k] = '\0';
            }
        }

        for (int j = 0; j < MAX_SEND_BUFF; ++j)
        {
            for (int k = 0; k < MAX_PAYLOAD_SIZE; ++k)
            {
                SM[i].send_buff[j][k] = '\0';
            }
        }
    }

    printf("init running\n%d\n%d\n", semid1, semid2);

    pthread_t thread_R, thread_S, thread_garbage;
    pthread_attr_t R_attr, S_attr, garbage_attr;

    pthread_attr_init(&R_attr);
    pthread_attr_init(&S_attr);
    pthread_attr_init(&garbage_attr);
    pthread_attr_setdetachstate(&R_attr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setdetachstate(&S_attr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setdetachstate(&garbage_attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&thread_R, &R_attr, R_func, (void *)SM);
    pthread_create(&thread_S, &S_attr, S_func, (void *)SM);
    pthread_create(&thread_garbage, &garbage_attr, garbage_func, (void *)SM);
    // thread creation done

    while (1)
    {
        // inet_pton(AF_INET, "127.0.0.1", &sockinfo->IP.sin_addr.s_addr);
        sockinfo->IP.sin_addr.s_addr == INADDR_ANY;
        sockinfo->sock_id = 0;
        sockinfo->port = 0;
        sockinfo->errorno == 0;
        printf("waiting\n");
        semaphore_wait(semid1);
        // printf("semaphore worked\n");
        if (sockinfo->errorno == 0 && sockinfo->port == 0 && sockinfo->sock_id == 0 && sockinfo->IP.sin_addr.s_addr == INADDR_ANY)
        {

            // char ip_str[INET_ADDRSTRLEN];
            // inet_ntop(AF_INET, &sockinfo->IP.sin_addr.s_addr, ip_str, INET_ADDRSTRLEN);
            printf("It is a UDP socket call\n");
            // printf("%d", sockinfo->IP.sin_addr.s_addr);
            // printf("%s", ip_str);
            int temp_sockid;

            if ((temp_sockid = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
            {
                printf("error creating socket\n");
                sockinfo->sock_id = -1;
                sockinfo->errorno = errno;
            }
            printf("Socket created\n");
            sockinfo->sock_id = temp_sockid;
            semaphore_signal(semid2);
        }
        else if (sockinfo->sock_id != 0 && sockinfo->port != 0) //&& sockinfo->IP.sin_addr.s_addr != INADDR_ANY
        {
            printf("It is a bind call\n");
            // printf("port: %d\n", sockinfo->port);
            struct sockaddr_in socket;
            socket.sin_family = AF_INET;
            socket.sin_addr.s_addr = sockinfo->IP.sin_addr.s_addr;
            socket.sin_port = htons(sockinfo->port);
            printf("Binding\n");
            if ((bind(sockinfo->sock_id, (struct sockaddr *)&socket, sizeof(socket))) < 0)
            {
                printf("Error in binding\n");
                sockinfo->sock_id = -1;
                sockinfo->errorno = errno;
            }
            semaphore_signal(semid2);
        }
    }

    pthread_join(thread_R, NULL);
    pthread_join(thread_S, NULL);
    pthread_join(thread_garbage, NULL);

    pthread_attr_destroy(&R_attr);
    pthread_attr_destroy(&S_attr);
    pthread_attr_destroy(&garbage_attr);
    shmdt(sockinfo);
    shmdt(SM);
    shmdt(mtp_errno);
    shmctl(shmid1, IPC_RMID, 0);
    shmctl(shmid2, IPC_RMID, 0);
    shmctl(SM_id, IPC_RMID, 0);

    return 0;
}