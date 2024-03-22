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

#ifndef MSOCKET_H
#define MSOCKET_H
#define T 5
#ifndef MTP_CONFIG_H
#define MTP_CONFIG_H

// Define MTP socket type
#define SOCK_MTP 3

// Define the maximum number of messages in the buffer
#define MAX_MESSAGES 10
#define MAX_PAYLOAD_SIZE 1024
#define MAX_SOCKETS 25
#define T_INTERVAL 5 // Value of T in seconds
#define P_DROP 0.2   // Probability of dropping a message
#define SEND_WND 5
#define RECEIVE_WND 5
#define MAX_RECEIVE_BUFF 5
#define MAX_SEND_BUFF 10

void semaphore_wait(int semid);
void semaphore_signal(int semid);

// Define the MTP header structure
typedef struct
{
    uint8_t sequence_number;
    // Add other necessary fields in the header
} MTPHeader;

// Define the MTP message structure
typedef struct
{
    MTPHeader header;
    char payload[MAX_PAYLOAD_SIZE];
} MTPMessage;

typedef struct SOCK_INFO
{
    int sock_id;
    struct sockaddr_in IP;
    int port;
    int errorno;
} SOCK_INFO;

// typedef struct {
//     int is_free; // 1 if free, 0 if allotted
//     pid_t process_id;
//     int udp_socket_id;
//     char other_end_ip[16]; // Assuming IPv4
//     uint16_t other_end_port;
//     MTPMessage send_buffer[MAX_BUFFER_SIZE];
//     MTPMessage receive_buffer[MAX_BUFFER_SIZE];
//     sem_t send_buffer_semaphore;
//     sem_t receive_buffer_semaphore;
//     // Add other necessary fields
// } MTPSocketEntry;

// Shared memory structure containing information about N MTP sockets
// typedef struct {
//     MTPSocketEntry SM[MAX_SOCKETS];
//     // Add other necessary fields
// } SharedMemory;

typedef struct send_window
{
    int size;
    char seq_nums[SEND_WND];
    int nospace;
} send_window;

typedef struct receive_window
{
    int size;
    char seq_nums[RECEIVE_WND];
    int nospace;
} receive_window;

typedef struct MTPSocketEntry
{
    int is_free;
    pid_t process_id;
    int udp_socket_id;

    char other_end_ip[16];
    uint16_t other_end_port;
    char receive_buff[MAX_RECEIVE_BUFF][MAX_PAYLOAD_SIZE];
    char send_buff[MAX_SEND_BUFF][MAX_PAYLOAD_SIZE];
    send_window swnd;
    receive_window rwnd;
} MTPSocketEntry;

// Function declarations
int m_socket(int domain, int type, int protocol);
int m_bind(int sockfd, const char *source_ip, uint16_t source_port, const char *dest_ip, uint16_t dest_port);
// int m_sendto(int sockfd, const void *buf, size_t len,const char *source_ip, uint16_t source_port, const char *dest_ip, uint16_t dest_port);
int m_sendto(int sockfd, char *buf, size_t len, const char *source_ip, uint16_t source_port, const char *dest_ip, uint16_t dest_port);
int m_recvfrom(int sockfd, void *buf, size_t len);
int initialize_semaphores();
int dropMessage(float p);
int m_close(int sockfd);

#endif
#endif
