#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[])
{
	fork();
	wait();
	fork();
	wait();

	getChildren(3);

	return 1;
}