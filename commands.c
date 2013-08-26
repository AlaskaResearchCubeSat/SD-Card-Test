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
#include "SDtst_errors.h"


//define printf formats
#define HEXOUT_STR    "%02X "
#define ASCIIOUT_STR  "%c"


//helper function to parse I2C address
//if res is true reject reserved addresses
unsigned char getI2C_addr(char *str,short res){
  unsigned long addr;
  unsigned char tmp;
  char *end;
  //attempt to parse a numeric address
  addr=strtol(str,&end,0);
  //check for errors
  if(end==str){
    //check for symbolic matches
    if(!strcmp(str,"LEDL")){
      return BUS_ADDR_LEDL;
    }else if(!strcmp(str,"ACDS")){
      return BUS_ADDR_ACDS;
    }else if(!strcmp(str,"COMM")){
      return BUS_ADDR_COMM;
    }else if(!strcmp(str,"IMG")){
      return BUS_ADDR_IMG;
    }else if(!strcmp(str,"CDH")){
      return BUS_ADDR_CDH;
    }else if(!strcmp(str,"GC")){
      return BUS_ADDR_GC;
    }
    //not a known address, error
    printf("Error : could not parse address \"%s\".\r\n",str);
    return 0xFF;
  }
  if(*end!=0){
    printf("Error : unknown sufix \"%s\" at end of address\r\n",end);
    return 0xFF;
  }
  //check address length
  if(addr>0x7F){
    printf("Error : address 0x%04lX is not 7 bits.\r\n",addr);
    return 0xFF;
  }
  //check for reserved address
  tmp=0x78&addr;
  if((tmp==0x00 || tmp==0x78) && res){
    printf("Error : address 0x%02lX is reserved.\r\n",addr);
    return 0xFF;
  }
  //return address
  return addr;
}

//reset a MSP430 on command
int restCmd(char **argv,unsigned short argc){
  unsigned char buff[10];
  unsigned char addr;
  unsigned short all=0;
  int resp;
  //force user to pass no arguments to prevent unwanted resets
  if(argc>1){
    puts("Error : too many arguments\r");
    return -1;
  }
  if(argc!=0){
    if(!strcmp(argv[1],"all")){
      all=1;
      addr=BUS_ADDR_GC;
    }else{
      //get address
      addr=getI2C_addr(argv[1],0);
      if(addr==0xFF){
        return 1;
      }
    }
    //setup packet 
    BUS_cmd_init(buff,CMD_RESET);
    resp=BUS_cmd_tx(addr,buff,0,0,BUS_I2C_SEND_FOREGROUND);
    switch(resp){
      case 0:
        puts("Command Sent Sucussfully.\r");
      break;
    }
  }
  //reset if no arguments given or to reset all boards
  if(argc==0 || all){
    //Close async connection
    async_close();
    //write to WDTCTL without password causes PUC
    reset(ERR_LEV_INFO,SDTST_ERR_SRC_CMD,CMD_ERR_RESET,0);
    //Never reached due to reset
    puts("Error : Reset Failed!\r");
  }
  return 0;
}

//set priority for tasks on the fly
int priorityCmd(char **argv,unsigned short argc){
  extern CTL_TASK_t *ctl_task_list;
  int i,found=0;
  CTL_TASK_t *t=ctl_task_list;
  if(argc<1 || argc>2){
    printf("Error: %s takes one or two arguments, but %u are given.\r\n",argv[0],argc);
    return -1;
  }
  while(t!=NULL){
    if(!strcmp(t->name,argv[1])){
      found=1;
      //match found, break
      break;
    }
    t=t->next;
  }
  //check that a task was found
  if(found==0){
      //no task found, return
      printf("Error: could not find task named %s.\r\n",argv[1]);
      return -3;
  }
  //print original priority
  printf("\"%s\" priority = %u\r\n",t->name,t->priority);
  if(argc==2){
      unsigned char val=atoi(argv[2]);
      if(val==0){
        printf("Error: invalid priority.\r\n");
        return -2;
      }
      //set priority
      ctl_task_set_priority(t,val);
      //print original priority
      printf("new \"%s\" priority = %u\r\n",t->name,t->priority);
  }
  return 0;
}

