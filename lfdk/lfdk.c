/*
 * LFDK - Linux Firmware Debug Kit
 * File: lfdk.c
 *
 * Copyright (C) 2006 - 2010 Merck Hung <merckhung@gmail.com>
 * Copyright (C) 2013 Desmond Wu <wkunhui@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>

#include <ncurses.h>
#include <panel.h>

//#include "../lfdd/lfdd.h"
#include "lfdk.h"

#include<sys/mman.h>
#include <sys/io.h>


static const char *progname;
static BasePanel BaseScreen;


int x = 0, y = 0;
int curr_index = 0, last_index;
int input = 0;
unsigned int counter = 0;
int ibuf;
char wbuf;
int func = PCI_DEVICE_FUNC;
int maxpcibus = 255;
char pciname[ LFDK_MAX_PATH ];
char enter_mem = 0;

unsigned int  io_int_get(unsigned int u32addr)
{
	unsigned int idata = 0;
	ioperm(u32addr,4,1);
	idata = inl(u32addr);
	return idata;
}
void  io_int_set(unsigned int u32addr, unsigned int u32data)
{
	ioperm(u32addr,4, 1);
	outl(u32data, u32addr );
}
void io_byte_set(unsigned int u32addr, unsigned char u8data)
{
	ioperm(u32addr, 1, 1);
	outb(u8data, u32addr );
}

unsigned int pci_int_get(unsigned char u8bus, unsigned char u8dev, unsigned char u8fun, unsigned char u8reg)
{
	unsigned int   u32addr=0,u32data=0;
	u32addr = u8reg | (u8fun << 8) | (u8dev << 11) | (u8bus << 16) | 0x80000000;
	
	io_int_set(0x0cf8, u32addr); 	
	u32data = io_int_get((unsigned short int) 0x0cfc); 	
	
	return u32data;
}

void pci_int_set(unsigned char u8bus, unsigned char u8dev, unsigned char u8fun, unsigned char u8reg, unsigned int u32data)
{
	unsigned int   u32addr=0;
	u32addr = u8reg | (u8fun << 8) | (u8dev << 11) | (u8bus << 16) | 0x80000000;
	
	io_int_set(0x0cf8, u32addr); 
	io_int_set(0x0cfc, u32data); 
	
}
void pci_byte_set(unsigned char u8bus, unsigned char u8dev, unsigned char u8fun, unsigned char u8reg, unsigned char u8data)

{
	unsigned int   u32addr=0,u32data=0;
	u32addr = u8reg | (u8fun << 8) | (u8dev << 11) | (u8bus << 16) | 0x80000000;
		
	io_int_set(0x0cf8, u32addr); 
	u32data = io_int_get((unsigned short int) 0x0cfc); 
	u32data = (u32data&0xffffff00) | u8data;
	io_int_set(0x0cfc, u32data); 
}


int mem_int_set( unsigned int u32addr, unsigned int u32data )
{
	int	fd = 0;
	unsigned int	*pu32mmap = NULL;	

	fd = open("/dev/mem", O_RDWR);
	if ( fd == -1 )
	{
		return -1;
	}

	pu32mmap = (unsigned int *)mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, u32addr&0xfffff000);
	if(pu32mmap == (void *) -1)
	{
		if (fd) close(fd);
		return -1;
	}	

	pu32mmap[(u32addr&0xfff)>>2] = u32data;	// Get data from register
	munmap(pu32mmap, 0xfff);
	if (fd) close(fd);
	return 0;
	
}


int mem_byte_set( unsigned int u32addr, unsigned char u8data ) 
{
	unsigned int u32buff=0;
	int	fd = 0;
	unsigned int	*pu32mmap = NULL;	

	fd = open("/dev/mem", O_RDWR);
	if ( fd == -1 )
	{
		return -1;
	}

	pu32mmap = (unsigned int *)mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, u32addr&0xfffff000);
	if(pu32mmap == (void *) -1)
	{
		if (fd) close(fd);

		return -1;
	}	

	u32buff = pu32mmap[(u32addr&0xfff)>>2];
	*(((unsigned char*)&u32buff) + (unsigned char)(u32addr&0x3)) = u8data;
	pu32mmap[(u32addr&0xfff)>>2] = u32buff;	// Get data from register
	usleep(100);

	munmap(pu32mmap, 0xfff);
	if (fd) close(fd);
	return 0;    
}

int mem_int_get( unsigned int u32addr, unsigned int *pu32data ) 
{
	int	fd = 0;
	unsigned int	*pu32mmap = NULL;	

	fd = open("/dev/mem", O_RDWR);
	if ( fd == -1 )
	{
		return -1;
	}

	pu32mmap = (unsigned int *)mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, u32addr&0xfffff000);
	if(pu32mmap == (void *) -1)
	{
		if (fd) close(fd);
		return -1;
	}	


	*pu32data = pu32mmap[(u32addr&0xfff)>>2] ;
	munmap(pu32mmap, 0xfff);
	if (fd) close(fd);
	return 0;    
}

void PrintBaseScreen( void );


static void usage( void ) {

    fprintf( stderr, "\n"LFDK_VERTEXT"\n" );
	fprintf( stderr, "Copyright (C) 2006 - 2010, Merck Hung <merckhung@gmail.com>\n" );
	fprintf( stderr, "Copyright (C) 2013 Desmond Wu <wkunhui@gmail.com>\n" );
    fprintf( stderr, "Usage: "LFDK_PROGNAME" [-h] [-d /dev/lfdd] [-n ./pci.ids] [-b 255]\n" );
    fprintf( stderr, "\t-n\tFilename of PCI Name Database, default is /usr/share/misc/pci.ids\n" );
    fprintf( stderr, "\t-d\tDevice name of Linux Firmware Debug Driver, default is /dev/lfdd\n" );
    fprintf( stderr, "\t-b\tMaximum PCI Bus number to scan, default is 255\n" );
    fprintf( stderr, "\t-h\tprint this message.\n");
    fprintf( stderr, "\n");
}


void InitColorPairs( void ) {

    init_pair( WHITE_RED, COLOR_WHITE, COLOR_RED );
    init_pair( WHITE_BLUE, COLOR_WHITE, COLOR_BLUE );
    init_pair( BLACK_WHITE, COLOR_BLACK, COLOR_WHITE ); 
    init_pair( CYAN_BLUE, COLOR_CYAN, COLOR_BLUE );
    init_pair( RED_BLUE, COLOR_RED, COLOR_BLUE );
    init_pair( YELLOW_BLUE, COLOR_YELLOW, COLOR_BLUE );
    init_pair( BLACK_GREEN, COLOR_BLACK, COLOR_GREEN );
    init_pair( BLACK_YELLOW, COLOR_BLACK, COLOR_YELLOW );
    init_pair( YELLOW_RED, COLOR_YELLOW, COLOR_RED );
    init_pair( YELLOW_BLACK, COLOR_YELLOW, COLOR_BLACK );
    init_pair( WHITE_YELLOW, COLOR_WHITE, COLOR_YELLOW );
}


void PrintBaseScreen( void ) {


    //
    // Background Color
    //
    PrintWin( BaseScreen, bg, 23, 80, 0, 0, WHITE_BLUE, "" );


    //
    // Base Screen
    //
    PrintWin( BaseScreen, logo, 1, 80, 0, 0, WHITE_RED, "Linux Firmware Debug Kit "LFDK_VERSION );
    PrintWin( BaseScreen, copyright, 1, 32, 0, 48, WHITE_RED, "Merck Hung <merckhung@gmail.com>" );
    PrintWin( BaseScreen, help, 1, 80, 23, 0, BLACK_WHITE, "(Q)uit (P)CI (M)emory (I)O CM(O)S" );


    update_panels();
    doupdate();
}


int main( int argc, char **argv ) {

    extern char *optarg;
    extern int optind;

    char c, device[ LFDK_MAX_PATH ];
    int i, fd, orig_fl;

    struct tm *nowtime;
    time_t timer;
    int last_sec;


    //
    // Initialize & set default value
    //
//    strncpy( device, LFDD_DEFAULT_PATH, LFDK_MAX_PATH );
    strncpy( pciname, LFDK_DEFAULT_PCINAME, LFDK_MAX_PATH );


    while( (c = getopt( argc, argv, "b:n:d:h" )) != EOF ) {

        switch( c ) {
        

            //
            // Change default path of device
            //
            case 'd' :

                strncpy( device, optarg, LFDK_MAX_PATH );
                break;

            //
            // Change default path of PCI name database
            //
            case 'n' :

                strncpy( pciname, optarg, LFDK_MAX_PATH );
                break;

            case 'b' :

                maxpcibus = atoi( optarg );
                if( maxpcibus >= LFDK_MAX_PCIBUS ) {
                
                    fprintf( stderr, "Maximum PCI Bus value must be less than %d\n", LFDK_MAX_PCIBUS );
                    return 1;
                }
                break;

            case 'v' :
                break;

            case 'h' :
            default:
                usage();
                return 1;
        }
    }


    //
    // Start communicate with LFDD I/O control driver
    //
    fd = open("/dev/mem", O_RDWR);
    if( fd < 0 ) {

        fprintf( stderr, "Cannot open device: %s\n", "/dev/mem" );
        return 1;
    }
	if (fd) close(fd);


    //
    // Ncurse start
    //
    initscr();
    start_color();
    cbreak();
    noecho();
    nodelay( stdscr, 1 );
    keypad( stdscr, 1 );
    curs_set( 0 );


    //
    // Initialize color pairs for later use
    //
    InitColorPairs();


    //
    // Prepare Base Screen
    //
    PrintBaseScreen();


    //
    // Scan PCI devices
    //
    ScanPCIDevice();


    for( ; ; ) {


        ibuf = getch();
        if( (ibuf == 'q') || (ibuf == 'Q') ) {
        
            //
            // Exit when ESC pressed
            //
            break;
        }


        //
        // Major function switch key binding
        //
        if( (ibuf == 'p') || (ibuf == 'P') ) {

            func = PCI_LIST_FUNC;
            ClearPCIScreen();
            ClearMemScreen(); 
            ClearIOScreen();
			ClearCmosScreen();
            continue;
        }
        else if( (ibuf == 'm') || (ibuf == 'M') ) {
        
            enter_mem = 1;
            func = MEM_SPACE_FUNC;
            ClearPCIScreen();
            ClearPCILScreen();
            ClearIOScreen();
			ClearCmosScreen();
            continue;
        }
        else if( (ibuf == 'i') || (ibuf == 'I') ) {

            enter_mem = 1;
            func = IO_SPACE_FUNC;
            ClearPCIScreen();
            ClearPCILScreen();
            ClearMemScreen();
			ClearCmosScreen();
            continue;
        }
        else if( (ibuf == 'o') || (ibuf == 'O') ) {

            enter_mem = 1;
            func = CMOS_SPACE_FUNC;
            ClearPCIScreen();
            ClearPCILScreen();
            ClearMemScreen();
			ClearIOScreen();
            continue;
        }
/*
        else if( ibuf == '2' ) {

            enter_mem = 1;
            func = I2C_SPACE_FUNC;
            ClearPCIScreen();
            ClearPCILScreen();
            ClearMemScreen();
			ClearIOScreen();
			ClearCmosScreen();
            continue;
        }
*/


        //
        // Update timer
        //
        time( &timer );
        nowtime = localtime( &timer );
        last_sec = nowtime->tm_sec;


        // Skip redundant update of timer
        if( last_sec == nowtime->tm_sec ) {

            PrintFixedWin( BaseScreen, time, 1, 8, 23, 71, BLACK_WHITE, "%2.2d:%2.2d:%2.2d", nowtime->tm_hour, nowtime->tm_min,  nowtime->tm_sec );
        }


        //
        // Major Functions
        //
        switch( func ) {
        
            case PCI_DEVICE_FUNC:

                PrintPCIScreen( fd );
                break;

            case PCI_LIST_FUNC:

                PrintPCILScreen();
                break;

            case MEM_SPACE_FUNC:

                PrintMemScreen( fd );
                break;

            case IO_SPACE_FUNC:

                PrintIOScreen( fd );
                break;

			case CMOS_SPACE_FUNC:

				PrintCmosScreen( fd );
				break;

			case I2C_SPACE_FUNC:

				PrintCmosScreen( fd );
				break;

            default:
                break;
        } 


        //
        // Refresh Screen
        //
        update_panels();
        doupdate();


        usleep( 1000 );
    }


    endwin();
    close( fd );


    fprintf( stderr, "\n" );
    usage();


    return 0;
}


