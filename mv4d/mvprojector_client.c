#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#include "my_mv4d_projector_ioctl.h"
#undef true
#ifndef true
#define true  1
#endif
#undef false
#ifndef false
#define false 0
#endif

/*

r   : Read
w   : Write
<c> : Command
<v> : Value

----

r <c>
w <c> <v>

 */


static const char* g_driverFile = "/dev/mvprojector";

static const char READC = 'r';
//static const char WRITEC = 'w';

static const int WRITEVALUE = 0;
static const int READVALUE = 1;

 struct Args
{
    int rw;
    int command;
    double value;
};


static void printHelp()
{
    printf("ERROR, missing parameters. Please use:\n"
            "r   : Read\n"
            "w   : Write\n"
            "<c> : Command\n"
            "<v> : Value\n"
            "\n\n"
            "mvprojector_linux_client r <c>\n"
            "OR\n"
            "mvprojector_linux_client w <c> <v>\n\n");
}


static int parseArgs(int argc, char* argv[], struct Args* outArgs)
{
    if (argc < 3)
    {
        printHelp();
        return 0;
    }
    printf("parseArgs:\n");

    char *p;
    printf("parseArgs0:\n");

    //outArgs->value = -1;

    printf("parseArgs:1\n");
    outArgs->command = strtoul(argv[2], &p, 16);
    outArgs->value = -1;
    printf("parseArgs:2 outArgs->command=0x%x\n",outArgs->command);
    outArgs->rw = (argv[1][0] == READC)? READVALUE : WRITEVALUE;

    if (outArgs->rw)
    {

    }
    else
    {
        if(argc < 4)
        {
            printHelp();
            return 0;
        }

        //outArgs->value =1;//wbl
        outArgs->value = atof(argv[3]);
        printf("outArgs->value=%f\n",outArgs->value);
    }
        printf("parseArgs:out\n");
    return true;
}

static int getWriteData(int command, double value, char *data)
{
    int intValue = (int)value;
#if 0
    switch(command)
    {
    case LASER_CMD_SET_CURRENT:
        printf("Set Laser Current:\n");
        data[0] = ((unsigned char*)(&intValue))[1];
        data[1] = ((unsigned char*)(&intValue))[1];
    data[2] = ((unsigned char*)(&intValue))[0];
     printf("Set Laser Current:data[0] =%d,data[1] =%d data[2] =%d\n",data[0],data[1] ,data[2]);
        return true;

    case COMMAND_SET_DELAY:
        printf("Set Laser Delay:\n");
        intValue = (int)(value*10 + 0.5);
        intValue = intValue & 0x0000ffff;
        data[0] = ((unsigned char*)(&intValue))[1];
        data[1] = ((unsigned char*)(&intValue))[0];
    data[2] = 0;
    printf("Set Laser Current:data[0] =%d,data[1] =%d data[2] =%d\n",data[0],data[1] ,data[2]);
        return true;

    case COMMAND_SET_PULSE_WIDTH:
        printf("Set Laser Pulse Width:\n");
        data[0] = ((unsigned char*)(&intValue))[1];
        data[1] = ((unsigned char*)(&intValue))[0];
    data[2] = 0;
    printf("Set Laser Current:data[0] =%d,data[1] =%d data[2] =%d\n",data[0],data[1] ,data[2]);
        return  true;
    }
    return false;
#endif
    data[0] = ((unsigned char*)(&intValue))[0];
//    data[1] = ((unsigned char*)(&intValue))[1];
  //      data[2] = ((unsigned char*)(&intValue))[2];
 printf("Set commd =0x%x,data[0] =0x%x\n",command,data[0]);
    return true;

};

int main(int argc, char* argv[])
{
    struct Args args ={
            .rw =1,
    .command=1,
    .value=1,
        };

    printf("\n\nmv4d projector client debug\n");


    if(!parseArgs(argc, argv, &args))
    {
        return 1;
    }

    printf("| RW : %s | Command : 0x%x | Value : %f |\n", (args.rw ? "read" : "write"), args.command, args.value);

    int fd = -1;
    if ((fd = open(g_driverFile, O_RDWR)) < 0)
    {
        printf("Failed to open the bus.\n");
        /* ERROR HANDLING; you can check errno to see what went wrong */
        return 1;
    }


    unsigned int ioctl_command = 0;
    int ret = 0;
    char data[1];
    int len = sizeof(data)/sizeof(data[0]);
#if 1
    if (args.rw)
    {
        ioctl_command = MVPROJECTOR_IOCTL_GET_COMMAND(args.command);
    }
    else
    {
        ioctl_command = MVPROJECTOR_IOCTL_SET_COMMAND(args.command);
        if(!getWriteData(args.command, args.value, data))
        {
            printf("ERROR in write data\n");
            return 1;
        }
        printf("write data : | 0x%x \n", data[0]);
    }
#endif
    ret = ioctl(fd, ioctl_command, data, len);
    printf("ioctl status = %d\n", ret);

    if (args.rw && !ret)
    {
        printf("read data : | 0x%x \n", data[0]);
    }
    //close(fd);

    printf("\n\n");
    return 0;
}
