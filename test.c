#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include "blather.h"
#include <pthread.h>

int main(int argc, char *argv[]) {
    char server_name[MAXPATH] = "0000.fifo";
    int server_fd = open(server_name, O_RDWR);
    char client_name[MAXPATH];
    strcpy(client_name, argv[1]);
    
    pid_t pid = getpid();
    char to_client[MAXPATH];
    char to_server[MAXPATH];
    char to_client_fifo[MAXPATH];
    char to_server_fifo[MAXPATH];
    snprintf(to_client, MAXPATH, "%d_to_client", pid);
    snprintf(to_server, MAXPATH, "%d_to_server", pid);
    snprintf(to_client_fifo, MAXPATH, "%s.fifo", to_client);
    snprintf(to_server_fifo, MAXPATH, "%s.fifo", to_server);
    remove(to_server_fifo);
    remove(to_client_fifo);
    mkfifo(to_client_fifo, S_IRUSR | S_IWUSR);
    mkfifo(to_server_fifo, S_IRUSR | S_IWUSR);
    
    

    join_t message;
    strcpy(message.name, argv[1]);
    strcpy(message.to_client_fname, to_client);
    strcpy(message.to_server_fname, to_server);
    

    write(server_fd, &message, sizeof(join_t));

    int to_clientfd = open(to_client_fifo, O_RDWR);
    int to_serverfd = open(to_server_fifo, O_RDWR);

    mesg_t new_message;


    int nread = read(to_clientfd, &new_message, sizeof(mesg_t));
    
    
    if(nread) {
        printf("%s\n", new_message.name);
    }
    else{
        printf("Fail");
    }
    
    mesg_t new_message2;
    new_message2.kind = BL_MESG;
    strcpy(new_message2.name, argv[1]);
    strcpy(new_message2.body, "send message");
    
    write(to_serverfd, &new_message2, sizeof(mesg_t));
    nread = read(to_clientfd, &new_message, sizeof(mesg_t));
    if(nread) {
        printf("%s\n", new_message.body);
    }
    else{
        printf("Fail");
    }

    
    return 0;
}