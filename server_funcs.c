#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include "blather.h"

client_t *server_get_client(server_t *server, int idx) {
    client_t* client = &(server->client[idx]);
    return client;
}
// Gets a pointer to the client_t struct at the given index. If the
// index is beyond n_clients, the behavior of the function is
// unspecified and may cause a program crash.

void server_start(server_t *server, char* server_name, int perms) {
    // get fifo name
    char fifo_name[MAXPATH];
    snprintf(fifo_name, MAXPATH, "%s.fifo", server_name);
    
    // remove existing fifo and make new fifo
    remove(fifo_name);
    mkfifo(fifo_name, S_IRUSR | S_IWUSR);
    printf("Server created %s\n", fifo_name);

    // open fifo and intiialize server fields
    int server_fifo_fd = open(fifo_name, O_RDWR);
    strcpy(server->server_name, server_name);
    server->join_fd = server_fifo_fd;
    server->join_ready = 0;
    server->n_clients = 0;
}
// Initializes and starts the server with the given name. A join fifo
// called "server_name.fifo" should be created. Removes any existing
// file of that name prior to creation. Opens the FIFO and stores its
// file descriptor in join_fd._
//
// ADVANCED: create the log file "server_name.log" and write the
// initial empty who_t contents to its beginning. Ensure that the
// log_fd is position for appending to the end of the file. Create the
// POSIX semaphore "/server_name.sem" and initialize it to 1 to
// control access to the who_t portion of the log.

void server_shutdown(server_t *server) {
    // close fifo's fd
    int ret = close(server->join_fd);
    // if close success
    if (ret == 0) {
        char server_fifo_name[MAXPATH];
        snprintf(server_fifo_name, MAXPATH + 5, "%s.fifo", server->server_name);
        // remove created fifo
        remove(server_fifo_name);
        
        mesg_t shutdown_message = {};
        shutdown_message.kind = BL_SHUTDOWN;
        // broadcast message to all clients
        server_broadcast(server, &shutdown_message);
    }
}
// Shut down the server. Close the join FIFO and unlink (remove) it so
// that no further clients can join. Send a BL_SHUTDOWN message to all
// clients and proceed to remove all clients in any order.
//
// ADVANCED: Close the log file. Close the log semaphore and unlink
// it.

int server_add_client(server_t *server, join_t *join) {
    // check maximum clients bound
    if(server->n_clients == MAXCLIENTS) return 1;

    // initialize names
    client_t new_client = {};
    char to_client_fifo_name[MAXPATH];
    char to_server_fifo_name[MAXPATH];
    snprintf(to_client_fifo_name, MAXPATH + 5, "%s.fifo", join->to_client_fname);
    snprintf(to_server_fifo_name, MAXPATH + 5, "%s.fifo", join->to_server_fname);

    strcpy(new_client.name, join->name);
    new_client.to_client_fd = open(to_client_fifo_name, O_RDWR);
    new_client.to_server_fd = open(to_server_fifo_name, O_RDWR);
    // initialize new client
    strcpy(new_client.to_client_fname, join->to_client_fname);
    strcpy(new_client.to_server_fname, join->to_server_fname);
    new_client.data_ready = 0;

    
    // add client to server
    int n_clients = server->n_clients;
    server->client[n_clients] = new_client;
    server->n_clients = n_clients + 1;

    

    return 0;
}
// Adds a client to the server according to the parameter join which
// should have fileds such as name filed in.  The client data is
// copied into the client[] array and file descriptors are opened for
// its to-server and to-client FIFOs. Initializes the data_ready field
// for the client to 0. Returns 0 on success and non-zero if the
// server as no space for clients (n_clients == MAXCLIENTS).

int server_remove_client(server_t *server, int idx) {

    // get the client and remove its fifo 
    client_t* client = server_get_client(server, idx);
    int clientFd = client->to_client_fd;
    int serverFd = client->to_server_fd;
    close(clientFd);
    close(serverFd);
    remove(client->to_client_fname);
    remove(client->to_server_fname);

    // move the position of remining client
    int n_clients = server->n_clients;
    int i = idx;
    for (; i < n_clients - 1; i++) {
        server->client[i] = server->client[i+1];
    }
    // decrease number of client
    server->n_clients = n_clients - 1;

    return 0;
}
// Remove the given client likely due to its having departed or
// disconnected. Close fifos associated with the client and remove
// them.  Shift the remaining clients to lower indices of the client[]
// preserving their order in the array; decreases n_clients.

int server_broadcast(server_t *server, mesg_t *mesg) {
    int n_client = server->n_clients;
    // broadcast message to all clients
    for(int i = 0; i < n_client; i++) {
        int to_client_fd = server->client[i].to_client_fd;
        write(to_client_fd, mesg, sizeof(mesg_t));
    }

    return 0;
}
// Send the given message to all clients connected to the server by
// writing it to the file descriptors associated with them.
//
// ADVANCED: Log the broadcast message unless it is a PING which
// should not be written to the log.

