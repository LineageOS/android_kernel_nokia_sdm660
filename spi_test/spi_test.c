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

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
int read_filesize =0;
static void pabort(const char *s)
{
    perror(s);
    abort();
}





/*

Write Enable: 0x06
Write Disable: 0x04
Volatile SR Write Enable: 0x50
Read Status Register 1: 0x05 (S7-S0)
Read Status Register 2: 0x35 (S15-S8)
Read Status Register 3: 0x15 (S23-S16)
Write Status Register 1: 0x01 S7-S0
Write Status Register 2: 0x31 S15-S8
Write Status Register 3: 0x11 S23-S16




*/

typedef struct spi_flash
{
    int fd;
    uint16_t delay;
    uint32_t speed;
    uint8_t bits;
    uint8_t mode;
    char device[128];
}spi_flash;

spi_flash flash=
{
	.device = "/dev/spidev8.0",
    .mode = 0,
    .bits = 8,
    .speed = 960000,
    .delay = 0,
	
};


uint8_t write_buf[32]={
0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,	 
0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,	 
};

int flash_write_enable(spi_flash *flash_ptr);
int flash_write_disable(spi_flash *flash_ptr);
int flash_read_sr1(spi_flash *flash_ptr,uint8_t *sr1_ptr);
int flash_read_sr2(spi_flash *flash_ptr,uint8_t *sr2_ptr);
int flash_read_sr3(spi_flash *flash_ptr,uint8_t *sr3_ptr);
int flash_write_sr1(spi_flash *flash_ptr,uint8_t sr1);
int flash_write_sr2(spi_flash *flash_ptr,uint8_t sr2);
int flash_write_sr3(spi_flash *flash_ptr,uint8_t sr3);
int flash_read_bytes(spi_flash *flash_ptr,uint32_t addr,uint8_t *data_ptr,int data_size);
int flash_page_program(spi_flash *flash_ptr,uint32_t addr,uint8_t *data_ptr,int data_size);
int flash_sector_erase(spi_flash *flash_ptr,uint32_t addr);
int flash_block_erase_32k(spi_flash *flash_ptr,uint32_t addr);
int flash_block_erase_64k(spi_flash *flash_ptr,uint32_t addr);
int flash_chip_erase(spi_flash *flash_ptr);
int flash_device_id(spi_flash *flash_ptr,uint8_t *mid_ptr,uint16_t *did_ptr);
void flash_check_not_busy(spi_flash *flash_ptr);
int write_file_to_flash(spi_flash *flash_ptr,char *filename,uint32_t offset);
int verify_file_in_flash(spi_flash *flash_ptr, char *filename, uint32_t offset);
int dump_file_from_flash(spi_flash *flash_ptr, char *filename, uint32_t offset,uint32_t file_size);

int flash_write_enable(spi_flash *flash_ptr)
{
    int ret=0;
    uint8_t tx[] = {
        0x06
    };
    uint8_t rx[ARRAY_SIZE(tx)] = {0, };
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx),
        .delay_usecs = flash_ptr->delay,
        .speed_hz = flash_ptr->speed,
        .bits_per_word = flash_ptr->bits,
    };

    ret = ioctl(flash_ptr->fd, SPI_IOC_MESSAGE(1), &tr);
    return ret;
       
}

int flash_write_disable(spi_flash *flash_ptr)
{
    int ret=0;
    uint8_t tx[] = {
        0x04
    };
    uint8_t rx[ARRAY_SIZE(tx)] = {0, };
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx),
        .delay_usecs = flash_ptr->delay,
        .speed_hz = flash_ptr->speed,
        .bits_per_word = flash_ptr->bits,
    };

    ret = ioctl(flash_ptr->fd, SPI_IOC_MESSAGE(1), &tr);
    return ret;
       
}

