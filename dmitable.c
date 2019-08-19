// Guilherme Magalhaes
// You are free to use this code under the GPL 2.0 license.

#include <stdio.h> // perror, printf
#include <stdlib.h> // exit
#include <string.h> // memcmp
#include <fcntl.h> // open
#include <unistd.h> // lseek, close, read

int pfd;

static void
seekEntry( unsigned int addr )
{
    if ( lseek( pfd, (off_t)addr, SEEK_SET ) < 0 ) {
        perror( "/dev/mem seek" );
        exit( 1 );
    }
}

static void
readEntry( void* entry, int size )
{
    if ( read( pfd, entry, size ) != size ) {
        perror( "readEntry" );
        exit( 1 );
    }
}

struct dmi_header
{
    unsigned char    type;
    unsigned char    length;
    unsigned int    handle;
};

static char * dmi_string(struct dmi_header *dm, unsigned char s)
{
    unsigned char *bp=(unsigned char *)dm;
    bp+=dm->length;
    if(!s)
        return "";
    s--;
    while(s>0 && *bp)
    {
        bp+=strlen((char *)bp);
        bp++;
        s--;
    }
    return (char *)bp;
}

static void decode(struct dmi_header *dm)
{
    unsigned char *data = (unsigned char *)dm;
    int n;
    int i;
    char s[64] = {0};
    
    switch(dm->type)
    {
        case 9:
            switch(data[5]) {
                case 0x01:
                case 0x02:
                strcpy(s,"other");
                break;
                case 0x03:
                strcpy(s,"ISA");
                break;
                case 0x06:
                strcpy(s,"PCI");
                break;
                case 0x07:
                strcpy(s,"PCMCIA");
                break;
                case 0x0A:
                strcpy(s,"Processor");
                break;
                case 0x0E:
                strcpy(s,"PCI 66 MHz");
                break;
                case 0x0F:
                strcpy(s,"AGP");
                break;                
                case 0x10:
                strcpy(s,"AGP 2x");
                break;
                case 0x11:
                strcpy(s,"AGP 4x");
                break;
            }
            printf("Slot designation: %s (%s)\n",dmi_string(dm,data[4]),s);
            switch(data[0x06]) {
                case 1:
                case 2:
                strcpy(s,"other");
                break;
                case 5:
                strcpy(s,"32 bit");
                break;
                case 6:
                strcpy(s,"64 bit");
                break;
            }
            printf("Slot data bus width: %s \n\n",s);
            break;
        case 10:
            n = (data[1] - 4) / 2;
            i = 4+2*(n-1);
            i = data[i];
            if (i & 0x80) i -= 0x80;
            switch(i) {
                case 0x00:
                case 0x01:
                strcpy(s,"other");
                break;
                case 0x03:
                strcpy(s,"video");
                break;
                case 0x05:
                strcpy(s,"ethernet");
                break;
                case 0x07:
                strcpy(s,"sound");
                break;
            }
            i = 5+2*(n-1);
            printf("On Board: %s (%s)\n\n",dmi_string(dm,data[i]),s);
            break;
        case 17:
            printf("Memory slot device locator: %s\n",dmi_string(dm,data[0x10]));
            printf("Memory slot bank locator: %s\n",dmi_string(dm,data[0x11]));
            switch(data[0x0E]) {
                case 0x03:
                strcpy(s,"SIMM");
                break;
                case 0x09:
                strcpy(s,"DIMM");
                break;
                default:
                strcpy(s,"other");
                break;
            }
            printf("Memory device Form Factor: %s\n",s);
            switch(data[0x12]) {
                case 0x03:
                strcpy(s,"DRAM");
                break;
                case 0x06:
                strcpy(s,"SRAM");
                break;
                case 0x07:
                strcpy(s,"RAM");
                break;
                case 0x08:
                strcpy(s,"ROM");
                break;
                case 0x09:
                strcpy(s,"FLASH");
                break;
                case 0x0A:
                strcpy(s,"EEPROM");
                break;
                case 0x0C:
                strcpy(s,"EPROM");
                break;
                case 0x0F:
                strcpy(s,"SDRAM");
                break;
                default:
                strcpy(s,"other");
                break;
            }
            printf("Memory device type: %s\n",s);
            memcpy(&i,&data[0x0A],2);
            printf("Memory device data width: %i\n",i);
            memcpy(&i,&data[0x0C],2);
            if (i & 8000)
                i = (i-80000)/1024;
            printf("Memory device size: %i MB\n\n",i);
            break;
        case 38:
            printf("IPMI\n");
            switch(data[4]) {
                case 0x01:
                strcpy(s,"kcs");
                break;
                case 0x02:
                strcpy(s,"smic");
                break;
                case 0x03:
                strcpy(s,"bt");
                break;
                case 0x00:
                default:
                strcpy(s,"unknown");
                break;
            }
            printf("Interface type: %s\n",s);
            memcpy(&i,&data[0x08],2);
            printf("Interface base address: %i\n",i);
            printf("Interface IRQ: %i\n",data[0x11]);
            break;
        default:
            printf("Other Type: %u\n",
                dm->type);
    }
}

static int dmi_table(unsigned int base, int len, int num)
{
    struct dmi_header *dm;
    unsigned char buf[8*0x400];
    unsigned int data;
    unsigned int ini;
    int i=0;
    
    if(base==0)
        return -1;

    data = base;

    while(i<num && data-base+sizeof(struct dmi_header)<=len)
    {
        ini = data;
        seekEntry(data);
        readEntry(buf, sizeof(struct dmi_header));
        dm = (struct dmi_header *) buf;
        
        data += dm->length;
        
        if(data-base<len-1) {
            memset(buf,0,8*0x400);
            seekEntry(ini);
            readEntry(buf, data - ini);
            decode((struct dmi_header *)buf);
        }
        
        seekEntry(data);
        readEntry(buf, 2);
        while(data-base<len-1 && (buf[0] || buf[1])) {
            data++;
            seekEntry(data);
            readEntry(buf, 2);
        }
        data+=2;
        i++;
    }
    
    return 0;
}

int dmi_checksum(unsigned char *buf)
{
    unsigned char sum=0;
    int a;
    
    for(a=0; a<15; a++)
        sum+=buf[a];
    
    return (sum==0);
}

static int dmi_iterate()
{
    unsigned char buf[15];
    unsigned long fp = 0xF0000;

    while(fp < 0xFFFFF)
    {
        seekEntry(fp);
        readEntry(buf, 15);
        if(memcmp(buf, "_DMI_", 5)==0 && dmi_checksum(buf))
        {
            unsigned short num=buf[13]<<8|buf[12];
            unsigned short len=buf[7]<<8|buf[6];
            unsigned long base=buf[11]<<24|buf[10]<<16|buf[9]<<8|buf[8];
            
            if(buf[14]!=0)
                printf("DMI %d.%d present.\n",buf[14]>>4, buf[14]&0x0F);
            else
                printf("DMI present.\n");
            printf("%d structures occupying %d bytes.\n",num, len);
            printf("DMI table at 0x%08lX.\n",base);
            
            if(dmi_table(base,len, num)==0)
                return 0;
        }
        fp+=16;
    }
    return -1;
}

int main()
{
    if ((pfd = open("/dev/mem", O_RDONLY)) < 0 ) {
        perror( "mem open" );
        exit( 1 );
    }
    
    dmi_iterate();
    
    close(pfd);
    
    return 0;
}
