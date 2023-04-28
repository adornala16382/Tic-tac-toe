#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

int connect_inet(char *host, char *service){
    struct addrinfo hints, *info_list, *info;
    int sock, error;
    // look up remote host
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // in practice, this means give us IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // indicate we want a streaming socket
    error = getaddrinfo(host, service, &hints, &info_list);
    if (error) {
        fprintf(stderr, "error looking up %s:%s: %s\n", host, service,
        gai_strerror(error));
        return -1;
    }
    for (info = info_list; info != NULL; info = info->ai_next) {
        sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        if (sock < 0) continue;
        error = connect(sock, info->ai_addr, info->ai_addrlen);
        if (error) {
            close(sock);
            continue;
        }
        break;
    }
    freeaddrinfo(info_list);
    if (info == NULL) {
        fprintf(stderr, "Unable to connect to %s:%s\n", host, service);
        return -1;
    }
    return sock;
}
#define BUFLEN 300
int main(int argc, char **argv){
    int sock, bytes;
    char buf[BUFLEN];
    if (argc != 3) {
        printf("Specify host and service\n");
        exit(EXIT_FAILURE);
    }
    sock = connect_inet(argv[1], argv[2]);
    if (sock < 0) exit(EXIT_FAILURE);
    while (1) {
        bytes = read(sock, buf, BUFLEN);
        if (bytes < 0) {
            perror("Error reading from socket");
            close(sock);
            exit(EXIT_FAILURE);
        }
        if(strcmp(buf, "CAN_READ\n") == 0){
            if((bytes = read(STDIN_FILENO, buf, BUFLEN)) > 0){
                int bytes_sent = write(sock, buf, bytes);
                // FIXME: should check whether the write succeeded!
                if (bytes_sent < 0) {
                    perror("Error writing to socket");
                    close(sock);
                    exit(EXIT_FAILURE);
                }
                else if (bytes_sent != bytes) {
                    fprintf(stderr, "Error: only %d bytes out of %d were sent to socket\n", bytes_sent, bytes);
                    close(sock);
                    exit(EXIT_FAILURE);
                }
            }
        }
        else{
            write(STDOUT_FILENO, buf, bytes);
        }
    }
    close(sock);
    return EXIT_SUCCESS;
}
