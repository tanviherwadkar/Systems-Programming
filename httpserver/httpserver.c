#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <regex.h>
#include <errno.h>
#include "asgn2_helper_funcs.h"

int min(int a, int b) {
    if (a <= b) {
        return a;
    } else {
        return b;
    }
}

int main(int argc, char *argv[]) {

    // make sure theres a port
    if (argc != 2) {
        fprintf(stderr, "Usage: ./httpserver <port>\n");
        exit(1);
    }

    // make sure the port is an int
    char *remainder = NULL;
    long port = strtol(argv[1], &remainder, 0);
    if (strlen(remainder) > 0) {
        fprintf(stderr, "Invalid port\n");
        exit(1);
    }

    // make sure the port is in the valid range
    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port\n");
        exit(1);
    }

    // initialize a socket
    Listener_Socket sock;
    listener_init(&sock,
        port); // i should probably be checking if listener_init returns an error but im not :3

    char buf[4096];
    char *reqpattern = "^([a-zA-Z]{1,8}) /([a-zA-Z0-9.-]{1,63}) (HTTP/[0-9]\\.[0-9])\r\n";
    char *hedpattern = "([a-zA-Z0-9.-]{1,128}):(\\s)([ -~]{1,128})\r\n";
    while (1) { // loop forever
        int sock_fd = listener_accept(
            &sock); // listen for a file descriptor (sock_fd == STD(IN/OUT) for client)

        if (sock_fd > 0) {
            // read in and store request
            ssize_t sock_bread = read_until(sock_fd, buf, 2048, "\r\n\r\n");
            if (sock_bread == -1) {
                dprintf(
                    sock_fd, "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
                close(sock_fd);
                continue;
            }
            buf[sock_bread] = 0;
            char request[sock_bread + 1];
            memcpy(request, buf, sock_bread + 1);
            //strncpy(request, buf, sock_bread);
            //printf("Client Request: \"%s\"\n", request);

            // regex request line
            regex_t regex;
            int ret = regcomp(&regex, reqpattern, REG_EXTENDED);
            if (ret != 0) {
                dprintf(sock_fd, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: "
                                 "22\r\nInternal Server Error\n");
                close(sock_fd);
                continue;
            }
            regmatch_t matches[4];
            ret = regexec(&regex, request, 4, matches, 0);
            if (ret != 0) {
                dprintf(
                    sock_fd, "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
                regfree(&regex);
                close(sock_fd);
                continue;
            }
            // get lengths of each match
            int length0 = matches[0].rm_eo - matches[0].rm_so;
            int length1 = matches[1].rm_eo - matches[1].rm_so;
            int length2 = matches[2].rm_eo - matches[2].rm_so;
            int length3 = matches[3].rm_eo - matches[3].rm_so;
            //printf("%d %d %d\n", length0, length1, length2);
            // create new strings with respective lengths
            char match0[length0];
            char match1[length1];
            char match2[length2];
            char match3[length3];
            // copy the match over
            strncpy(match0, request + matches[0].rm_so, length0);
            strncpy(match1, request + matches[1].rm_so, length1);
            strncpy(match2, request + matches[2].rm_so, length2);
            strncpy(match3, request + matches[3].rm_so, length3);
            // null terminate
            match0[length0] = '\0';
            match1[length1] = '\0';
            match2[length2] = '\0';
            match3[length3] = '\0';

            // print
            // printf("0th match: \"%s\"\n", match0);
            // printf("1st match: \"%s\"\n", match1);
            // printf("2nd match: \"%s\"\n", match2);
            // printf("3rd match: \"%s\"\n", match3);
            regfree(&regex);

            // error checks: invalid version, method not implemented, directory
            if (strcmp("HTTP/1.1", match3) != 0) {
                dprintf(sock_fd, "HTTP/1.1 505 Version Not Supported\r\nContent-Length: "
                                 "22\r\n\r\nVersion Not Supported\n");
                close(sock_fd);
                continue;
            }

            int get = 0;
            (void) get;
            int put = 0;
            (void) put;

            if (strcmp("GET", match1) == 0) {
                get = 1;
            } else if (strcmp("PUT", match1) == 0) {
                put = 1;
            } else {
                dprintf(sock_fd,
                    "HTTP/1.1 501 Not Implemented\r\nContent-Length: 16\r\n\r\nNot Implemented\n");
                close(sock_fd);
                continue;
            }

            /*
            GET FUNCTIONALITY
            */

            if (get == 1) {
                int dircheck = open(match2, O_DIRECTORY);
                if (dircheck != -1) {
                    dprintf(sock_fd,
                        "HTTP/1.1 403 1 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n");
                    close(dircheck);
                    close(sock_fd);
                    continue;
                }

                int infd = open(match2, O_RDONLY);
                if (infd == -1) {
                    if (errno == ENOENT) {
                        dprintf(sock_fd,
                            "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n");
                        close(infd);
                        close(sock_fd);
                        continue;
                    } else if (errno == EACCES) {
                        dprintf(sock_fd,
                            "HTTP/1.1 403 2 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n");
                        close(infd);
                        close(sock_fd);
                        continue;
                    }
                }
                int tbread = 0;
                int bytes_read;
                while ((bytes_read = read(infd, buf, 4096)) > 0) {
                    if (bytes_read == -1) {
                        tbread = -1;
                        break;
                    }
                    tbread += bytes_read;
                }
                close(infd);
                if (tbread == -1) {
                    dprintf(sock_fd, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: "
                                     "22\r\nInternal Server Error\n");
                    close(sock_fd);
                    continue;
                }

                dprintf(sock_fd, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", tbread);
                infd = open(match2, O_RDONLY);
                int bpassed = pass_n_bytes(infd, sock_fd, tbread);
                if (bpassed == -1) {
                    dprintf(sock_fd, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: "
                                     "22\r\nInternal Server Error\n");
                    close(sock_fd);
                    close(infd);
                    continue;
                }
                close(infd);
            }

            /*
            PUT FUNCTIONALITY
            */
            if (put == 1) {
                printf("\nmade it to put\n\n");
                int rh = regcomp(&regex, hedpattern, REG_EXTENDED);
                rh = regexec(&regex, request, 4, matches, 0);
                int offset = 0;
                int fail = 0;
                char key[30];
                while (1) {
                    if (rh != 0) {
                        dprintf(sock_fd,
                            "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
                        regfree(&regex);
                        close(sock_fd);
                        fail = 1;
                        break;
                    }
                    int len1 = matches[1].rm_eo - matches[1].rm_so;
                    int len3 = matches[3].rm_eo - matches[3].rm_so;
                    char hmatch1[len1 + 1];
                    char hmatch3[len3 + 1];
                    strncpy(hmatch1, request + offset + matches[1].rm_so, len1);
                    strncpy(hmatch3, request + offset + matches[3].rm_so, len3);
                    hmatch1[len1] = 0;
                    hmatch3[len3] = 0;
                    if (strcmp(hmatch1, "Content-Length") == 0) {
                        strcpy(key, hmatch3);
                    }
                    offset += matches[0].rm_eo;
                    if (strncmp(request + offset, "\r\n", 2) == 0) {
                        offset += 2;
                        break;
                    }
                    rh = regexec(&regex, request + offset, 4, matches, 0);
                }
                if (fail == 1) {
                    continue;
                }
                regfree(&regex);

                // make sure key is valid
                char *remainder = NULL;
                long inplen = strtol(key, &remainder, 0);
                if (strlen(remainder) > 0) {
                    dprintf(sock_fd,
                        "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
                    close(sock_fd);
                    continue;
                }
                printf("%ld\n", inplen);

                // open or create file
                int existed = 1;
                int newfile = open(match2, O_WRONLY | O_TRUNC, 0666);
                if (newfile == -1) {
                    existed = 0;
                }
                if (existed == 0) {
                    newfile = open(match2, O_TRUNC | O_CREAT | O_WRONLY, 0666);
                }
                int minlen = min(sock_bread - offset, inplen);
                ssize_t bwrit = write_n_bytes(newfile, request + offset, minlen);
                if (bwrit == -1) {
                    dprintf(sock_fd, "ERROR\n");
                    close(sock_fd);
                    continue;
                }
                pass_n_bytes(sock_fd, newfile, inplen - minlen);
                printf("message overlap: %ld\n", sock_bread - offset);
                close(newfile);
                if (existed == 1) {
                    dprintf(sock_fd, "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n");
                }
                if (existed == 0) {
                    dprintf(sock_fd, "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n");
                }
            }
        }
        close(
            sock_fd); // you can pretty much treat the sock_fd like a normal file descriptor so it needs to be closed too
    }
    return 0;
}
