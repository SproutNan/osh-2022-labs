#include <stdio.h>

int main(){
	char buf[100];
	printf("Return value of syscall is %d.\nContent of buf is %s", syscall(548, buf, 100), buf);
	printf("Return value of syscall is %d.\n", syscall(548, buf, 5), buf);
}