void server_check_sources(server_t *server) {
    
    int n_client = server->n_clients;
    // setup poll struc array for join and data ready fd
    struct pollfd pfds[n_client + 1];
    pfds[0].fd = server->join_fd;
    pfds[0].events = POLLIN;
    pfds[0].revents = 0;
    
    // initalize fields for poll
    for(int i = 1; i <= n_client; i++) {
        client_t* client = server_get_client(server, i-1);
        pfds[i].fd = client->to_server_fd;
        pfds[i].events = POLLIN;
        pfds[i].revents = 0;
    }
    
    int res = poll(pfds, n_client + 1, -1);
    if (res < 0) {
        perror("poll() failed");
    }
    // check for poll for join message
    if (pfds[0].revents & POLLIN){
        server->join_ready = 1;
    }
    // check for poll for all incoming message 
    if (n_client > 0) {
        for(int i = 1; i < n_client + 1; i++) {
            if(pfds[i].revents & POLLIN) {
                client_t* client = server_get_client(server, i - 1);
                client->data_ready = 1;
            }
        }
    }

}
// Checks all sources of data for the server to determine if any are
// ready for reading. Sets the servers join_ready flag and the
// data_ready flags of each of client if data is ready for them.
// Makes use of the poll() system call to efficiently determine
// which sources are ready.

int server_join_ready(server_t *server) {
    return server->join_ready;
}
// Return the join_ready flag from the server which indicates whether
// a call to server_handle_join() is safe.

int server_handle_join(server_t *server) {
    join_t join_message = {};
    // if join ready then add client to the server
    if(server_join_ready(server)) {
        read(server->join_fd, &join_message, sizeof(join_t));
        server_add_client(server, &join_message);
        
        mesg_t new_message = {};
        new_message.kind = BL_JOINED;
        strcpy(new_message.name, join_message.name);
        strcpy(new_message.body, "");

        // broadcast join message to all clients
        server_broadcast(server, &new_message);
        server->join_ready = 0;

        return 0;
    }
    return 1;
}
// Call this function only if server_join_ready() returns true. Read a
// join request and add the new client to the server. After finishing,
// set the servers join_ready flag to 0.

int server_client_ready(server_t *server, int idx) {
    return server->client[idx].data_ready;
}
// Return the data_ready field of the given client which indicates
// whether the client has data ready to be read from it.

int server_handle_client(server_t *server, int idx) {
    // if data ready then handle incoming message
    if(server_client_ready(server, idx)) {
        client_t *ready_client = server_get_client(server, idx);
        int to_server_fd = ready_client->to_server_fd;
        mesg_t ready_message = {};

        // read incoming message and analyze the kind
        int nread = read(to_server_fd, &ready_message, sizeof(mesg_t));
        if (nread < 0) {
            return 1;
        }
        mesg_kind_t kind = ready_message.kind;
        mesg_kind_t departed = BL_DEPARTED;
        mesg_kind_t message = BL_MESG;

        // remove the client that is departed
        if(kind == departed) {
            server_remove_client(server, idx);
        }
        
        // broadcast message to all clients
        if(kind == departed || kind == message) {
            server_broadcast(server, &ready_message);
        }

        // reset handled client to have 0 data ready field
        ready_client->data_ready = 0;

        return 0;
    }

    return 1;
}
// Process a message from the specified client. This function should
// only be called if server_client_ready() returns true. Read a
// message from to_server_fd and analyze the message kind. Departure
// and Message types should be broadcast to all other clients.  Ping
// responses should only change the last_contact_time below. Behavior
// for other message types is not specified. Clear the client's
// data_ready flag so it has value 0.
//
// ADVANCED: Update the last_contact_time of the client to the current
// server time_sec.






// void server_tick(server_t *server);
// // ADVANCED: Increment the time for the server

// void server_ping_clients(server_t *server);
// // ADVANCED: Ping all clients in the server by broadcasting a ping.

// void server_remove_disconnected(server_t *server, int disconnect_secs);
// // ADVANCED: Check all clients to see if they have contacted the
// // server recently. Any client with a last_contact_time field equal to
// // or greater than the parameter disconnect_secs should be
// // removed. Broadcast that the client was disconnected to remaining
// // clients.  Process clients from lowest to highest and take care of
// // loop indexing as clients may be removed during the loop
// // necessitating index adjustments.

// void server_write_who(server_t *server);
// // ADVANCED: Write the current set of clients logged into the server
// // to the BEGINNING the log_fd. Ensure that the write is protected by
// // locking the semaphore associated with the log file. Since it may
// // take some time to complete this operation (acquire semaphore then
// // write) it should likely be done in its own thread to preven the
// // main server operations from stalling.  For threaded I/O, consider
// // using the pwrite() function to write to a specific location in an
// // open file descriptor which will not alter the position of log_fd so
// // that appends continue to write to the end of the file.

// void server_log_message(server_t *server, mesg_t *mesg);
// // ADVANCED: Write the given message to the end of log file associated
// // with the server.