//get/set ctl_timeslice_period
int timesliceCmd(char **argv,unsigned short argc){
  if(argc>1){
    printf("Error: too many arguments.\r\n");
    return 0;
  }
  //if one argument given then set otherwise get
  if(argc==1){
    int en;
    CTL_TIME_t val=atol(argv[1]);
    //check value
    if(val==0){
      printf("Error: bad value.\r\n");
      return -1;
    }
    //disable interrupts so that opperation is atomic
    en=ctl_global_interrupts_set(0);
    ctl_timeslice_period=val;
    ctl_global_interrupts_set(en);
  }
  printf("ctl_timeslice_period = %ul\r\n",ctl_timeslice_period);
  return 0;
}

//return state name
const char *stateName(unsigned char state){
  switch(state){
    case CTL_STATE_RUNNABLE:
      return "CTL_STATE_RUNNABLE";
    case CTL_STATE_TIMER_WAIT:
      return "CTL_STATE_TIMER_WAIT";
    case CTL_STATE_EVENT_WAIT_ALL:
      return "CTL_STATE_EVENT_WAIT_ALL";
    case CTL_STATE_EVENT_WAIT_ALL_AC:
      return "CTL_STATE_EVENT_WAIT_ALL_AC";
    case CTL_STATE_EVENT_WAIT_ANY:
      return "CTL_STATE_EVENT_WAIT_ANY";
    case CTL_STATE_EVENT_WAIT_ANY_AC:
      return "CTL_STATE_EVENT_WAIT_ANY_AC";
    case CTL_STATE_SEMAPHORE_WAIT:
      return "CTL_STATE_SEMAPHORE_WAIT";
    case CTL_STATE_MESSAGE_QUEUE_POST_WAIT:
      return "CTL_STATE_MESSAGE_QUEUE_POST_WAIT";
    case CTL_STATE_MESSAGE_QUEUE_RECEIVE_WAIT:
      return "CTL_STATE_MESSAGE_QUEUE_RECEIVE_WAIT";
    case CTL_STATE_MUTEX_WAIT:
      return "CTL_STATE_MUTEX_WAIT";
    case CTL_STATE_SUSPENDED:
      return "CTL_STATE_SUSPENDED";
    default:
      return "unknown state";
  }
}

//print the status of all tasks in a table
int statsCmd(char **argv,unsigned short argc){
  extern CTL_TASK_t *ctl_task_list;
  int i;
  CTL_TASK_t *t=ctl_task_list;
  //format string
  const char *fmt="%-10s\t%u\t\t%c%-28s\t%lu\r\n";
  //print out nice header
  printf("\r\nName\t\tPriority\tState\t\t\t\tTime\r\n--------------------------------------------------------------------\r\n");
  //loop through tasks and print out info
  while(t!=NULL){
    printf(fmt,t->name,t->priority,(t==ctl_task_executing)?'*':' ',stateName(t->state),t->execution_time);
    t=t->next;
  }
  //add a blank line after table
  printf("\r\n");
  return 0;
}

//print current time
int timeCmd(char **argv,unsigned short argc){
  printf("time ticker = %li\r\n",get_ticker_time());
  return 0;
}

int asyncCmd(char **argv,unsigned short argc){
   char c;
   CTL_EVENT_SET_t e=0,evt;
   unsigned char addr;
   if(argc!=0){
    printf("Error : \"%s\" takes zero arguments\r\n",argv[0]);
    return -1;
  }
  //close async connection
  if(async_close()!=RET_SUCCESS){
    printf("Error : async_close() failed.\r\n");
  }
}

