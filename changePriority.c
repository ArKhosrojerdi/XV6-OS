#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[])
{
	int priority;

	// if (argc < 3)
	// {
	// 	printf(2, "Usage: nice pid priority\n");
	// 	exit();
	// }
	priority = atoi(argv[1]);
	if (priority < 0 || priority > 5)
	{
		printf(2, "Invalid priority (0-5)!\n");
		exit();
	}
	changePriority(priority);
	cps();
	exit();
}