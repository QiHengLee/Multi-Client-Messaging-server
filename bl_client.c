#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include "blather.h"
#include <pthread.h>

pthread_t user_thread;
pthread_t server_thread;

simpio_t simpio_actual;
simpio_t *simpio = &simpio_actual;

char client_name[MAXPATH];
char to_client_fifo[MAXPATH];
char to_server_fifo[MAXPATH];

void *user_worker(void *arg) {
    int to_server_fifo_fd = open(to_server_fifo, O_RDWR);

    // keep waiting for input from client until reach end of input
    while(!simpio->end_of_input) {
        simpio_reset(simpio);
        iprintf(simpio, "");                                          // print prompt
        while(!simpio->line_ready && !simpio->end_of_input){          // read until line is complete
            simpio_get_char(simpio);
        }
        // if client input done then retrieve the message and send it server
        if(simpio->line_ready){
            mesg_t new_message = {};
            new_message.kind = BL_MESG;
            strcpy(new_message.name, client_name);
            strcpy(new_message.body, simpio->buf);
            write(to_server_fifo_fd, &new_message, sizeof(mesg_t));
        }
    }

    // handle depart message when end of input reached
    mesg_t new_message = {};
    new_message.kind = BL_DEPARTED;
    strcpy(new_message.name, client_name);
    strcpy(new_message.body, "");
    int nwrite = write(to_server_fifo_fd, &new_message, sizeof(mesg_t));

    // cancle thread for server worker
    pthread_cancel(server_thread);
    return NULL;
}

void *server_worker(void *arg) {
    int to_client_fd = open(to_client_fifo, O_RDWR);
    
    // loop indefinetely until user worker stops this thread or shutdown message received
    while(1) {
        mesg_t server_message = {};
        int nread = read(to_client_fd, &server_message, sizeof(mesg_t));
        if(nread != 0) {
            // handle shutdown and stop the loop
            if(server_message.kind == BL_SHUTDOWN) {
                iprintf(simpio, "!!! server is shutting down !!!\n", server_message.name);
                break;
            }
            // handle message
            else if(server_message.kind == BL_MESG) {
                iprintf(simpio, "[%s] : %s\n", server_message.name, server_message.body);
            }
            // handle join message
            else if(server_message.kind == BL_JOINED) {
                iprintf(simpio, "-- %s JOINED --\n", server_message.name);
            }
            // handle depart message
            else if(server_message.kind == BL_DEPARTED) {
                iprintf(simpio, "-- %s DEPARTED --\n", server_message.name);
            }
        }
    }
    
    // cancel thread for user worker
    pthread_cancel(user_thread);
    return NULL;
}

int main(int argc, char *argv[]) {
    if(argc < 3) {
        printf("usage: %s <server_name> <client_name>", argv[0]);
        exit(0);
    }
    // retrieve servername and client name from user
    char server_name[MAXPATH];
    strcpy(server_name, argv[1]);
    strcpy(client_name, argv[2]);
    char server_name_fifo[MAXPATH];
    snprintf(server_name_fifo, MAXPATH, "%s.fifo", server_name);
    
    // provide name for to client and to server fifo
    pid_t pid = getpid();
    char to_client_fname[MAXPATH];
    char to_server_fname[MAXPATH];
    snprintf(to_client_fname, MAXPATH, "%d_to_client", pid);
    snprintf(to_server_fname, MAXPATH, "%d_to_server", pid);
    snprintf(to_server_fifo, MAXPATH, "%s.fifo", to_server_fname);
    snprintf(to_client_fifo, MAXPATH, "%s.fifo", to_client_fname);

    // remove existing fifo and make fifo for to client and to server
    remove(to_server_fifo);
    remove(to_client_fifo);
    mkfifo(to_server_fifo, S_IRUSR | S_IWUSR);
    mkfifo(to_client_fifo, S_IRUSR | S_IWUSR);

    // compose join message
    join_t join_message = {};
    strcpy(join_message.name, client_name);
    strcpy(join_message.to_client_fname, to_client_fname);
    strcpy(join_message.to_server_fname, to_server_fname);

    // send join message to server
    int server_fifo_fd = open(server_name_fifo, O_RDWR);
    write(server_fifo_fd, &join_message, sizeof(join_t));

    // set up simple terminal io
    char prompt[MAXNAME];
    snprintf(prompt, MAXNAME, "%s>> ", client_name);
    simpio_set_prompt(simpio, prompt);         // set the prompt
    simpio_reset(simpio);                      // initialize io
    simpio_noncanonical_terminal_mode();       // set the terminal into a compatible mode

    // create 2 threads to run user and server worker
    pthread_create(&user_thread, NULL, user_worker, NULL);
    pthread_create(&server_thread, NULL, server_worker, NULL);

    // gather the threads
    pthread_join(user_thread, NULL);
    pthread_join(server_thread, NULL);
    
    simpio_reset_terminal_mode();
    printf("\n");                 
    return 0;
}
