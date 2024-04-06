#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <regex.h>
#include <errno.h>
#include "queue.h"
#include "rwlock.h"
#include "asgn2_helper_funcs.h"
#include "LinkedList.h"

#define OPTIONS "t:"
queue_t *q;
pthread_mutex_t mutex;
List L;

// =============================================
// Function Declarations
void handle_rq(int fd);
void *worker();
// =============================================

// =============================================
// Main
int main(int argc, char **argv) {

    if (argc < 2) {
        fprintf(stderr, "Usage: ./httpserver -t <num threads> <port>\n");
        exit(1);
    }

    int opt = 0;
    long numt = 0;

    char *remainder = NULL;

    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
        case 't':
            numt = strtol(optarg, &remainder, 0);
            if (strlen(remainder) > 0) {
                fprintf(stderr, "Invalid thread size.\n");
                exit(1);
            }
            break;
        default: break;
        }
    }

    if (argv[optind] == NULL) {
        fprintf(stderr, "Usage: ./httpserver -t <num threads> <port>\n");
        exit(1);
    }
    long port = strtol(argv[optind], &remainder, 0);

    if (strlen(remainder) > 0) {
        fprintf(stderr, "Invalid port.\n");
        exit(1);
    }

    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port.\n");
        exit(1);
    }

    printf("numt = %d port = %d\n", (int) numt, (int) port);

    Listener_Socket sock;
    listener_init(&sock, port);

    // initialize global variables
    pthread_mutex_init(&mutex, NULL);
    q = queue_new(numt);
    L = newList();
    (void) L;

    pthread_t threads[numt];
    for (int i = 0; i < (int) numt; i++) {
        pthread_create(&(threads[i]), NULL, worker, NULL);
    }

    while (1) {
        uintptr_t sock_fd = listener_accept(&sock);
        queue_push(q, (void *) sock_fd);
    }

    return 0;
}
// =============================================

// =============================================
// Helper Functions
int min(int a, int b) {
    if (a <= b) {
        return a;
    } else {
        return b;
    }
}

void *worker() {
    while (1) {
        uintptr_t sock_fd = -1;
        queue_pop(q, (void **) &sock_fd);
        handle_rq(sock_fd);
        close(sock_fd);
    }
}

