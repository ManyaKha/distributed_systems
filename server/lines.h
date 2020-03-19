#include <unistd.h>

int send_msg(int socket, char *mensaje, int longitud);
int receive_msg(int socket, char *mensaje, int longitud);
ssize_t read_line(int fd, void *buffer, size_t n);
ssize_t write_line(int fd, void *buffer, size_t n);