int flash_read_sr1(spi_flash *flash_ptr,uint8_t *sr1_ptr)
{
       int ret=0;
    uint8_t tx[] = {
        0x05,0x00
    };
    uint8_t rx[ARRAY_SIZE(tx)] = {0, };
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx),
        .delay_usecs = flash_ptr->delay,
        .speed_hz = flash_ptr->speed,
        .bits_per_word = flash_ptr->bits,
    };

    ret = ioctl(flash_ptr->fd, SPI_IOC_MESSAGE(1), &tr);
    *sr1_ptr=rx[1];
    return ret; 
    
}

int flash_read_sr2(spi_flash *flash_ptr,uint8_t *sr2_ptr)
{
    int ret=0;
    uint8_t tx[] = {
        0x35,0x00
    };
    uint8_t rx[ARRAY_SIZE(tx)] = {0, };
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx),
        .delay_usecs = flash_ptr->delay,
        .speed_hz = flash_ptr->speed,
        .bits_per_word = flash_ptr->bits,
    };

    ret = ioctl(flash_ptr->fd, SPI_IOC_MESSAGE(1), &tr);
    *sr2_ptr=rx[1];
    return ret; 
    
}

int flash_read_sr3(spi_flash *flash_ptr,uint8_t *sr3_ptr)
{
    int ret=0;
    uint8_t tx[] = {
        0x15,0x00
    };
    uint8_t rx[ARRAY_SIZE(tx)] = {0, };
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx),
        .delay_usecs = flash_ptr->delay,
        .speed_hz = flash_ptr->speed,
        .bits_per_word = flash_ptr->bits,
    };

    ret = ioctl(flash_ptr->fd, SPI_IOC_MESSAGE(1), &tr);
	flash_check_not_busy(flash_ptr);
    *sr3_ptr=rx[1];
    return ret; 
    
}

int flash_write_sr1(spi_flash *flash_ptr,uint8_t sr1)
{
    int ret=0;
    uint8_t tx[] = {
        0x01,sr1,
    };
    uint8_t rx[ARRAY_SIZE(tx)] = {0, };
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx),
        .delay_usecs = flash_ptr->delay,
        .speed_hz = flash_ptr->speed,
        .bits_per_word = flash_ptr->bits,
    };
    ret = flash_write_enable(flash_ptr);
    ret = ioctl(flash_ptr->fd, SPI_IOC_MESSAGE(1), &tr);
	flash_check_not_busy(flash_ptr);
    return ret;    
}

int flash_write_sr2(spi_flash *flash_ptr,uint8_t sr2)
{
       int ret=0;
    uint8_t tx[] = {
        0x31,sr2
    };
    uint8_t rx[ARRAY_SIZE(tx)] = {0, };
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx),
        .delay_usecs = flash_ptr->delay,
        .speed_hz = flash_ptr->speed,
        .bits_per_word = flash_ptr->bits,
    };
    ret = flash_write_enable(flash_ptr);
    ret = ioctl(flash_ptr->fd, SPI_IOC_MESSAGE(1), &tr);
	flash_check_not_busy(flash_ptr);
    return ret;  
}

int flash_write_sr3(spi_flash *flash_ptr,uint8_t sr3)
{
       int ret=0;
    uint8_t tx[] = {
        0x11,sr3
    };
    uint8_t rx[ARRAY_SIZE(tx)] = {0, };
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx),
        .delay_usecs = flash_ptr->delay,
        .speed_hz = flash_ptr->speed,
        .bits_per_word = flash_ptr->bits,
    };
    ret = flash_write_enable(flash_ptr);
    ret = ioctl(flash_ptr->fd, SPI_IOC_MESSAGE(1), &tr);
    return ret; 
}

int flash_read_bytes(spi_flash *flash_ptr,uint32_t addr,uint8_t *data_ptr,int data_size)
{
    int ret=0;
    uint8_t *tx=(uint8_t *)calloc(data_size+4,sizeof(uint8_t));
    uint8_t *rx=(uint8_t *)calloc(data_size+4,sizeof(uint8_t));
    tx[0] = 0x03;
    tx[1] = (addr>>16)&0xFF;
    tx[2] = (addr>>8)&0xFF;
    tx[3] = (addr)&0xFF;
    
    
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = data_size+4,
        .delay_usecs = flash_ptr->delay,
        .speed_hz = flash_ptr->speed,
        .bits_per_word = flash_ptr->bits,
    };

    ret = ioctl(flash_ptr->fd, SPI_IOC_MESSAGE(1), &tr);
    memcpy(data_ptr,rx+4,data_size);
    free(tx);
    free(rx);
    return ret;     
}

