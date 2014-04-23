#include <msp430.h>
#include <ctl_api.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <ARCbus.h>
#include <SDlib.h>
#include "timerA.h"
#include "terminal.h"
#include <Error.h>
#include <UCA1_uart.h>

CTL_TASK_t tasks[3];

//stacks for tasks
unsigned stack1[1+200+1];          
unsigned stack2[1+700+1];
unsigned stack3[1+300+1];   

CTL_EVENT_SET_t cmd_parse_evt;

unsigned char buffer[80];

int __putchar(int c){
  return UCA1_TxChar(c);
}

//handle subsystem specific commands
int SUB_parseCmd(unsigned char src,unsigned char cmd,unsigned char *dat,unsigned short len){
  int i;
  switch(cmd){
    //Handle Print String Command
    case 6:
      //check packet length
      if(len>sizeof(buffer)){
        //return error
        return ERR_PK_LEN;
      }
      //copy to temporary buffer
      for(i=0;i<len;i++){
        buffer[i]=dat[i];
      }
      //terminate string
      buffer[i]=0;
      //set event
      ctl_events_set_clear(&cmd_parse_evt,0x01,0);
      //Return Success
      return RET_SUCCESS;
  }
  //Return Error
  return ERR_UNKNOWN_CMD;
}

void cmd_parse(void *p) __toplevel{
  unsigned int e;
  //init event
  ctl_events_init(&cmd_parse_evt,0);
  //loop forever
  for(;;){
    e=ctl_events_wait(CTL_EVENT_WAIT_ANY_EVENTS_WITH_AUTO_CLEAR,&cmd_parse_evt,0x01,CTL_TIMEOUT_NONE,0);
    if(e&0x01){
      //print message
      if(async_isOpen()){
        printf("%s\r\n",buffer);
      }
    }
  }
}

void sub_events(void *p) __toplevel{
  unsigned int e,len;
  int i;
  unsigned char buf[10],*ptr;
  extern unsigned char async_addr;
  for(;;){
    e=ctl_events_wait(CTL_EVENT_WAIT_ANY_EVENTS_WITH_AUTO_CLEAR,&SUB_events,SUB_EV_ALL|SUB_EV_ASYNC_OPEN|SUB_EV_ASYNC_CLOSE,CTL_TIMEOUT_NONE,0);
    if(e&SUB_EV_PWR_OFF){
      //print message
      //puts("System Powering Down\r");
    }
    if(e&SUB_EV_PWR_ON){
      //print message
      //puts("System Powering Up\r");
    }
    if(e&SUB_EV_SEND_STAT){
      //send status
      //puts("Sending status\r");
      //setup packet 
      //TODO: put actual command for subsystem response
      ptr=BUS_cmd_init(buf,20);
      //TODO: fill in telemitry data
      //send command
      BUS_cmd_tx(BUS_ADDR_CDH,buf,0,0,BUS_I2C_SEND_FOREGROUND);
    }
    if(e&SUB_EV_TIME_CHECK){
      //printf("time ticker = %li\r\n",get_ticker_time());
    }
    if(e&SUB_EV_SPI_DAT){
      //puts("SPI data recived:\r");
      //get length
      len=arcBus_stat.spi_stat.len;
      //print out data
      /*for(i=0;i<len;i++){
        //printf("0x%02X ",rx[i]);
        printf("%03i ",arcBus_stat.spi_stat.rx[i]);
      }
      printf("\r\n");*/
      //free buffer
      BUS_free_buffer_from_event();
    }
    if(e&SUB_EV_SPI_ERR_CRC){
      puts("SPI bad CRC\r");
    }
    if(e&SUB_EV_ASYNC_OPEN){
      //kill off the terminal if it is running
      //ctl_task_remove(&tasks[1]);
      //setup closed event
      async_setup_close_event(&SUB_events,SUB_EV_ASYNC_CLOSE);
      //setup async terminal        
      //ctl_task_run(&tasks[1],2,terminal,"\rRemote Terminal started\r\n","async_terminal",sizeof(stack2)/sizeof(stack2[0])-2,stack2+1,0);
      //print message
      //printf("Async Opened from 0x%02X\r\n",async_addr);
      //set LED's to indicate status
      #ifdef ACDS_BUILD
        P4OUT&=~BIT0;
        P4OUT|=BIT1;
      #else
        P7OUT&=~BIT0;
        P7OUT|=BIT1;
      #endif
    }
    if(e&SUB_EV_ASYNC_CLOSE){
      //kill off async terminal
      //ctl_task_remove(&tasks[1]);
      //set LED's to indicate status
      #ifdef ACDS_BUILD
        P4OUT&=~BIT1;
      #else
        P7OUT&=~BIT1;
      #endif
    }
  }
}