void handle_rq(int sock_fd) {
    char buf[4096];
    char *reqpattern = "^([a-zA-Z]{1,8}) /([a-zA-Z0-9.-]{1,63}) (HTTP/[0-9]\\.[0-9])\r\n";
    char *hedpattern = "([a-zA-Z0-9.-]{1,128}):(\\s)([ -~]{1,128})\r\n";

    // read in and store request
    ssize_t sock_bread = read_until(sock_fd, buf, 2048, "\r\n\r\n");
    if (sock_bread == -1) {
        dprintf(sock_fd, "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
        close(sock_fd);
        return;
    }
    buf[sock_bread] = 0;
    char request[sock_bread + 1];
    memcpy(request, buf, sock_bread + 1);

    // regex request line
    regex_t regex;
    int ret = regcomp(&regex, reqpattern, REG_EXTENDED);
    if (ret != 0) {
        dprintf(sock_fd, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: "
                         "22\r\nInternal Server Error\n");
        close(sock_fd);
        return;
    }
    regmatch_t matches[4];
    ret = regexec(&regex, request, 4, matches, 0);
    if (ret != 0) {
        dprintf(sock_fd, "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
        regfree(&regex);
        close(sock_fd);
        return;
    }
    // get lengths of each match
    int length0 = matches[0].rm_eo - matches[0].rm_so;
    int length1 = matches[1].rm_eo - matches[1].rm_so;
    int length2 = matches[2].rm_eo - matches[2].rm_so;
    int length3 = matches[3].rm_eo - matches[3].rm_so;
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
    regfree(&regex);

    if (strcmp("HTTP/1.1", match3) != 0) {
        dprintf(sock_fd, "HTTP/1.1 505 Version Not Supported\r\nContent-Length: "
                         "22\r\n\r\nVersion Not Supported\n");
        close(sock_fd);
        return;
    }

    rwlock_t *file_lock;
    // CRITICAL REGION: manipulating global variable
    pthread_mutex_lock(&mutex);
    bool found = false;
    for (moveFront(L); get_index(L) >= 0; moveNext(L)) {
        if (strcmp(match2, get_filename(L)) == 0) {
            file_lock = get_lock(L);
            found = true;
            break;
        }
    }
    if (found == false) { // if rwlock for file doesnt exist
        append(L, match2); // append to the linked list
        file_lock = back(L); // get the rwlock of the last element in the list
    }
    pthread_mutex_unlock(&mutex);
    // END CRITICAL REGION

    // handle header (request id & content length)
    int rh = regcomp(&regex, hedpattern, REG_EXTENDED);
    rh = regexec(&regex, request, 4, matches, 0);
    int offset = 0;
    int fail = 0;
    char cl[30] = "0";
    char ri[30] = "0";
    bool clfound = false;
    while (1) {
        if (rh != 0) {
            regfree(&regex);
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
            strcpy(cl, hmatch3);
            clfound = true;
        } else if (strcmp(hmatch1, "Request-Id") == 0) {
            strcpy(ri, hmatch3);
        }
        offset += matches[0].rm_eo;
        if (strncmp(request + offset, "\r\n", 2) == 0) {
            offset += 2;
            break;
        }
        rh = regexec(&regex, request + offset, 4, matches, 0);
    }
    if (fail == 1) {
        dprintf(sock_fd, "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
        close(sock_fd);
        return;
    }
    regfree(&regex);

    // atoi content length & request id
    long clint = -1;
    (void) clint;
    long riint = 0;
    (void) riint;
    char *remainder = NULL;
    if (clfound) {
        clint = strtol(cl, &remainder, 0);
        if (strlen(remainder) > 0) {
            dprintf(sock_fd, "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
            close(sock_fd);
            return;
        }
    }
    remainder = NULL;
    riint = strtol(ri, &remainder, 0);
    if (strlen(remainder) > 0) {
        dprintf(sock_fd, "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
        close(sock_fd);
        return;
    }

    bool get = false;
    bool put = false;

    if (strcmp("GET", match1) == 0) {
        get = true;
    } else if (strcmp("PUT", match1) == 0) {
        put = true;
    } else {
        dprintf(
            sock_fd, "HTTP/1.1 501 Not Implemented\r\nContent-Length: 16\r\n\r\nNot Implemented\n");
        close(sock_fd);
        return;
    }

    if (get) {
        reader_lock(file_lock);
        int dircheck = open(match2, O_DIRECTORY);
        if (dircheck != -1) {
            dprintf(sock_fd, "HTTP/1.1 403 1 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n");
            fprintf(stderr, "GET,/%s,403,%ld\n", match2, riint);
            close(dircheck);
            close(sock_fd);
            reader_unlock(file_lock);
            return;
        }
        int infd = open(match2, O_RDONLY);
        if (infd == -1) {
            if (errno == ENOENT) {
                dprintf(sock_fd, "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n");
                fprintf(stderr, "GET,/%s,404,%ld\n", match2, riint);
                close(infd);
                close(sock_fd);
                reader_unlock(file_lock);
                return;
            } else if (errno == EACCES) {
                dprintf(
                    sock_fd, "HTTP/1.1 403 2 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n");
                fprintf(stderr, "GET,/%s,403,%ld\n", match2, riint);
                close(infd);
                close(sock_fd);
                reader_unlock(file_lock);
                return;
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
            fprintf(stderr, "GET,/%s,500,%ld\n", match2, riint);
            close(sock_fd);
            reader_unlock(file_lock);
            return;
        }

        dprintf(sock_fd, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", tbread);
        fprintf(stderr, "GET,/%s,200,%ld\n", match2, riint);
        infd = open(match2, O_RDONLY);
        int bpassed = pass_n_bytes(infd, sock_fd, tbread);
        if (bpassed == -1) {
            dprintf(sock_fd, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: "
                             "22\r\nInternal Server Error\n");
            fprintf(stderr, "GET,/%s,500,%ld\n", match2, riint);
            close(sock_fd);
            close(infd);
            reader_unlock(file_lock);
            return;
        }
        close(infd);
        reader_unlock(file_lock);
        return;
    }

    else if (put) {
        writer_lock(file_lock);
        int existed = 1;
        int newfile = open(match2, O_WRONLY | O_TRUNC, 0666);
        if (newfile == -1) {
            existed = 0;
        }
        if (existed == 0) {
            newfile = open(match2, O_TRUNC | O_CREAT | O_WRONLY, 0666);
        }
        int minlen = min(sock_bread - offset, clint);
        ssize_t bwrit = write_n_bytes(newfile, request + offset, minlen);
        if (bwrit == -1) {
            dprintf(sock_fd, "ERROR\n");
            close(sock_fd);
            return;
        }
        pass_n_bytes(sock_fd, newfile, clint - minlen);
        printf("message overlap: %ld\n", sock_bread - offset);
        close(newfile);
        if (existed == 1) {
            dprintf(sock_fd, "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n");
            fprintf(stderr, "PUT,/%s,200,%ld\n", match2, riint);
        } else if (existed == 0) {
            dprintf(sock_fd, "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n");
            fprintf(stderr, "PUT,/%s,201,%ld\n", match2, riint);
        }
        writer_unlock(file_lock);
    }
}