int flash_page_program(spi_flash *flash_ptr,uint32_t addr,uint8_t *data_ptr,int data_size)
{
    int ret=0;
    if(data_size>256)
        return -1;
    
    uint8_t *tx=(uint8_t *)calloc(data_size+4,sizeof(uint8_t));
    uint8_t *rx=(uint8_t *)calloc(data_size+4,sizeof(uint8_t));
    tx[0] = 0x02;
    tx[1] = (addr>>16)&0xFF;
    tx[2] = (addr>>8)&0xFF;
    tx[3] = (addr)&0xFF;
    memcpy(tx+4,data_ptr,data_size);
    
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = data_size+4,
        .delay_usecs = flash_ptr->delay,
        .speed_hz = flash_ptr->speed,
        .bits_per_word = flash_ptr->bits,
    };
    ret = flash_write_enable(flash_ptr);
    ret = ioctl(flash_ptr->fd, SPI_IOC_MESSAGE(1), &tr);
	flash_check_not_busy(flash_ptr);
    free(tx);
    free(rx);
    return ret;     
}

int flash_sector_erase(spi_flash *flash_ptr,uint32_t addr)
{
    int ret=0;
    uint8_t tx[]=
    {
        0x20,
        (addr>>16)&0xFF,
        (addr>>8)&0xFF,
        (addr)&0xFF
    };

    uint8_t rx[ARRAY_SIZE(tx)] = {0, };
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len =ARRAY_SIZE(tx),
        .delay_usecs = flash_ptr->delay,
        .speed_hz = flash_ptr->speed,
        .bits_per_word = flash_ptr->bits,
    };
    ret = flash_write_enable(flash_ptr);
    ret = ioctl(flash_ptr->fd, SPI_IOC_MESSAGE(1), &tr);
    flash_check_not_busy(flash_ptr);
    return ret;     
}

int flash_block_erase_32k(spi_flash *flash_ptr,uint32_t addr)
{
    int ret=0;
    uint8_t tx[]=
    {
        0x52,
        (addr>>16)&0xFF,
        (addr>>8)&0xFF,
        (addr)&0xFF
    };

    uint8_t rx[ARRAY_SIZE(tx)] = {0, };
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len =ARRAY_SIZE(tx),
        .delay_usecs = flash_ptr->delay,
        .speed_hz = flash_ptr->speed,
        .bits_per_word = flash_ptr->bits,
    };
    ret = flash_write_enable(flash_ptr);
    ret = ioctl(flash_ptr->fd, SPI_IOC_MESSAGE(1), &tr);
    flash_check_not_busy(flash_ptr);
    return ret;     
}

int flash_block_erase_64k(spi_flash *flash_ptr,uint32_t addr)
{
    int ret=0;
    uint8_t tx[]=
    {
        0xD8,
        (addr>>16)&0xFF,
        (addr>>8)&0xFF,
        (addr)&0xFF
    };

    uint8_t rx[ARRAY_SIZE(tx)] = {0, };
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len =ARRAY_SIZE(tx),
        .delay_usecs = flash_ptr->delay,
        .speed_hz = flash_ptr->speed,
        .bits_per_word = flash_ptr->bits,
    };
    ret = flash_write_enable(flash_ptr);
	
	
    ret = ioctl(flash_ptr->fd, SPI_IOC_MESSAGE(1), &tr);
    flash_check_not_busy(flash_ptr);
    return ret;     
}