int sendCmd(char **argv,unsigned short argc){
  unsigned char *ptr,id;
  unsigned short len;
  int i,j,k;
  if(!async_isOpen()){
    printf("Error : Async is not open\r\n");
    return -1;
  }
  //check number of arguments
  if(argc<1){
    printf("Error : too few arguments.\r\n");
    return 1;
  }
  //Send string data
  for(i=1,k=0;i<=argc;i++){
    j=0;
    while(argv[i][j]!=0){
      async_TxChar(argv[i][j++]);
    }
    async_TxChar(' ');
  }
  return 0;
}

int mmc_write(char **argv, unsigned short argc){
  //pointer to buffer, pointer inside buffer, pointer to string
  unsigned char *buffer=NULL,*ptr=NULL,*string;
  //response from block write
  int resp;
  int i ;
  //get buffer, set a timeout of 2 secconds
  buffer=BUS_get_buffer(CTL_TIMEOUT_DELAY,2048);
  //check for error
  if(buffer==NULL){
    printf("Error : Timeout while waiting for buffer.\r\n");
    return -1;
  }
  //clear all bytes in buffer
  memset(buffer,0,512);
  //concatinate arguments into one big string with spaces in between
  for(ptr=buffer,i=1; i<=argc; i++){
    string=(unsigned char*)argv[i];
    while(*string!=0){
      *ptr++=*string++;
    }
    *ptr++=' ';
  }
  //Terminate string
  *(ptr-1)=0;
  //write data
  resp=mmcWriteBlock(0,buffer);
  //check if write was successful
  if(resp==MMC_SUCCESS){
    printf("data written to memeory\r\n");
  }else{
    printf("resp = 0x%04X\r\n%s\r\n",resp,SD_error_str(resp));
  }
  //free buffer
  BUS_free_buffer();
  return 0;
}

int mmc_read(char **argv, unsigned short argc){
  char *ptr=NULL,*buffer=NULL; 
  int resp;
   //get buffer, set a timeout of 2 secconds
  buffer=BUS_get_buffer(CTL_TIMEOUT_DELAY,2048);
  //check for error
  if(buffer==NULL){
    printf("Error : Timeout while waiting for buffer.\r\n");
    return -1;
  }
  //init buffer
  memset(buffer,0,513);
  //read from SD card
  resp=mmcReadBlock(0,(unsigned char*)buffer);
  //check for error
  if(resp!=MMC_SUCCESS){
    //print error from SD card
    printf("%s\r\n",SD_error_str(resp));
    return 1;
  }
  //force termination after last byte
  buffer[512]=0;
  //check for non printable chars
  for(ptr=(char*)buffer;ptr!=0;ptr++){
    //check for non printable chars
    if(!isprint(*ptr)){
      //check for null
      if(*ptr){
        //not null, non printable char encountered
        printf("String prematurely terminated due to a non printable character.\r\n");
      }
      //terminate string
      *ptr=0;
      //exit loop
      break;
    }
  }
  //print out the string
  printf("Here is the string you wrote:\r\n\'%s\'\r\n",buffer);
  //free buffer
  BUS_free_buffer();
  return 0;
}

int mmc_dump(char **argv, unsigned short argc){
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
  //read from SD card
  resp=mmcReadBlock(sector,(unsigned char*)buffer);
  //print response from SD card
  printf("%s\r\n",SD_error_str(resp));
  //print out buffer
  for(i=0;i<512/16;i++){
    printf(HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR "\r\n",
    buffer[i*16],buffer[i*16+1],buffer[i*16+2],buffer[i*16+3],buffer[i*16+4],buffer[i*16+5],buffer[i*16+6],buffer[i*16+7],buffer[i*16+8],buffer[i*16+9],buffer[i*16+10],
    buffer[i*16+11],buffer[i*16+12],buffer[i*16+13],buffer[i*16+14],buffer[i*16+15]);
  }
  //free buffer
  BUS_free_buffer();
  return 0;
}

//read card size
int mmc_cardSize(char **argv, unsigned short argc){
  unsigned long size;
  unsigned char CSD[16];
  int resp;
  resp=mmcReadReg(0x40|9,CSD);
  size=mmcGetCardSize(CSD);
  if(resp==MMC_SUCCESS){
    printf("card size = %luKB\r\n",size);
  }else{
    printf("%s\r\n",SD_error_str(resp));
  }
  return 0;
}

