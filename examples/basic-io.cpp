#include "conjure/io/interfaces.h"
#include <unistd.h>
#include <stdio.h>

using namespace conjure; 

int main() {
    int fd = io::Open("./test_file.txt", O_CREAT | O_WRONLY);
    if (fd == -1) {
        puts("error open file");
        return 1;
    }
    puts("open success");
    int a = io::Write(fd, "testtesttest");
    if (a > 0) {
        puts("write success");
    } else {
        puts("write error");
    }
}