static const TERM_SPEC async_term={"SD Card Test Program Ready",async_Getc};
static const TERM_SPEC uart_term={"SD Card Test Program Ready",UCA1_Getc};

int main(void){
  //DO this first
  ARC_setup(); 
  
  //setup system specific peripherals

  //setup mmc interface
  mmcInit_msp();
  
  //TESTING: set log level to report everything by default
  set_error_level(0);
  
  //setup UCA1 UART
  UCA1_init_UART();

#ifdef ACDS_BUILD
 //set all driver pins low
  P2OUT&=0x03;
  P4OUT&=0x03;
  P5OUT&=0x03;
  P6OUT&=0x03;
  P7OUT&=0x03;
  P8OUT&=0x03;
  //set pins to GPIO function
  P2SEL&=0x03;
  P4SEL&=0x03;
  P5SEL&=0x03;
  P6SEL&=0x03;
  P7SEL&=0x03;
  P8SEL&=0x03;
  //set driver pins as outputs
  P2DIR|=0xFC;
  P4DIR|=0xFC;
  P5DIR|=0xFC;
  P6DIR|=0xFC;
  P7DIR|=0xFC;
  P8DIR|=0xFC;
  //setup LED Pins
  P2SEL&=0xFC;
  P4SEL&=0xFC;
  P2OUT&=0xFC;
  P4OUT&=0xFC;
  P2DIR|=0x03;
  P4DIR|=0x03;
  //turn on power LED
  P2DIR|=BIT1;
  
  //setup bus interface
  initARCbus(BUS_ADDR_ACDS);
#else
  //setup P8 for output
  P8OUT=0x00;
  P8DIR=0xFF;
  P8SEL=0x00;
  
  //Turn off LED's
  P7OUT=0x00;
  P7DIR=0xFF;
  //turn on power LED
  P7OUT|=BIT4;

  //setup bus interface
  initARCbus(BUS_ADDR_IMG);
#endif


  //initialize stacks
  memset(stack1,0xcd,sizeof(stack1));  // write known values into the stack
  stack1[0]=stack1[sizeof(stack1)/sizeof(stack1[0])-1]=0xfeed; // put marker values at the words before/after the stack
  
  memset(stack2,0xcd,sizeof(stack2));  // write known values into the stack
  stack2[0]=stack2[sizeof(stack2)/sizeof(stack2[0])-1]=0xfeed; // put marker values at the words before/after the stack
    
  memset(stack3,0xcd,sizeof(stack3));  // write known values into the stack
  stack3[0]=stack3[sizeof(stack3)/sizeof(stack3[0])-1]=0xfeed; // put marker values at the words before/after the stack

  //create tasks
  ctl_task_run(&tasks[0],BUS_PRI_LOW,cmd_parse,NULL,"cmd_parse",sizeof(stack1)/sizeof(stack1[0])-2,stack1+1,0);
 
  ctl_task_run(&tasks[1],BUS_PRI_NORMAL,terminal,(void*)&uart_term,"terminal",sizeof(stack2)/sizeof(stack2[0])-2,stack2+1,0);

  ctl_task_run(&tasks[2],BUS_PRI_HIGH,sub_events,NULL,"sub_events",sizeof(stack3)/sizeof(stack3[0])-2,stack3+1,0);
  
  mainLoop();
}