int mmc_eraseCmd(char **argv, unsigned short argc){
  unsigned long start,end;
  int resp;
  //check arguments
  if(argc!=2){
    printf("Error : %s requiors two arguments\r\n",argv[0]);
    return 1;
  }
  errno=0;
  start=strtoul(argv[1],NULL,0);
  end=strtoul(argv[2],NULL,0);
  if(errno){
    printf("Error : could not parse arguments\r\n");
    return 2;
  }
  printf("Erasing from %lu to %lu\r\n",start,end);
  //send erase command
  resp=mmcErase(start,end);
  printf("%s\r\n",SD_error_str(resp));
  return 0;
}

//data types for TstCmd
enum{DAT_LFSR=0,DAT_COUNT};

//return next value in data sequence
char dat_next(char v,int type){
  switch(type){
    //next value in the LFSR sequence x^8 + x^6 + x^5 + x^4 + 1
    case DAT_LFSR:
      //code taken from: http://en.wikipedia.org/wiki/Linear_feedback_shift_register#Galois_LFSRs
      return (v>>1)^(-(v&1)&0xB8);
    //count up by one
    case DAT_COUNT:
      return v+1;
    //unknown type return zero
    default:
      printf("Error : unknown type\r\n");
      return 0;
  }
}

//write LFSR pattern onto SD card sectors and read it back
int mmc_TstCmd(char **argv, unsigned short argc){
  int resp;
  char seed,*buffer=NULL;
  short lfsr;
  int j,count,tc,dat=DAT_LFSR,have_seed=0;
  unsigned long i,start,end;
  if(argc<2){
    printf("Error : Too few arguments\r\n");
    return 1;
  }
  errno=0;
  start=strtoul(argv[1],NULL,0);
  end=strtoul(argv[2],NULL,0);
  if(argc>=3){
    //other arguments are optional
    for(i=3;i<=argc;i++){
      if(!strcmp(argv[i],"LFSR")){
        dat=DAT_LFSR;
      }else if(!strcmp(argv[i],"count")){
        dat=DAT_COUNT;
      }else if(!strncmp("seed=",argv[i],sizeof("seed"))){
        //parse seed
        seed=atoi(argv[i]+sizeof("seed"));
        have_seed=1;
      }else{
        printf("Error : unknown argument \"%s\".\r\n",argv[i]);
        return 3;
      }
    }
  }
  if(!have_seed){
    //seed LFSR from TAR
    seed=TAR;   //not concerned about correct value so don't worry about diffrent clocks
    //make sure seed is not zero
    if(seed==0){
      seed=1;
    }
    printf("seed = %i\r\n",(unsigned short)seed);
  }
  if(errno){
    printf("Error : could not parse arguments\r\n");
    return 2;
  }
  //get buffer, set a timeout of 2 secconds
  buffer=BUS_get_buffer(CTL_TIMEOUT_DELAY,2048);
  //check for error
  if(buffer==NULL){
    printf("Error : Timeout while waiting for buffer.\r\n");
    return -1;
  }
  #ifndef ACDS_BUILD
    //TESTING: set line high
    P8OUT|=BIT0;
  #endif
  //write to sectors
  for(i=start,lfsr=seed;i<=end;i++){
    //fill with psudo random data
    for(j=0;j<512;j++){
      buffer[j]=lfsr;
      //print out first few bytes
      /*if(j<20){
        printf("0x%02X, ",buffer[j]);
      }*/
      //get next in sequence
      lfsr=dat_next(lfsr,dat);
    }
    //printf("\r\n");
    //write data
    resp=mmcWriteBlock(i,(unsigned char*)buffer);
    if(resp!=MMC_SUCCESS){
      printf("Error : write failure for sector %i\r\nresp = 0x%04X\r\n%s\r\n",i,resp,SD_error_str(resp));
      //free buffer
      BUS_free_buffer();
      return -1;
    }
  }
  //read back sectors and check for correctness
  for(i=start,lfsr=seed,tc=0;i<=end;i++){
    //clear block data
    memset(buffer,0,512);
    //read data from card
    resp=mmcReadBlock(i,(unsigned char*)buffer);
    if(resp!=MMC_SUCCESS){
      printf("Error : read failure for sector %i\r\nresp = 0x%04X\r\n%s\r\n",i,resp,SD_error_str(resp));
      //free buffer
      BUS_free_buffer();
      return -1;
    }
    //compare to psudo random data
    for(j=0,count=0;j<512;j++){
      //print out the first few bytes
      /*if(j<20){
        printf("0x%02X =? 0x%02X, ",lfsr,buffer[j]);
      }*/
      if(buffer[j]!=lfsr){
        count++;
        //printf("Error found in byte %i in sector %i\r\n",j,i);
        //printf("Error b%i  s%i\r\n",j,i);
      }
      //get next in sequence
      lfsr=dat_next(lfsr,dat);
    }
    //printf("\r\n");
    if(count!=0){
      printf("%i errors found in sector %i\r\n",count,i);
      tc+=count;
    }
  }
  #ifndef ACDS_BUILD
    //TESTING: set line low
    P8OUT&=~BIT0;
  #endif
  if(tc==0){
    printf("All sectors read susussfully!\r\n");
  }
  //free buffer
  BUS_free_buffer();
  return 0;
}

