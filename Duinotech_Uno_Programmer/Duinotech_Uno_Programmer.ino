//TO add- read from flash to serial port to create array
//check processor (in case)
//read and output to serial port with <---headers--->

//working- reset, write, verify
//includes snake game
//USB shield uses 9,10,11,12,13 
//uses Acm.isReady() to detect disconnect- use to reset LED's
//complete, working with button and LED

int cxnstat=0;                      //is device connected?
int lastcxnstat=0;
#include "sketch.h"
#include <cdcacm.h>
#include <usbhub.h>

class ACMAsyncOper : public CDCAsyncOper{
public:
    uint8_t OnInit(ACM *pacm);
};

uint8_t ACMAsyncOper::OnInit(ACM *pacm){
    uint8_t rcode;
    rcode = pacm->SetControlLineState(3);    // Set DTR = 1 RTS=1
    LINE_CODING  lc;
    lc.dwDTERate  = 115200;
    lc.bCharFormat  = 0;
    lc.bParityType  = 0;
    lc.bDataBits  = 8;
    rcode = pacm->SetLineCoding(&lc);
    return rcode;    
}

USB     Usb;
ACMAsyncOper  AsyncOper;
ACM           Acm(&Usb,&AsyncOper);

#define RXBUFSIZE 260
char hex[]="0123456789ABCDEF";
byte stksync[]={0x30,0x20};               //get in sync cmd
byte stkpmode[]={0x50,0x20};              //programming mode command
byte stkgetd[]={0x75,0x20};               //get info command
byte stklda[]={0x55,0x01,0x00,0x20};      //load address (words, lo:hi)
byte stkpgf[]={0x64,0x00,0x00,0x46};      //program flash block, size in bytes hi:lo (0x20 follows data)
byte stkrdf[]={0x74,0x00,0x80,0x46,0x20}; //read flash block, 128 bytes
byte rxbuf[RXBUFSIZE+2]="";               //from attached Uno
int rxbufptr=0;

#define BUTTON 3
#define LED1 4
#define LED2 5
#define LED3 6
#define LED4 7

const char* test;
int testsz=0;
byte tmp[64];
unsigned long progt;                      //track time to do entire operation

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println("Start. Type ~ to read FLASH.");
  if(Usb.Init()==-1){Serial.println("OSCOKIRQ failed to assert");}
  delay(200);
  pinMode(BUTTON,INPUT_PULLUP);   //init button
  pinMode(LED1,OUTPUT);
  digitalWrite(LED1,LOW);         //LED1 OFF
  pinMode(LED2,OUTPUT);
  digitalWrite(LED2,LOW);         //LED2 OFF
  pinMode(LED3,OUTPUT);
  digitalWrite(LED3,LOW);         //LED3 OFF
  pinMode(LED4,OUTPUT);
  digitalWrite(LED4,LOW);         //LED4 OFF
}

void loop() {
  cxnstat=Acm.isReady();      //check if connected
  Usb.Task();
  checkrx();
  if(cxnstat!=lastcxnstat){
    Serial.print("CXN:");
    Serial.println(cxnstat);
    lastcxnstat=cxnstat;
    if(cxnstat==0){                   //clear LEDs on disconnect
      digitalWrite(LED1,LOW);         //LED1 OFF
      digitalWrite(LED2,LOW);         //LED2 OFF
      digitalWrite(LED3,LOW);         //LED3 OFF
      digitalWrite(LED4,LOW);         //LED4 OFF
    }
  }
  if(Serial.available()){
    if(Serial.read()=='~'){
      progt=millis();
      if(resetinit()){
        int a;
        Serial.println("Reset OK");
        Serial.println("--------------------------COPY HERE--------------------------");
        Serial.println("const char PROGMEM sketch[32256]={");
        a=readdata();
        Serial.println("};");
        if(a){Serial.println("--------------------------COPY HERE--------------------------");}
        else{Serial.println("READ FAIL");}
      }else{
        Serial.println("Reset fail");
      }
    }
  Serial.print("Read took ");
  Serial.print(millis()-progt);
  Serial.println("ms");
  }
  if(digitalRead(BUTTON)==0){              //button to program a fixed sketch
    Serial.println("Programming sketch");
    progt=millis();
    digitalWrite(LED1,LOW);         //LED1 OFF
    digitalWrite(LED2,LOW);         //LED2 OFF
    digitalWrite(LED3,LOW);         //LED3 OFF
    digitalWrite(LED4,LOW);         //LED4 OFF
    if(resetinit()){
      Serial.println("Reset OK");
      digitalWrite(LED1,HIGH);
      if(progdata(sketch,sizeof(sketch))){      //included sketch
//      if(progdata(0,32256)){                    //this sketch (entire flash)
        Serial.println("Write OK");      
        digitalWrite(LED2,HIGH);
        if(verdata(sketch,sizeof(sketch))){     //included sketch
//        if(verdata(0,32256)){                   //this sketch (entire flash)
          Serial.println("Verify OK");
          digitalWrite(LED3,HIGH);
          Serial.print("Total:");
          Serial.print(millis()-progt);
          Serial.println("ms");
        }else{
          Serial.println("Verify Fail");                
          digitalWrite(LED4,HIGH);
        }
      }else{
        Serial.println("Write fail");                    
        digitalWrite(LED4,HIGH);
      }
    }else{
      Serial.println("Reset fail");      
      digitalWrite(LED4,HIGH);
    }
  delay(2000);                            //hold LED status for 2 sec
  }
}

