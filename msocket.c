#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "msocket.h"

#define MAX_PAYLOAD_SIZE 1024
#define MAX_SOCKETS 25
// #define MAX_SEND_BUFF 10
#define MAX_RECV_BUFF 5
#define MAX_MTP_SOCK 25

// float p = 0.75;

int semid1, semid2;

SOCK_INFO *sockinfo;
MTPSocketEntry *SM;
int *mtp_errno;

// int m_socket(int domain, int type, int protocol);
// int m_bind(int sockfd, const char *source_ip, uint16_t source_port, const char *dest_ip, uint16_t dest_port);
// int m_sendto(int sockfd, char *buf, size_t len,const char *source_ip, uint16_t source_port, const char *dest_ip, uint16_t dest_port);
// int m_recvfrom(int sockfd, void *buf, size_t len);
// int initialize_semaphores();
// int dropMessage(float p);
// int m_close(int sockfd);

void semaphore_wait(int semid)
{
    struct sembuf pop;
    pop.sem_num = 0;
    pop.sem_op = -1;
    pop.sem_flg = 0;
    semop(semid, &pop, 1);
}

void semaphore_signal(int semid)
{
    struct sembuf vop;
    vop.sem_num = 0;
    vop.sem_op = 1;
    vop.sem_flg = 0;
    semop(semid, &vop, 1);
}

int initialize_semaphores()
{

    key_t key1 = ftok(".", 1);
    key_t key2 = ftok(".", 2);
    key_t key3 = ftok(".", 3);
    key_t key4 = ftok(".", 4);
    key_t key5 = ftok(".", 5);

    semid1 = semget(key3, 1, 0777 | IPC_CREAT);
    semid2 = semget(key4, 1, 0777 | IPC_CREAT);

    semctl(semid1, 0, SETVAL, 0);
    semctl(semid2, 0, SETVAL, 0);

    int shmid1, shmid2;

    shmid1 = shmget(key5, sizeof(int), 0777 | IPC_CREAT);

    if (shmid1 == -1)
    {
        printf("Unable to create shared mem\n");
    }

    mtp_errno = (int *)shmat(shmid1, 0, 0);

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

    return 0;
}

int m_socket(int domain, int type, int protocol)
{
    // Check if socket type is SOCK_MTP

    if (type != SOCK_MTP)
    {
        *mtp_errno = EINVAL; // Invalid argument
        return -1;
    }

    printf("inside msocket\n");
    // Find a free entry in SM
    int free_index = -1;
    for (int i = 0; i < MAX_SOCKETS; ++i)
    {

        printf("%d\n", SM[i].is_free);
        if (SM[i].is_free == 1)
        {
            free_index = i;
            break;
        }
    }

    printf("%d\n", free_index);

    // If no free entry is available
    if (free_index == -1)
    {
        *mtp_errno = ENOBUFS; // No buffer space available
        return -1;
    }

    // // Create UDP socket
    // int udp_socket_id = socket(domain, SOCK_DGRAM, protocol);
    // if (udp_socket_id == -1) {
    //     mtp_errno = errno; // Set errno to the corresponding error
    //     return -1;
    // }

    semaphore_signal(semid1);
    printf("semaphore working\n");
    semaphore_wait(semid2);

    if (sockinfo->sock_id == -1)
    {
        *mtp_errno = sockinfo->errorno;
        sockinfo->sock_id = 0;
        sockinfo->IP.sin_addr.s_addr == INADDR_ANY;
        sockinfo->port = 0;
        sockinfo->errorno = 0;
        return sockinfo->errorno;
    }

    // Initialize SM entry with corresponding values
    SM[free_index].is_free = 0;
    SM[free_index].udp_socket_id = sockinfo->sock_id;
    printf("udp socketid: %d\n", sockinfo->sock_id);
    SM[free_index].process_id = getpid();
    // Initialize other fields as needed
    sockinfo->sock_id = 0;
    sockinfo->IP.sin_addr.s_addr == INADDR_ANY;
    sockinfo->port = 0;
    sockinfo->errorno = 0;

    return free_index; // Return the index of the allocated MTP socket in SM
}

