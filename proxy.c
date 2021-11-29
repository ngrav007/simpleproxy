#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>      /* memset() */
#include <sys/socket.h>  /* socket() */
#include <sys/types.h>
#include <netinet/in.h>  /* Defines types like AF_INET */
#include <netdb.h>       /* gethostbyaddr() */
#include <arpa/inet.h>   /* inet_ntoa() */
#include <unistd.h>      /* Read and write and shit */

#include "CacheTable.h"

#define BUFSIZE     8192
#define CACHESIZE     10
#define HOUR        3600
#define MB         10000

int validate_request(HTTP_R cobj);
void store_response_info(HTTP_R cobj, char *response, size_t response_l);
void print_cache_obj(HTTP_R cobj);
char *forge_http_response(HTTP_R centry, int *response_l);

int main(int argc, char **argv) {
    /* Command-line Arg Checking */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

/* -------------------------------------------------------------------------- */

    int listenfd;                     /* Listening proxy socket */
    int sockfd;                       /* Send/Recv proxy socket */
    int connfd_c;                     /* Connection Socket (w/ client) */
    int proxy_port;                   /* Port to listen on */
    int server_port;                  /* Server's port */
    int client_len;                   /* Byte size of client's address */
    struct sockaddr_in proxyaddr;     /* Proxy's address */
    struct sockaddr_in serveraddr;    /* Server's address */
    struct sockaddr_in clientaddr;    /* Client's address */
    struct hostent *client;           /* Client's info */
    struct hostent *server;           /* Server's info */
    char *client_hostaddr;            /* Client's Host IP Address */
    char *server_hostnameptr;         /* Server's Hostname String */
    char *res_buffer;                 /* HTTP Response buffer */
    char  buffer[BUFSIZE];        /* HTTP Request buffer */
    int optval;                       /* Flag value for setsockopt */
    int read_size;                    /* Message byte size, read() */
    int write_size;                   /* Message byte size, write() */

/* ---------------------------- Configure Proxy ----------------------------- */

    /* Create Cache */
    Cache cache = Cache_new(CACHESIZE);

    /* Assign Port Number for Proxy */
    proxy_port = atoi(argv[1]);

    /* Create socket for proxy to listen on */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("Error: opening listening socket\n");
        exit(EXIT_FAILURE);
    }

    /* Set Socket Option */
    optval = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
        (const void *)&optval, sizeof(int));

    /* Build the Proxy's Internet Address */
    bzero((char *) &proxyaddr, sizeof(proxyaddr));
    proxyaddr.sin_family = AF_INET;
    proxyaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    proxyaddr.sin_port = htons((unsigned short)proxy_port);

    /* Bind() the listening socket to a port number */ 
    if (bind(listenfd, (const struct sockaddr *)&proxyaddr,
            sizeof(proxyaddr)) < 0) {
        perror("Error: binding listening socket\n");
        exit(EXIT_FAILURE);
    }

    /* Listen() for a connection from the client */
    if (listen(listenfd, 1) < 0) {
        perror("Error: setting listening socket\n");
        exit(EXIT_FAILURE);
    }