int flash_chip_erase(spi_flash *flash_ptr)
{
    int ret=0;
    uint8_t tx[] = {
        0x60
    };
    uint8_t rx[ARRAY_SIZE(tx)] = {0, };
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx),
        .delay_usecs = flash_ptr->delay,
        .speed_hz = flash_ptr->speed,
        .bits_per_word = flash_ptr->bits,
    };
    ret = flash_write_enable(flash_ptr);
    ret = ioctl(flash_ptr->fd, SPI_IOC_MESSAGE(1), &tr);
	flash_check_not_busy(flash_ptr);
    return ret;
       
}

int flash_device_id(spi_flash *flash_ptr,uint8_t *mid_ptr,uint16_t *did_ptr)
{
    int ret=0;
    uint8_t tx[] = {
        0x9F,0x00,0x00,0x00,
    };
    uint8_t rx[ARRAY_SIZE(tx)] = {0, };
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx),
        .delay_usecs = flash_ptr->delay,
        .speed_hz = flash_ptr->speed,
        .bits_per_word = flash_ptr->bits,
    };

    ret = ioctl(flash_ptr->fd, SPI_IOC_MESSAGE(1), &tr);
    *mid_ptr=rx[1];
    *did_ptr=(rx[2]<<8)|rx[3];
    return ret;
       
}
//check WIP
void flash_check_not_busy(spi_flash *flash_ptr)
{
	int i;
	uint8_t sr1;
	do{
		for(i=0; i<10; i++)
		{
			__asm volatile("nop");
		}
		flash_read_sr1(flash_ptr,&sr1);
	}
	while( (sr1 & 0x1) == 1 );
}


int flash_erase(spi_flash *flash_ptr,uint32_t addr, uint32_t len)
{

    uint32_t temp_addr;

    if (addr + len > 0x1FFFFF)
    {
        return -1;
    }

	for(temp_addr=addr ; temp_addr< (addr + len) ; temp_addr+=1024*64)
	{
			flash_block_erase_64k(flash_ptr,temp_addr);
	}

    return 0;
}

int flash_init(spi_flash *flash_ptr)
{
	int ret =0 ;
	uint8_t sr1,sr2,sr3;
	ret=flash_write_sr1(flash_ptr,0x00);
	ret=flash_write_sr2(flash_ptr,0x00);
	ret=flash_write_sr3(flash_ptr,0x00);
	
	flash_read_sr1(flash_ptr,&sr1);
	flash_read_sr2(flash_ptr,&sr2);
	flash_read_sr3(flash_ptr,&sr3);
	
	printf("sr1 = 0x%x\n",sr1);
	printf("sr2 = 0x%x\n",sr2);
	printf("sr3 = 0x%x\n",sr3);
	
	return ret;
	
}

void print_usage(char *file)
{
    printf("%s  name\n", file);
    printf("%s addr\n", file);
}

#if 0
static void print_usage(const char *prog)
{
    printf("Usage: %s [-DsbdlHOLC3]\n", prog);
    puts("  -D --device   device to use (default /dev/spidev1.1)\n"
         "  -s --speed    max speed (Hz)\n"
         "  -d --delay    delay (usec)\n"
         "  -b --bpw      bits per word \n"
         "  -l --loop     loopback\n"
         "  -H --cpha     clock phase\n"
         "  -O --cpol     clock polarity\n"
         "  -L --lsb      least significant bit first\n"
         "  -C --cs-high  chip select active high\n"
         "  -3 --3wire    SI/SO signals shared\n");
    exit(1);
}
#endif
static void parse_opts(spi_flash *flash_ptr,int argc, char *argv[])
{
    while (1) {
        static const struct option lopts[] = {
            { "device",  1, 0, 'D' },
            { "speed",   1, 0, 's' },
            { "delay",   1, 0, 'd' },
            { "bpw",     1, 0, 'b' },
            { "loop",    0, 0, 'l' },
            { "cpha",    0, 0, 'H' },
            { "cpol",    0, 0, 'O' },
            { "lsb",     0, 0, 'L' },
            { "cs-high", 0, 0, 'C' },
            { "3wire",   0, 0, '3' },
            { "no-cs",   0, 0, 'N' },
            { "ready",   0, 0, 'R' },
            { NULL, 0, 0, 0 },
        };
        int c;

        c = getopt_long(argc, argv, "D:s:d:b:lHOLC3NR", lopts, NULL);

        if (c == -1)
            break;

        switch (c) {
        case 'D':
            strcpy(flash_ptr->device,optarg);
            break;
        case 's':
            flash_ptr->speed = atoi(optarg);
            break;
        case 'd':
            flash_ptr->delay = atoi(optarg);
            break;
        case 'b':
            flash_ptr->bits = atoi(optarg);
            break;
        case 'l':
            flash_ptr->mode |= SPI_LOOP;
            break;
        case 'H':
            flash_ptr->mode |= SPI_CPHA;
            break;
        case 'O':
            flash_ptr->mode |= SPI_CPOL;
            break;
        case 'L':
            flash_ptr->mode |= SPI_LSB_FIRST;
            break;
        case 'C':
            flash_ptr->mode |= SPI_CS_HIGH;
            break;
        case '3':
            flash_ptr->mode |= SPI_3WIRE;
            break;
        case 'N':
            flash_ptr->mode |= SPI_NO_CS;
            break;
        case 'R':
            flash_ptr->mode |= SPI_READY;
            break;
        default:
            print_usage(argv[0]);
            break;
        }
    }
}

