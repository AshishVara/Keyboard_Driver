#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

char key;

void main()
{
	int fd = 0;
	int i = 0,n = 0;
        fd = open("/dev/vkbdev",O_RDWR);
        if(fd < 0)
        {
                printf("failed to open file :%d\n",fd);
		exit(0);
        }
	while(1)
	{
		n = read(fd,&key,1);
	        if((n > 0) && (key != 0))
		{
//			for(i = 0; i < n;i++)
//			{
				printf("%d and %c\n",key,key);
				key = 0;
//				if((i & 0xf) == 0)
//				{
//					printf("\n");
//				}
//			}
//			memset(buff,0, 4096);
		}
		sleep(1);
	}
        close(fd);
}