int mmc_multiWTstCmd(char **argv, unsigned short argc){
  unsigned char *ptr;
  int stat;
  unsigned long i,start,end;
  unsigned short multi=1;
  if(argc<2){
    printf("Error : too few arguments\r\n");
    return -1;
  }
  if(argc>3){
    printf("Error : too many arguments\r\n");
    return -2;
  }
  //get start and end
  errno=0;
  start=strtoul(argv[1],NULL,0);
  end=strtoul(argv[2],NULL,0);
  if(errno){
    printf("Error : could not parse arguments\r\n");
    return 2;
  }
  if(argc>2){
    if(!strcmp("single",argv[3])){
      multi=0;
    }else if(!strcmp("multi",argv[3])){
      multi=1;
    }else{
      //unknown argument
      printf("Error : unknown argument \"%s\".\r\n",argv[3]);
      return -3;
    }
  }
  #ifndef ACDS_BUILD
    //TESTING: set line high
    P8OUT|=BIT0;
  #endif
  if(!multi){
    //write each block in sequence
    for(i=start,ptr=NULL;i<end;i++,ptr+=512){
      if((stat=mmcWriteBlock(i,ptr))!=MMC_SUCCESS){
        printf("Error writing block %li. Aborting.\r\n",i);
        printf("%s\r\n",SD_error_str(stat));
        return 1;
      }
    }
  }else{
    //write all blocks with one command
    if((stat=mmcWriteMultiBlock(start,NULL,end-start))!=MMC_SUCCESS){
      printf("Error with write. %i\r\n",stat);
      printf("%s\r\n",SD_error_str(stat));
      return 1;
    }
  }
  #ifndef ACDS_BUILD
    //TESTING: set line low
    P8OUT&=~BIT0;
  #endif
  printf("Data written sucussfully\r\n");
  return 0;
}