/* ---------------------- Client: Handling HTTP Request --------------------- */
    
    /* Accept: Tell listening socket to accept incoming connections */   
    client_len = sizeof(clientaddr);
    while (1) {
        connfd_c = accept(listenfd, (struct sockaddr *)&clientaddr, 
                            &client_len);
        if (connfd_c < 0) {
            perror("Error: accepting client connection\n");
            exit(EXIT_FAILURE);
        }
    
        /* Get Host by Address (gethostbyaddr): determine who sent message */
        client = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
                                sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        if (client == NULL) {
            fprintf(stderr, "Error: gethostbyaddr - getting client info\n");
            exit(EXIT_FAILURE);
        }

        client_hostaddr = inet_ntoa(clientaddr.sin_addr);
        if (client_hostaddr == NULL) {
            perror("Error: inet_ntoa - binary IPv4 to string\n");
            exit(EXIT_FAILURE);
        }

        /* Print connection message to stdout */
        // fprintf(stdout, "Proxy established connection with %s (%s)\n",
        //             client->h_name, client_hostaddr);

        /* Read: Read the input string from client */
        bzero(buffer, BUFSIZE);
        read_size = read(connfd_c, buffer, BUFSIZE);
        if (read_size < 0) {
            fprintf(stderr, "Error: reading from socket\n");
            perror("Error read() - ");
            close(connfd_c);
            Cache_free(cache);
            exit(EXIT_FAILURE);
        } else if (strcmp(buffer, "halt\n") == 0) {
            fprintf(stderr, "Halt signal recieved! Terminating. . .\n");
            close(connfd_c);
            Cache_free(cache);
            exit(EXIT_FAILURE);
        }

        /* Copy HTTP cobj for later use */
        char *http_request = malloc(sizeof(char) * read_size + 1);
        bzero(http_request, read_size + 1);
        strncpy(http_request, buffer, read_size);

/* ---------------- Proxy: Store HTTP Request Contents in Cache ------------- */

        /* Parse HTTP cobj and store in cache */
        HTTP_R cobj = CacheObject_new(http_request);
        free(http_request);
        /* HTTP cobj Validation */
        if (validate_request(cobj) != 1) {
            fprintf(stderr, "Error: invalid HTTP request\n");
            close(connfd_c);
            CacheObject_free(cobj);
            Cache_free(cache);
            exit(EXIT_FAILURE);
        }

/* ------------------- Server: Send HTTP Request to Server ------------------ */
        HTTP_R centry = Cache_get(cache, cobj);
        if (centry == NULL) {
            /* Update Cache Contents */
            Cache_update(cache);

            /* Set Port Number (Default = 80) */
            server_port = cobj->port;

            /* Create socket for proxy to send requests */
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) {
                fprintf(stderr, "Error: opening socket\n");
                close(connfd_c);
                Cache_free(cache);
                exit(EXIT_FAILURE);
            }

            server = gethostbyname(cobj->host);
            if (server == NULL) {
                fprintf(stderr, "Error: no such host: %s \n", cobj->host);
                perror("Error: gethostbyname() - ");
                close(sockfd);
                close(connfd_c);
                Cache_free(cache);
                exit(EXIT_FAILURE);
            }

            /* Build the server's Internet address */
            bzero((char *) &serveraddr, sizeof(serveraddr));
            serveraddr.sin_family = AF_INET;
            bcopy((char *)server->h_addr, 
                  (char *)&serveraddr.sin_addr.s_addr,
                   server->h_length);
            fprintf(stderr, "PORT = %d\n", server_port);                   
            serveraddr.sin_port = htons(server_port);

            /* Connect: create a connection with the server */
            if (connect(sockfd, (const struct sockaddr *)&serveraddr,
                            sizeof(serveraddr)) < 0) {
                perror("Error: connecting with the server\n");
                close(connfd_c);
                close(sockfd);
                Cache_free(cache);
                exit(EXIT_FAILURE);
            }

            /* Write: send HTTP Request to server */
            write_size = write(sockfd, buffer, read_size);
            if (write_size < 0) {
                perror("Error: writing to socket to server\n");
                close(connfd_c);
                close(sockfd);
                Cache_free(cache);
                exit(EXIT_FAILURE);
            }

/* ------------------ Server: Get HTTP Response from Server ----------------- */
            /* Read: the server's reply into res_buffer, and send to client*/
            res_buffer = calloc(MB, sizeof(char));

            int read_total = 0;
            int resize = 1;
            do {
                bzero(buffer, BUFSIZE);
                read_size = read(sockfd, buffer, BUFSIZE - 1); 
                if (read_size < 0) {
                    perror("Error: reading from socket to server\n");
                    close(connfd_c);
                    close(sockfd);
                    free(res_buffer);
                    Cache_free(cache);
                    exit(EXIT_FAILURE);
                }
                
                if (read_total >= sizeof(res_buffer)) {
                    resize++;
                    char *new_buffer = calloc(MB * resize + 1,
                                               sizeof(char));
                    memcpy(new_buffer, res_buffer, read_total);
                    free(res_buffer);
                    res_buffer = new_buffer;
                }
                memcpy((res_buffer + read_total), buffer, read_size);
                read_total += read_size;
                // strcat(res_buffer, buffer);
            } while (read_size > 0);

            char *http_response = malloc(sizeof(char) * read_total + 1);
            bzero(http_response, read_total + 1);
            memcpy(http_response, res_buffer, read_total);

/* --------------- Cache: Store Important Response Information  ------------- */
            store_response_info(cobj, http_response, read_total);
            free(http_response);

/* ------------------- Client: Send HTTP Response to Client ----------------- */
            write_size = write(connfd_c, res_buffer, read_total);
            if (write_size < 0) {
                perror("Error: writing to socket to server\n");
                close(connfd_c);
                close(sockfd);
                free(res_buffer);
                Cache_free(cache);
                exit(EXIT_FAILURE);
            }
            free(res_buffer);
        } else {
            CacheObject_free(cobj);
            Cache_update(cache);
            int response_len = 0;
            char *forged_response = forge_http_response(centry, &response_len);
            // int response_len = strlen(forged_response);
            write_size = write(connfd_c, forged_response, response_len);
            if (write_size < 0) {
                perror("Error: writing to socket to server\n");
                close(connfd_c);
                close(sockfd);
                free(res_buffer);
                Cache_free(cache);
                exit(EXIT_FAILURE);
            }
            free(forged_response);
        }
    }

