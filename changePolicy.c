#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[])
{
	int plcy;
	plcy = atoi(argv[1]);
    changePolicy(plcy);
	exit();
	return 1;
}