int write_file_to_flash(spi_flash *flash_ptr,char *filename,uint32_t offset)
{

    //1. open the device
    FILE *fp=NULL;
    int filesize = 0;
    int write_filesize = 0;
    uint32_t start_addr = 0;
    uint32_t addr = 0;
    uint8_t *buf;
    uint8_t *buf_bk;
    int ret=0;
    fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        printf("Can not open file\n");
        return -1;
    }
    //2. check the file size
    fseek(fp, 0, SEEK_END); 
    filesize = ftell(fp); 
    fseek(fp, 0, SEEK_SET);

    if ((filesize % 4) != 0)
    {
        write_filesize = filesize + 4 - (filesize % 4);
    }
    else
    {
        write_filesize = filesize;
    }


    read_filesize =filesize;
    printf("Flash Erease : 0x%08x , write_filesize=0x%08x,filesize=0x%08x\n", offset, write_filesize,filesize);
    //3. read file to buffer
    buf = (uint8_t*)malloc(write_filesize);
    memset(buf, 0xFF, write_filesize);
    buf_bk = (uint8_t*)malloc(write_filesize);
    memset(buf_bk, 0xFF, write_filesize);
    fread(buf, 1,filesize, fp);
    fclose(fp);
    
    //4. dump the original sector,ignore for now
    

    //5. Erease the flash secort
    printf("Flash Erease : 0x%08x , 0x%08x\n", offset, write_filesize);
    flash_erase( flash_ptr, offset,  write_filesize);





    //6. Program the flash
    printf("Flash Program : 0x%08x , 0x%08x\n", offset, write_filesize);
    int program_num = write_filesize / 256;
    int last_size = write_filesize % 256;
    start_addr=0;
    addr=offset;
    ret=0;
    for (int j = 0; j < program_num; j++)
    {
        ret = flash_page_program(flash_ptr, addr, buf+start_addr,256);
	addr+=256;
	start_addr+=256;
    }
    if(last_size>0)
    {
        ret = flash_page_program(flash_ptr, addr, buf+start_addr,last_size);
    }
 


    //7. verify the flash
    memset(buf_bk, 0x00, write_filesize);
    start_addr=0;
    addr=offset;
    ret=0;
    for (int j = 0; j < program_num; j++)
    {
        ret = flash_read_bytes(flash_ptr, addr, buf_bk+start_addr,256);
	addr+=256;
	start_addr+=256;	
    }
    if(last_size>0)
    {
	ret = flash_read_bytes(flash_ptr, addr, buf_bk+start_addr,last_size);
    }


    for (int i = 0; i < write_filesize; i++)
    {

        if ((buf)[i] != (buf_bk)[i])
        {

            printf("Flash Verify Failed : [0x%08x] :0x%08x(File) != 0x%08x(Flash)\n", offset + i , (buf)[i], (buf_bk)[i]);
            free(buf);
            free(buf_bk);
            return -4;
        }

    }
	
    //8.free
    printf("Flash Program Success\n");

    free(buf);
    free(buf_bk);

    return 0;

}


