#include <unistd.h>
#include <errno.h>
#include "lines.h"

int enviar(int socket, char *mensaje, int longitud)
{
	int r;
	int l = longitud;
		

	do {	
		r = write(socket, mensaje, l);
		l = l -r;
		mensaje = mensaje + r;
	} while ((l>0) && (r>=0));
	
	if (r < 0)
		return (-1);   /* fallo */
	else
		return(0);	/* se ha enviado longitud */
}

int recibir(int socket, char *mensaje, int longitud)
{
	int r;
	int l = longitud;
		

	do {	
		r = read(socket, mensaje, l);
		l = l -r ;
		mensaje = mensaje + r;
	} while ((l>0) && (r>=0));
	
	if (r < 0)
		return (-1);   /* fallo */
	else
		return(0);	/* se ha enviado longitud */
}



ssize_t readLine(int fd, void *buffer, size_t n)
{
	ssize_t numRead;  /* num of bytes fetched by last read() */
	size_t totRead;	  /* total bytes read so far */
	char *buf;
	char ch;


	if (n <= 0 || buffer == NULL) { 
		errno = EINVAL;
		return -1; 
	}

	buf = buffer;
	totRead = 0;
	
	for (;;) {
        	numRead = read(fd, &ch, 1);	/* read a byte */

        	if (numRead == -1) {	
            		if (errno == EINTR)	/* interrupted -> restart read() */
                		continue;
            	else
			return -1;		/* some other error */
        	} else if (numRead == 0) {	/* EOF */
            		if (totRead == 0)	/* no byres read; return 0 */
                		return 0;
			else
                		break;
        	} else {			/* numRead must be 1 if we get here*/
            		if (ch == '\n')
                		break;
            		if (ch == '\0')
                		break;
            		if (totRead < n - 1) {		/* discard > (n-1) bytes */
				totRead++;
				*buf++ = ch; 
			}
		} 
	}
	
	*buf = '\0';
    	return totRead;
}


ssize_t writeLine(int fd, void *buffer, size_t n)
{
        int err;
        char endLine[] = "\n";


        if (n <= 0 || buffer == NULL) {
                errno = EINVAL;
                return -1;
        }

        err = enviar(fd, buffer, n);
        if (err == -1)
                return err;
        err = enviar(fd, endLine, 1);
        if (err == -1)
                return err;

        return 0;
}