int mmc_multiRTstCmd(char **argv, unsigned short argc){
  unsigned char *ptr,*buffer;
  int resp;
  unsigned long i,start,end;
  unsigned short multi=1;
  if(argc<2){
    printf("Error : too few arguments\r\n");
    return -1;
  }
  if(argc>3){
    printf("Error : too many arguments\r\n");
    return -2;
  }
  //get start and end
  errno=0;
  start=strtoul(argv[1],NULL,0);
  end=strtoul(argv[2],NULL,0);
  if(errno){
    printf("Error : could not parse arguments\r\n");
    return -4;
  }
  if((end-start)*512>BUS_get_buffer_size()){
    printf("Error : data size too large for buffer.\r\n");
    return -5;
  }
  if(argc>2){
    if(!strcmp("single",argv[3])){
      multi=0;
    }else if(!strcmp("multi",argv[3])){
      multi=1;
    }else{
      //unknown argument
      printf("Error : unknown argument \"%s\".\r\n",argv[3]);
      return -3;
    }
  }
  //get buffer, set a timeout of 2 secconds
  buffer=BUS_get_buffer(CTL_TIMEOUT_DELAY,2048);
  #ifndef ACDS_BUILD
    //TESTING: set line high
    P8OUT|=BIT0;
  #endif
  if(!multi){
    //write each block in sequence
    for(i=start,ptr=buffer;i<end;i++,ptr+=512){
      if((resp=mmcReadBlock(i,ptr))!=MMC_SUCCESS){
        printf("Error reading block %li. Aborting.\r\n",i);     
        printf("%s\r\n",SD_error_str(resp));
        //free buffer
        BUS_free_buffer();
        return 1;   
      }
    }
  }else{
    //write all blocks with one command
    if((resp=mmcReadBlocks(start,end-start,buffer))!=MMC_SUCCESS){
      printf("Error with read.\r\n");
      printf("resp = 0x%04X\r\n%s\r\n",resp,SD_error_str(resp));
      //free buffer
      BUS_free_buffer();
      return 1;   
    }
  }
  #ifndef ACDS_BUILD
    //TESTING: set line low
    P8OUT&=~BIT0;
  #endif
  //print out buffer
  for(i=0;i<(end-start)*512/16;i++){
    printf(HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR HEXOUT_STR "\r\n",
    buffer[i*16],buffer[i*16+1],buffer[i*16+2],buffer[i*16+3],buffer[i*16+4],buffer[i*16+5],buffer[i*16+6],buffer[i*16+7],buffer[i*16+8],buffer[i*16+9],buffer[i*16+10],
    buffer[i*16+11],buffer[i*16+12],buffer[i*16+13],buffer[i*16+14],buffer[i*16+15]);
  }
  printf("Data read sucussfully\r\n");
  //free buffer
  BUS_free_buffer();
  return 0;
}

int mmc_reinit(char **argv, unsigned short argc){
  int resp;
  //setup the SD card
  resp=mmcReInit_card();
  //set some LEDs
  #ifndef ACDS_BUILD
    P7OUT&=~(BIT7|BIT6);
    if(resp==MMC_SUCCESS){
      P7OUT|=BIT7;
    }else{
      P7OUT|=BIT6;
    }
  #endif
  //print status
  printf("%s\r\n",SD_error_str(resp));
  return 0;
}

int mmcDMA_Cmd(char **argv, unsigned short argc){
  if(SD_DMA_is_enabled()){
    printf("DMA is enabled\r\n");
  }else{
    printf("DMA is not enabled\r\n");
  }
  return 0;
}

int mmcreg_Cmd(char**argv,unsigned short argc){
  unsigned char dat[16],reg;
  int resp;
  int i;
  //check number of arguments
  if(argc!=1){
    printf("Error : %s requires one argument\r\n",argv[0]);
    return -1;
  }
  //check register to read
  if(!strcmp("CSD",argv[1])){
    reg=0x40|9;
  }else if(!strcmp("CID",argv[1])){
    reg=0x40|10;
  }else{
    printf("Error : invalid register \"%s\"\r\n",argv[1]);
    return -2;
  }
  //read register
  resp=mmcReadReg(reg,dat);
  //check for success
  if(resp!=MMC_SUCCESS){
    printf("%s\r\n",SD_error_str(resp));
    return -3;
  }
  //print out contents
  for(i=0;i<16;i++){
    printf("%02X",dat[i]);
  }
  //add new line
  printf("\r\n");
  //return
  return 0;
}