/* ------------------------- Terminate Connection: End ---------------------- */
    Cache_free(cache);
    close(connfd_c);
    close(sockfd);

    return 0;
}

/* -------------------------- Function Implementations ---------------------- */

void store_response_info(HTTP_R cobj, char *response, size_t response_l)
{
    char *header_start = response;
    char *status_end   = strstr(response, "\n");    
    char *header_end   = strstr(response, "\r\n\r\n");
    char *status = malloc(sizeof(char) * (status_end - header_start) + 2);
    int status_sz = status_end - header_start + 1;
    bzero(status, status_sz + 1);
    memcpy(status, response, status_sz); 
    cobj->status = status;
    cobj->status_l = status_sz;

    char *content = malloc(sizeof(char) * (response_l - status_sz));
    int content_sz = response_l - status_sz;
    bzero(content, content_sz);
    memcpy(content, status_end + 1, content_sz);
    cobj->content = content;
    cobj->content_l = content_sz;    

    char *cache_cntrl = strstr(response, "Cache-Control: ");
    if (cache_cntrl == NULL) {
        cobj->max_age = HOUR;
    } else {
        cobj->max_age = atoi((cache_cntrl + 15));
    }
    fprintf(stderr, "cache_cntrl = %s", cache_cntrl);
    fprintf(stderr, "max-age = %d", cobj->max_age);
}

void print_cache_obj(HTTP_R cobj)
{
    fprintf(stdout, "| method: %s\n| url: %s\n| version: %s\n| host: %s\n",
        cobj->method, cobj->url, cobj->version, cobj->host);
    fprintf(stdout, "| port: %d\n| max_age: %d\n| start_age: %d\n",
        cobj->port, cobj->max_age, cobj->start_age, cobj->curr_age);
    fprintf(stdout, "| curr_age: %d\n| content_l: %d\n| hash: %d\n", 
        cobj->curr_age, cobj->content_l, cobj->hash);
}

char *forge_http_response(HTTP_R centry, int *response_l)
{
    char buffer[BUFSIZE];
    bzero(buffer, BUFSIZE);
    sprintf(buffer, "Age: %d\r\n", centry->curr_age);
    int buf_l = strlen(buffer);

    char *age_line = malloc(sizeof(char) * buf_l + 1);
    bzero(age_line, buf_l + 1);
    strncpy(age_line, buffer, buf_l);

    *response_l = buf_l + centry->status_l + centry->content_l;
    char *response = malloc(sizeof(char) * (*response_l));
    bzero(response, *response_l);
    strncpy(response, centry->status, centry->status_l);
    strcat(response, age_line);
    memcpy(response + centry->status_l + buf_l, centry->content, centry->content_l);
    free(age_line);

    return response;
}

int validate_request(HTTP_R cobj)
{
    int valid;
    valid = (cobj->method  !=  NULL) ? 1 : 0;
    valid = (cobj->host    !=  NULL) ? 1 : 0;

    if (valid != 0) {
        valid = (cobj->port    >=     0) ? 1 : 0;
        valid = (cobj->port    <= 65353) ? 1 : 0;
    }
    
    return valid;
}
