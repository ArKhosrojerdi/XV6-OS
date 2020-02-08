#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "spinlock.h"

int main()
{
    rwinit();
    rwtest(2);
    return 1;
}
