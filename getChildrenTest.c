#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[])
{
	int pid = atoi(argv[1]);
	fork();
	wait();
	fork();
	wait();
	fork();
	wait();
	// fork();
	// wait();
	// fork();
	// wait();

	getChildren(pid);

	exit();
	return 1;
}