int verify_file_in_flash(spi_flash *flash_ptr, char *filename, uint32_t offset)
{

     //1. open the device
    FILE *fp=NULL;
    int filesize = 0;
    int write_filesize = 0;
    uint32_t start_addr = 0;
    uint32_t addr = 0;
    uint8_t *buf;
    uint8_t *buf_bk;
    int ret=0;
    fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        printf("Can not open file\n");
        return -1;
    }
    //2. check the file size
    fseek(fp, 0, SEEK_END); 
    filesize = ftell(fp); 
    fseek(fp, 0, SEEK_SET);

    if ((filesize % 4) != 0)
    {
        write_filesize = filesize + 4 - (filesize % 4);
    }
    else
    {
        write_filesize = filesize;
    }


    //3. read file to buffer
    buf = (uint8_t*)malloc(write_filesize);
    memset(buf, 0xFF, write_filesize);
    buf_bk = (uint8_t*)malloc(write_filesize);
    memset(buf_bk, 0xFF, write_filesize);
    fread(buf, 1,filesize, fp);
    fclose(fp);
	
	
    int program_num = write_filesize / 256;
    int last_size = write_filesize % 256;

    memset(buf_bk, 0x00, write_filesize);
    start_addr=0;
    addr=offset;
    ret=0;
    for (int j = 0; j < program_num; j++)
    {
        ret = flash_read_bytes(flash_ptr, addr, buf_bk+start_addr,256);
	addr+=256;
	start_addr+=256;	
    }
    if(last_size>0)
    {
	ret = flash_read_bytes(flash_ptr, addr, buf_bk+start_addr,last_size);
    }


    for (int i = 0; i < write_filesize; i++)
    {

        if ((buf)[i] != (buf_bk)[i])
        {

            printf("Flash Verify Failed : [0x%08x] :0x%08x(File) != 0x%08x(Flash)\n", offset + i , (buf)[i], (buf_bk)[i]);
            free(buf);
            free(buf_bk);
            return -4;
        }

    }
	
    printf("Flash Verify Success\n");



    free(buf);
    free(buf_bk);
    return 0;



}


int dump_file_from_flash(spi_flash *flash_ptr, char *filename, uint32_t offset,uint32_t file_size)
{
    FILE *fp=NULL;
    uint8_t *buf;
    int ret = 0;
    uint32_t start_addr = 0;
    uint32_t addr = 0;	
    if (offset + file_size > 0x1FFFFF)
    {
        return -1;
    }
	
    fp = fopen(filename, "wb");
    if (fp == NULL)
    {
        printf("Can not open file\n");
        return -1;
    }
    buf = (uint8_t*)malloc(file_size);
    memset(buf, 0xFF, file_size);
    
    int program_num = file_size / 256;
    int last_size = file_size % 256;
  

    memset(buf, 0x00, file_size);
    start_addr=0;
    addr=offset;
    ret=0;
    for (int j = 0; j < program_num; j++)
    {
        ret = flash_read_bytes(flash_ptr, addr, buf+start_addr,256);
	addr+=256;
	start_addr+=256;	
    }
    if(last_size>0)
    {
	ret = flash_read_bytes(flash_ptr, addr, buf+start_addr,last_size);
    }
    
    fwrite(buf,file_size,1,fp);

 
    printf("Flash Dump Success\n"); 
    free(buf);
    fclose(fp);
    return 0;
}

