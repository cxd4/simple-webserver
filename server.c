/*
 * project:     miniweb
 * author:      Oscar Sanchez (oms1005@gmail.com)
 * HTTP Server
 * WORKS ON BROWSERS TOO!
 * Inspired by IBM's nweb
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * 2016.05.07 cxd4
 *
 * setpgrp() documentation says to use setpgid() instead.
 * Maybe this is why gcc -ansi -Wall complains about an implicit name.
 */
#include <unistd.h>

#define BUFSIZE 8096
#define ERROR 42
#define SORRY 43
#define LOG   44

struct {
    char *ext;
    char *filetype;
} extensions[] = {
    { "gif",  "image/gif" },
    { "jpg",  "image/jpeg" },
    { "jpeg", "image/jpeg" },
    { "png",  "image/png" },
    { "zip",  "image/zip" },
    { "gz",   "image/gz" },
    { "tar",  "image/tar" },
    { "htm",  "text/html" },
    { "html", "text/html" },
    { "php",  "image/php" },
    { "cgi",  "text/cgi" },
    { "asp",  "text/asp" },
    { "jsp",  "image/jsp" },
    { "xml",  "text/xml" },
    { "js",   "text/js" },
    { "css",  "test/css" },
    { 0, 0 }
};

void my_log(int type, char *s1, char *s2, int num)
{
    int fd;
    char logbuffer[BUFSIZE * 2];

    switch (type) {
    case ERROR:
        sprintf(logbuffer, "ERROR:  %s:%s Errno=%d exiting pid=%d", s1, s2, errno, getpid());
        break;
    case SORRY:
        sprintf(logbuffer, "<HTML><BODY><H1>Web Server Sorry: %s %s</H1></BODY></HTML>\r\n", s1, s2);
        write(num, logbuffer, strlen(logbuffer));
        sprintf(logbuffer, "SORRY:  %s:%s", s1, s2);
        break;
    case LOG:
        sprintf(logbuffer, "INFO :  %s:%s:%d", s1, s2, num);
        break;
    }

    if ((fd = open("server.log", O_CREAT | O_WRONLY | O_APPEND, 0644)) >= 0) {
        write(fd, logbuffer, strlen(logbuffer));
        write(fd, "\n", 1);
        close(fd);
    }
    if (type == ERROR || type == SORRY)
        exit(3);
}

void web(int fd, int hit)
{
    int j, file_fd, buflen, len;
    long i, ret;
    char * fstr;
    static char buffer[BUFSIZE + 1];

    ret = read(fd, buffer, BUFSIZE);
    if (ret == 0 || ret == -1) {
        my_log(SORRY, "failed to read browser request", "", fd);
    }
    if (ret > 0 && ret < BUFSIZE)
        buffer[ret]=0;
    else
        buffer[0]=0;

    for (i = 0; i < ret; i++)
        if (buffer[i] == '\r' || buffer[i] == '\n')
            buffer[i] = '*';
    my_log(LOG, "request", buffer, hit);

    if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4))
        my_log(SORRY, "Only simple GET operation supported", buffer, fd);

    for (i = 4; i < BUFSIZE; i++) {
        if (buffer[i] == ' ') {
            buffer[i] = 0;
            break;
        }
    }

    for (j = 0; j < i - 1; j++)
        if (buffer[j] == '.' && buffer[j + 1] == '.')
            my_log(SORRY, "Parent directory (..) path names not supported", buffer, fd);

    if (!strncmp(&buffer[0], "GET /\0", 6) || !strncmp(&buffer[0], "get /\0", 6))
        strcpy(buffer, "GET /index.html");

    buflen = strlen(buffer);
    fstr = (char *)0;
    for (i = 0; extensions[i].ext != 0; i++) {
        len = strlen(extensions[i].ext);
        if (!strncmp(&buffer[buflen - len], extensions[i].ext, len)) {
            fstr = extensions[i].filetype;
            break;
        }
    }
    if (fstr == 0)
        my_log(SORRY, "file extension type not supported", buffer, fd);

    if ((file_fd = open(&buffer[5], O_RDONLY)) == -1)
        my_log(SORRY, "failed to open file", &buffer[5], fd);

    my_log(LOG, "SEND", &buffer[5], hit);

    sprintf(buffer, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\n\r\n", fstr);
    write(fd, buffer, strlen(buffer));

    while ((ret = read(file_fd, buffer, BUFSIZE)) > 0) {
        write(fd, buffer, ret);
    }
#ifdef LINUX
    sleep(1);
#endif
    exit(1);
}

static const char* baddirs[] = {
    "/", "/etc", "/bin", "/lib", "/tmp", "/usr", "/dev", "/sbin"
};
int main(int argc, char **argv)
{
    static struct sockaddr_in cli_addr;
    static struct sockaddr_in serv_addr;
    long string_as_number;
    unsigned short port;
    int status;
    int pid, listenfd, hit;
    register int i;

    if (argc < 3  || argc > 3 || !strcmp(argv[1], "-?")) {
        printf(
            "usage: server [port] [server directory] &"
            "\tExample: server 80 ./ &\n\n"
            "\tOnly Supports: "
        );
        for (i = 0; extensions[i].ext != 0; i++) {
            putchar(' ');
            fputs(extensions[i].ext, stdout);
        }

        putchar('\n');
        exit(0);
    }
    for (i = 0; i < 8; i++) {
        if (strncmp(argv[2], baddirs[i], strlen(baddirs[i])) == 0) {
            printf("ERROR:  Bad top directory %s, see server -?\n", argv[2]);
            exit(3);
        }
    }
    if (chdir(argv[2]) == -1) {
        printf("ERROR: Can't Change to directory %s\n", argv[2]);
        exit(4);
    }

    if (fork() != 0)
        return 0;
    signal(SIGCLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    for (i = 0; i < 32; i++)
        close(i);

    status = setpgrp();
    if (status != 0)
        fprintf(stderr, "Process group set failed with error %i.\n", status);

    my_log(LOG, "http server starting", argv[1], getpid());

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
        my_log(ERROR, "system call", "socket", 0);

    string_as_number = strtol(argv[1], NULL, 0);
    if (string_as_number < 0 || string_as_number > 60000) {
        fprintf(stderr, "Invalid port number:  %li\n", string_as_number);
        return 1;
    }
    port = (unsigned short)string_as_number;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        my_log(ERROR, "system call", "bind", 0);
    if (listen(listenfd, 64) < 0)
        my_log(ERROR, "system call", "listen", 0);

    for (hit = 1; ; hit++) {
        socklen_t length;
        int socketfd;

        length = sizeof(cli_addr);
        socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length);
        if (socketfd < 0)
            my_log(ERROR, "system call", "accept", 0);

        if ((pid = fork()) < 0) {
            my_log(ERROR, "system call", "fork", 0);
        } else {
            if (pid == 0) {
                close(listenfd);
                web(socketfd, hit);
            } else {
                close(socketfd);
            }
        }
    }
}
