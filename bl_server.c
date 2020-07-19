#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include "blather.h"

int exit_signal = 1;
// handle SIGINT helper
void handle_SIGINT(int sig_num) {
    exit_signal = 0;
}
// handle SIGTERM helper
void handle_SIGTERM(int sig_num) {
    exit_signal = 0;
}

int main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("usage: %s <server_name>\n", argv[0]);
        exit(0);
    }

    char server_name[MAXPATH];
    strcpy(server_name, argv[1]);
    server_t server = {};
    // start server with server name from user
    server_start(&server, server_name, -1);

    struct sigaction my_sa = {};               // portable signal handling setup with sigaction()

    my_sa.sa_handler = handle_SIGTERM;         // run function handle_SIGTERM
    sigaction(SIGTERM, &my_sa, NULL);          // register SIGTERM with given action

    my_sa.sa_handler = handle_SIGINT;          // run function handle_SIGINT
    sigaction(SIGINT,  &my_sa, NULL);          // register SIGINT with given action

    // exit signal is always 1 and signal handler will set it to 0 for graceful exit
    while(exit_signal) {
        // check all the sources
        server_check_sources(&server);
        
        int join_ready = server.join_ready;
        // handle join ready 
        if(join_ready) {
            server_handle_join(&server);
        }

        // handle data ready cleint
        int n_clients = server.n_clients;
        for(int i = 0; i < n_clients; i++) {
            if(server.client[i].data_ready) {
                server_handle_client(&server, i);
            }
        }
        
    }
    // server shut down and broadcast shut down message for graceful exit
    server_shutdown(&server);
    return 0;
}
