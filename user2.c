#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/sem.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "msocket.h"

int main()
{
    int sockfd = m_socket(AF_INET, SOCK_MTP, 0);
    if (sockfd < 0)
    {
        printf("Socket creation failed\n");
        return 0;
    }
    printf("Socket FD: %d\n", sockfd);
 
    char IP[20] = "127.0.0.1";

    int SPORT = 60000;

    char DIP[20] = "127.0.0.1";

    int DPORT = 50000;

    if (m_bind(sockfd,IP,SPORT,DIP,DPORT) < 0)
    {
        printf("Unsuccessful Bind\n");
        return 0;
    }
    printf("Bind successful\n");

    char buffer[1000];
    while (1)
    {
        int n = m_recvfrom(sockfd, buffer, 1000);

        if (n != -1)
        {

            printf("Recieved:\n %s\n", buffer);
            if (strcmp(buffer, "0") == 0)
                break;
            printf("Enter message(0 to exit)\n");
            scanf("%s", buffer);
            int n = m_sendto(sockfd, buffer, strlen(buffer) + 1,IP,SPORT,DIP,DPORT);
        }
    }

    m_close(sockfd);
}