int resetinit(){       //reset connected Uno and set prog mode
  int retval=1;        //true until an error occurs
  uint8_t rcode;
  rcode=Acm.SetControlLineState(0);     //toggle DTR low
  sdelay(200);
  rcode=Acm.SetControlLineState(3);     //toggle DTR high
  for(int i=0;i<5;i++){
    sendarray(stksync,sizeof(stksync));          
    datadelay(400,2);
    if((rxbufptr>1)&&(rxbuf[0]==0x14)&&(rxbuf[1]==0x10)){i=100;}    //in sync, jump out of loop
    showbuf();
    }
  sendarray(stkpmode,sizeof(stkpmode));          
  datadelay(400,2);
  if((rxbufptr<2)||(rxbuf[0]!=0x14)||(rxbuf[1]!=0x10)){return 0;}    //not in sync
  showbuf();
  sendarray(stkgetd,sizeof(stkgetd));                  
  datadelay(400,5);
  if((rxbufptr<5)||(rxbuf[0]!=0x14)||(rxbuf[1]!=0x1E)||(rxbuf[2]!=0x95)||(rxbuf[3]!=0x0F)||(rxbuf[4]!=0x10)){Serial.println("Wrong Signature");return 0;}    //not in sync  
  showbuf();  
  return retval;
}

void checkrx(){
  uint8_t  buf[64];
  uint16_t rcvd = 64;
  if(Acm.isReady()){
    Acm.RcvData(&rcvd, buf);
    if(rcvd){                     //more than zero bytes received
      for(uint16_t i=0;i<rcvd;i++){
        rxbuf[rxbufptr]=(byte)buf[i];   //copy to buffer
        rxbufptr++;
        if(rxbufptr>RXBUFSIZE){rxbufptr=RXBUFSIZE;}  //clamp if buffer is full
      }
    }
  }
  //delay(10);        //doesn't seem to be necessary, try uncommenting if errors occur
}

void sendarray(byte *c,int n){  //n is size of c, only works for very short arrays
  for(int i=0;i<n;i++){tmp[i]=c[i];}    //copy to temp, as next function clears contents
  Acm.SndData(n,tmp);      //send-> this zeroes contents!
  Usb.Task();
}

void sendarrayp(const char *c,int n){  //from progmem
  int i;
  uint8_t d;
  for(i=0;i<n;i++){
    d=pgm_read_byte_near(c+i);
    Acm.SndData(1,&d);      //send one octet
  }   
}

void sendtx(char c){
  uint8_t d=c;
  Acm.SndData(1,&d);      //send one octet
  Usb.Task();
}

unsigned long datadelay(unsigned long t,int n){      //wait up to t millis for n bytes to arrive
  unsigned long m0=millis();
  t=t+m0;
  while((millis()<t)&&(rxbufptr<n)){
    Usb.Task();    
    checkrx();
  }
  return (millis()-m0);                      //return time waited
}

void sdelay(unsigned long t){     //delay with USB tasks
  t=t+millis();
  while(millis()<t){
    Usb.Task();    
    checkrx();
  }
}