int m_bind(int sockfd, const char *source_ip, uint16_t source_port, const char *dest_ip, uint16_t dest_port)
{
    // Check if sockfd is valid
    if (sockfd < 0 || sockfd >= MAX_SOCKETS)
    {
        errno = EINVAL; // Invalid argument
        return -1;
    }

    // Bind the UDP socket
    inet_pton(AF_INET, source_ip, &sockinfo->IP.sin_addr.s_addr);
    sockinfo->sock_id = SM[sockfd].udp_socket_id;
    sockinfo->port = source_port;

    semaphore_signal(semid1);
    semaphore_wait(semid2);

    if (sockinfo->sock_id == -1)
    {
        sockinfo->errorno = errno; // Set errno to the corresponding error
        sockinfo->sock_id = 0;
        sockinfo->IP.sin_addr.s_addr == INADDR_ANY;
        sockinfo->port = 0;
        sockinfo->errorno = 0;
        return sockinfo->errorno;
    }

    sockinfo->sock_id = 0;
    sockinfo->IP.sin_addr.s_addr == INADDR_ANY;
    sockinfo->port = 0;
    sockinfo->errorno = 0;

    // Update SOCK_INFO with destination IP and port
    strcpy(SM[sockfd].other_end_ip, dest_ip);
    SM[sockfd].other_end_port = dest_port;

    return 1;
}

int m_sendto(int sockfd, char *buf, size_t len, const char *source_ip, uint16_t source_port, const char *dest_ip, uint16_t dest_port)
{

    if (strcmp(SM[sockfd].other_end_ip, dest_ip) != 0 || SM[sockfd].other_end_port != dest_port)
    {
        *mtp_errno = ENOTCONN; // Set global error variable to ENOTBOUND
        return -1;             //***drop message
    }

    int buffer_index = -1;
    int i;
    for (i = 0; i < MAX_SEND_BUFF; i++)
    {
        if (SM[sockfd].send_buff[i][0] == '\0')
        {
            buffer_index = i;
            break;
        }
    }

    // If no space available in send buffer
    if (buffer_index == -1)
    {
        errno = ENOBUFS;
        return -1;
    }

    strcpy(SM[sockfd].send_buff[i], buf);
    return len;
}

int m_recvfrom(int sockfd, void *buf, size_t len)
{

    int buffer_index = -1;
    int i;
    for (i = 0; i < MAX_RECV_BUFF; i++)
    {
        if (SM[sockfd].receive_buff[i][0] != '\0')
        {
            buffer_index = i;
        }
    }

    // If no space available in send buffer
    if (buffer_index == -1)
    {
        errno = ENOMSG;
        return -1;
    }

    strcpy(buf, SM[sockfd].receive_buff[i]);

    for (int k = 0; k < MAX_PAYLOAD_SIZE; ++k)
    {
        SM[i].receive_buff[i][k] = '\0';
    }

    return sizeof(buf); // Return the number of bytes received
}

int m_close(int sockfd)
{
    // Close the UDP socket

    if (close(SM[sockfd].udp_socket_id) == -1)
        return -1;

    int i;
    for (i = 0; i < MAX_MTP_SOCK; i++)
    {
        if (SM[i].udp_socket_id == sockfd && !SM[i].is_free)
        {
            // Mark the entry as free
            SM[i].is_free = 1;
            // Reset all fields of the MTPSocketEntry structure
            SM[i].process_id = 0;
            SM[i].udp_socket_id = 0;
            SM[i].other_end_ip[0] = '\0';
            SM[i].other_end_port = 0;
            memset(SM[i].receive_buff, 0, sizeof(SM[i].receive_buff));
            memset(SM[i].send_buff, 0, sizeof(SM[i].send_buff));
            // Reset send_window and receive_window structures if needed
            // Exit the loop once the entry is found and cleaned up
            break;
        }
    }
    return 0; // Return success
}

int dropMessage(float p)
{
    float rand_num = (float)rand() / RAND_MAX; // Generate a random number between 0 and 1
    if (rand_num < p)
    {
        return 1; // Message dropped
    }
    else
    {
        return 0; // Message not dropped
    }
}
