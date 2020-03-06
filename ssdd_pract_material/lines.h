#include <unistd.h>

int enviar(int socket, char *mensaje, int longitud);
int recibir(int socket, char *mensaje, int longitud);
ssize_t readLine(int fd, void *buffer, size_t n);
ssize_t writeLine(int fd, void *buffer, size_t n);