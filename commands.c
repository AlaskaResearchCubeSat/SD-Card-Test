#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <msp430.h>
#include <ctl_api.h>
#include <errno.h>
#include <SDlib.h>
#include "terminal.h"
#include <ARCbus.h>
#include <Error.h>
#include <commandLib.h>
#include "SDtst_errors.h"


//define printf formats
#define HEXOUT_STR    "%02X "
#define ASCIIOUT_STR  "%c"

//report an error into the error log
int reportCmd(char **argv,unsigned short argc){
  if(argc!=4){
    printf("Error : %s requires 4 arguments but %i given.\r\n",argv[0],argc);
    return 1;
  }
  report_error(atoi(argv[1]),atoi(argv[2]),atoi(argv[3]),atoi(argv[4]));
  return 0;
}

int mmccount_Cmd(char **argv, unsigned short argc){
  int resp; 
  char *buffer=NULL;
  unsigned long sector=0;
  int i;
  //check if sector given
  if(argc!=0){
    //read sector
    if(1!=sscanf(argv[1],"%ul",&sector)){
      //print error
      printf("Error parsing sector \"%s\"\r\n",argv[1]);
      return -1;
    }
  }
  //get buffer, set a timeout of 2 secconds
  buffer=BUS_get_buffer(CTL_TIMEOUT_DELAY,2048);
  //check for error
  if(buffer==NULL){
    printf("Error : Timeout while waiting for buffer.\r\n");
    return -1;
  }
  //fill buffer with count
  for(i=0;i<512;i++){
    buffer[i]=i;
  }
  //write buffer
  resp=mmcWriteBlock(sector,(unsigned char*)buffer);
  //print response from SD card
  printf("%s\r\n",SD_error_str(resp));
  //free buffer
  BUS_free_buffer();
  return 0;
}

//table of commands with help
const CMD_SPEC cmd_tbl[]={{"help"," [command]\r\n\t""get a list of commands or help on a spesific command.",helpCmd},
                         CTL_COMMANDS,ARC_COMMANDS,ERROR_COMMANDS,MMC_COMMANDS,MMC_DREAD_COMMAND,
                         {"report","lev src err arg\r\n\t""Report an error",reportCmd},
                         {"mmccount","[sector]\r\n\t""Write count values to a sector.",mmccount_Cmd},
                         //end of list
                         {NULL,NULL,NULL}};