int main(int argc, char *argv[])
{

    int ret = 0;
    if ((argc != 3))
    {
        print_usage(argv[0]);
        return -1;
    }
    parse_opts(&flash,argc, argv);

    flash.fd = open(flash.device, O_RDWR);
    if (flash.fd < 0)
        pabort("can't open device");


        /*
     * spi mode
     */
    ret = ioctl(flash.fd, SPI_IOC_WR_MODE, &flash.mode);
    if (ret == -1)
        pabort("can't set spi mode");

    ret = ioctl(flash.fd, SPI_IOC_RD_MODE, &flash.mode);
    if (ret == -1)
        pabort("can't get spi mode");

    /*
     * bits per word
     */
    ret = ioctl(flash.fd, SPI_IOC_WR_BITS_PER_WORD, &flash.bits);
    if (ret == -1)
        pabort("can't set bits per word");

    ret = ioctl(flash.fd, SPI_IOC_RD_BITS_PER_WORD, &flash.bits);
    if (ret == -1)
        pabort("can't get bits per word");

    /*
     * max speed hz
     */
    ret = ioctl(flash.fd, SPI_IOC_WR_MAX_SPEED_HZ, &flash.speed);
    if (ret == -1)
        pabort("can't set max speed hz");

    ret = ioctl(flash.fd, SPI_IOC_RD_MAX_SPEED_HZ, &flash.speed);
    if (ret == -1)
        pabort("can't get max speed hz");

    printf("spi mode: %d\n", flash.mode);
    printf("bits per word: %d\n", flash.bits);
    printf("max speed: %d Hz (%d KHz)\n", flash.speed, flash.speed/1000);

    uint8_t mid;
    uint16_t did;
    struct timeval tvstart,tvend;
unsigned int addr;
char name[100];
    flash_device_id(&flash,&mid,&did);
    printf("Manufacturer ID: 0x%x ,Device ID : 0x%x\n", mid, did);
#if 1
    flash_init(&flash);
    gettimeofday( &tvstart, NULL);
	memcpy(name,argv[1],sizeof(argv[1]));
       addr = strtoul(argv[2], NULL, 0);
            printf("write , name= %s, addr = 0x%02x\n", name,addr);
			
  char bin_name[100] = { 0 };
  snprintf(bin_name, sizeof(bin_name), "%s%s.%s", "/system/bin/",
      name,"bin");
      printf("write , bin_name= %s, addr = 0x%02x\n", bin_name,addr);
    write_file_to_flash(&flash,bin_name,addr);
    gettimeofday( &tvend, NULL);
    printf("Program Time :%ld s\n",(tvend.tv_sec-tvstart.tv_sec)+(tvend.tv_usec-tvstart.tv_usec)/1000000);


    gettimeofday( &tvstart, NULL);
    verify_file_in_flash(&flash, bin_name, addr);
    gettimeofday( &tvend, NULL);
    printf("Verify Time :%ld s\n",(tvend.tv_sec-tvstart.tv_sec)+(tvend.tv_usec-tvstart.tv_usec)/1000000);

    gettimeofday( &tvstart, NULL);
    printf("read_fileseze= :0x%08x addr=0x%02x\n",read_filesize,addr);
    dump_file_from_flash(&flash,"/system/bin/REF_HD_BK", addr,read_filesize);
    gettimeofday( &tvend, NULL);
    printf("Dump Time :%ld s\n",(tvend.tv_sec-tvstart.tv_sec)+(tvend.tv_usec-tvstart.tv_usec)/1000000);
#endif

#if 0
	flash_block_erase_64k(&flash,0x00);
	
    uint8_t data_buf[32];
    flash_read_bytes(&flash,0x00,data_buf,32);
	for(int i=0;i<32;i++)
	{
		if(i%8==7)
		{
		    printf("0x%02x \n",data_buf[i]); 			
		}
		else
		{
		    printf("0x%02x ",data_buf[i]);  
		}
	}	
	printf("\n"); 	
	
	flash_page_program(&flash,0x00,write_buf,32);

    flash_read_bytes(&flash,0x00,data_buf,32);
	for(int i=0;i<32;i++)
	{
		if(i%8==7)
		{
		    printf("0x%02x \n",data_buf[i]); 			
		}
		else
		{
		    printf("0x%02x ",data_buf[i]);  
		}
	}	
#else
	
#endif
    close(flash.fd);

    return ret;
}