int progdata(const char* test,int testsz){    //write data from test to flash
  unsigned long tt=millis();
  int retval=1;         //true until an error occurs
  int a=0;
  int s=testsz;
  int p;
  while(s>0){
  p=128;              //128 is '328 page size
  if(p>s){p=s;}       //cap last packet size
    stklda[1]=(a/2)&0xFF;   //set low address
    stklda[2]=((a/2)>>8)&0xFF;   //set hi address
    sendarray(stklda,sizeof(stklda));  //send load address
    datadelay(50,2);
    if((rxbufptr<2)||(rxbuf[0]!=0x14)||(rxbuf[1]!=0x10)){Serial.println("LDA fail");return 0;}    //not in sync
    showbuf();
    stkpgf[1]=(p>>8)&0xFF;    //hi size
    stkpgf[2]=p&0xFF;         //lo size
    sendarray(stkpgf,sizeof(stkpgf));  //send page
    sendarrayp(test+a,p);     //send data from PROGMEM
    sendtx(0x20);             //CRC_EOP
    datadelay(1000,2);
    if((rxbufptr<2)||(rxbuf[0]!=0x14)||(rxbuf[1]!=0x10)){Serial.println("Write fail");return 0;}    //not in sync
    showbuf();
    s=s-p;              //size left
    a=a+p;              //next address to write
  }    
  Serial.print(testsz);
  Serial.print(" bytes in ");
  Serial.print(millis()-tt);
  Serial.print(" ms at ");
  Serial.print(testsz*1000L/(millis()-tt));
  Serial.println(" bytes/sec.");
  return retval;
}

int readdata(){
  int retval=1;         //true until an error occurs
  int a=0;
  int s=32256;          //available flash size on Uno
  int p;
  int n=0;              //count incoming bytes
  while(s>0){
  p=16;              //default page size, to fit on a line
  if(p>s){p=s;}       //cap last packet size
    stklda[1]=(a/2)&0xFF;   //set low address
    stklda[2]=((a/2)>>8)&0xFF;   //set hi address
    sendarray(stklda,sizeof(stklda));  //send load address
    datadelay(50,2);
    if((rxbufptr<2)||(rxbuf[0]!=0x14)||(rxbuf[1]!=0x10)){Serial.println("LDA fail");return 0;}    //not in sync
    showbuf();    
    stkrdf[1]=(p>>8)&0xFF;    //hi size
    stkrdf[2]=p&0xFF;         //lo size
    sendarray(stkrdf,sizeof(stkrdf));  //send read command
    datadelay(200,p+2);
    if((rxbufptr<p+2)||(rxbuf[0]!=0x14)||(rxbuf[p+1]!=0x10)){Serial.println("Read fail");return 0;}    //not in sync
    for(int i=0;i<p;i++){Serial.write('0');Serial.write('x');Serial.write(hex[(rxbuf[i+1]>>4)&15]);Serial.write(hex[(rxbuf[i+1])&15]);if(i<p-1){Serial.write(',');}}//dump to serial port
    n=n+rxbufptr-2;
    showbuf();
    s=s-p;              //size left
    a=a+p;              //next address to write
    if(s>0){Serial.println(",");}
  }    
  return retval;
}

int verdata(const char* test,int testsz){    //read and verify data from test to flash
  unsigned long tt=millis();
  int retval=1;         //true until an error occurs
  int a=0;
  int s=testsz;
  int p;
  int n=0;              //count incoming bytes
  while(s>0){
  p=256;              //default page size, limit for read is memory buffer
  if(p>s){p=s;}       //cap last packet size
    stklda[1]=(a/2)&0xFF;   //set low address
    stklda[2]=((a/2)>>8)&0xFF;   //set hi address
    sendarray(stklda,sizeof(stklda));  //send load address
    datadelay(50,2);
    if((rxbufptr<2)||(rxbuf[0]!=0x14)||(rxbuf[1]!=0x10)){Serial.println("LDA fail");return 0;}    //not in sync
    showbuf();    
    stkrdf[1]=(p>>8)&0xFF;    //hi size
    stkrdf[2]=p&0xFF;         //lo size
    sendarray(stkrdf,sizeof(stkrdf));  //send read command
    datadelay(200,p+2);
    if((rxbufptr<p+2)||(rxbuf[0]!=0x14)||(rxbuf[p+1]!=0x10)){Serial.println("Read fail");return 0;}    //not in sync
    for(int i=0;i<p;i++){if(pgm_read_byte_near(test+a+i)!=rxbuf[i+1]){Serial.println("Data mismatch");return 0;}}   //data doesn't match
    n=n+rxbufptr-2;
    showbuf();
    s=s-p;              //size left
    a=a+p;              //next address to write
  }    
  Serial.print(n);
  Serial.print(" bytes in ");
  Serial.print(millis()-tt);
  Serial.print(" ms at ");
  Serial.print(testsz*1000L/(millis()-tt));
  Serial.println(" bytes/sec.");
  return retval;
}

void showbuf(){
  //uncomment to show number of bytes received
  //Serial.print("b:");
  //Serial.println(rxbufptr);  
  rxbufptr=0;         //'clear' buffer
}