int mmcInitChkCmd(char**argv,unsigned short argc){
  int resp;
  resp=mmc_is_init();
  if(resp==MMC_SUCCESS){
    printf("Card Initialized\r\n");
  }else{
    printf("Card Not Initialized\r\n%s\r\n",SD_error_str(resp));
  }
  return 0;
}
  
  
//print the status of each tasks stack
int stackCmd(char **argv,unsigned short argc){
  extern CTL_TASK_t *ctl_task_list;
  int i;
  CTL_TASK_t *t=ctl_task_list;
  //format string
  const char *fmt="%-10s\t%lp\t%lp\t%li\r\n";
  //print out nice header
  printf("\r\nName\tPointer\tStart\tRemaining\r\n--------------------------------------------------------------------\r\n");
  //loop through tasks and print out info
  while(t!=NULL){
    printf(fmt,t->name,t->stack_pointer,t->stack_start,t->stack_pointer-t->stack_start);
    t=t->next;
  }
  //add a blank line after table
  printf("\r\n");
  return 0;
}

int replayCmd(char **argv,unsigned short argc){
  error_log_replay();
  return 0;
}

//report an error into the error log
int reportCmd(char **argv,unsigned short argc){
  if(argc!=4){
    printf("Error : %s requires 4 arguments but %i given.\r\n",argv[0],argc);
    return 1;
  }
  report_error(atoi(argv[1]),atoi(argv[2]),atoi(argv[3]),atoi(argv[4]));
  return 0;
}

//clear saved errors from the SD card
int clearCmd(char **argv,unsigned short argc){
  int ret;
  ret=clear_saved_errors();
  if(ret){
    printf("Error erasing errors : %s\r\n",SD_error_str(ret));
  }
  return 0;
}

//table of commands with help
const CMD_SPEC cmd_tbl[]={{"help"," [command]\r\n\t""get a list of commands or help on a spesific command.",helpCmd},
                         {"priority"," task [priority]\r\n\t""Get/set task priority.",priorityCmd},
                         {"timeslice"," [period]\r\n\t""Get/set ctl_timeslice_period.",timesliceCmd},
                         {"stats","\r\n\t""Print task status",statsCmd},
                         {"reset","\r\n\t""reset the msp430.",restCmd},
                         {"time","\r\n\t""Return current time.",timeCmd},
                         {"async","\r\n\t""Close async connection.",asyncCmd},
                         {"exit","\r\n\t""Close async connection.",asyncCmd},                 //nice for those of us who are used to typing exit
                         {"mmcr","\r\n\t""read string from mmc card.",mmc_read},
                         {"mmcdump","[sector]\r\n\t""dump a sector from MMC card.",mmc_dump},
                         {"mmcw","[data,..]\r\n\t""write data to mmc card.",mmc_write},
                         {"mmcsize","\r\n\t""get card size.",mmc_cardSize},
                         {"mmce","start end\r\n\t""erase sectors from start to end",mmc_eraseCmd},
                         {"mmctst","start end [seed]\r\n\t""Test by writing to blocks from start to end.",mmc_TstCmd},
                         {"mmcmw","start end [single|multi]\r\n\t""Multi block write test.",mmc_multiWTstCmd},
                         {"mmcmr","start end [single|multi]\r\n\t""Multi block read test.",mmc_multiRTstCmd},
                         {"mmcreinit","\r\n\t""initialize the mmc card the mmc card.",mmc_reinit},
                         {"DMA","\r\n\t""Check if DMA is enabled.",mmcDMA_Cmd},
                         {"mmcreg","[CID|CSD]\r\n\t""Read SD card registers.",mmcreg_Cmd},
                         {"mmcinitchk","\r\n\t""Check if the SD card is initialized",mmcInitChkCmd},
                         {"stack","\r\n\t""Print task stack status",stackCmd},
                         {"replay","\r\n\t""Replay errors from log",replayCmd},
                         {"report","lev src err arg\r\n\t""Report an error",reportCmd},
                         {"clear","\r\n\t""Clear all saved errors on the SD card",clearCmd},
                         //end of list
                         {NULL,NULL,NULL}};
