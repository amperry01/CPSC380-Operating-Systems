
#include <fcntl.h>
#include <unistd.h>

int main (int argc, char *argv[]){
    int fin, fout, nbytes;
    char buffer[1024];

    fin = open(argv[1], O_RDONLY);

    if (fin == -1){
        write(2, "Error opening file\n", 20);
        return -1;
    }

    fout = open(argv[2], O_WRONLY | O_CREAT, 0644);

    if (fout == -1){
        write(2, "Error opening file\n", 20);
        return -1;
    }

    // a lot of bytes on the stack btw
    while ((nbytes = read(fin, buffer, 1024)) != 0) {
        write(fout, buffer, nbytes);
    } 

    close(fin);
    close(fout);

    return 0;
}