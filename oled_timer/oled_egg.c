//OLED Timer PIC16F876A
//MPLAB X v3.10 xc8 v1.35
//moty22.co.uk


#include <pic16f876a.h>
#include <xc.h>
#include "font5x7.c"
#pragma config LVP=OFF, FOSC=HS, CP=OFF, CPD=OFF, WDTE=OFF

#define sec_sw RB0
#define min_sw RB1
#define start_stop RB2
#define si RB4		//sensor pin
#define si_in TRISB4=1		//sensor is input
#define si_out TRISB4=0		//sensor is output
#define bz1 RA1		//buzzer
#define bz2 RA2 	//buzzer
#define _XTAL_FREQ 8000000

// prototypes
void command( unsigned char comm);
void oled_init();
void drawChar(char fig, unsigned char y, unsigned char x);
void clrScreen();
void sendData(unsigned int dataB);
void startBit(void);
void stopBit(void);
void drawChar2(char fig, unsigned char y, unsigned char x);
unsigned char reply_in (void);
void cmnd_w_in(unsigned char cmnd);
unsigned char sensor_rst_in(void);
void temp(void);
void display(void);
void bell(void);

unsigned char on=1,sec=0,min=0,Tsec=0,second;	//
unsigned char d[4],pd[4],d_in[3],pd_in[3] ;	//
bit icon;

//int every second
void interrupt clock_tick(void){
	if (CCP1IF) {       //update time
		CCP1IF=0;
		second = 1;
        ++Tsec; //timing for temperature
        return;
	}
}

void main(void) {
    unsigned char fast=0, run=0;	//
    
    	// PIC I/O init
    ADCON1 = 0b110;     //digital inputs 
    TRISC = 0b11101;		// scl=rc3, sda=rc4, rc1=CPP2, rc0=T1CKL 
	TRISB = 0b111111;   	// switches, sensors 
	TRISA1 = 0;	TRISA2 = 0;//buzzer output
	nRBPU = 0;			//RB pullup on
    CCP2CON=0b1100;		//PWM mode
	CCPR2L=100;			//50% duty
	T2CON=0b101;		//prescale 1:4, timer ON
	PR2=199;			//ccp2 output 199=5000 hz
	CCP1CON=0b1011;		//compare mode
	CCPR1H=0x2; CCPR1L=0x70; // sets CCP1 for count of 0x270=624(625)-1sec
	T1CON=0b100011;		//osc off, timer on, 1:4  ,sync
	GIE=1; PEIE=1;      //interrupts    
    CCP1IE=1;
    
	//i2c init
	SMP=1;	//slew rate 100khz
	CKE=0;	//SMBus dis
	SSPADD = 9; //200khz
	SSPCON = 0B101000;	//SSPEN=1, Master mode, clock = FOSC / (4 * (SSPADD+1))
	SSPCON2 = 0;
    __delay_ms(100);
     
    oled_init();   // oled init
    clrScreen();       // clear screen
    drawChar2(0x3A, 1, 4); //:
    drawChar2(0x2E, 4, 4);  //.
    drawChar2(0x80, 4, 7);  //° 128
    drawChar2(0x43, 4, 8);  //C
    display();

    while (1) {

		//reset
		while(!sec_sw && !min_sw){
			sec=0; min=0; display();  //sec_mem=0; min_mem=0; 
			__delay_ms(250);
		}	
		//seconds setting
		if(sec_sw){fast=0;}
		while(!sec_sw){
			if(!sec_sw && !min_sw){break;}
			++sec;
			++fast;
			if(sec==60){sec=0;}
			display();
			if(fast>4){__delay_ms(250);}else{__delay_ms(1000);}
		}
		//minutes setting
		if(min_sw){fast=0;}
		while(!min_sw){
			if(!sec_sw && !min_sw){break;}
			++min;
			++fast;
			if(min==100){min=0;}	
			display();
			if(fast>4){__delay_ms(250);}else{__delay_ms(1000);}
		}

		// start clock
		if(!start_stop && (min || sec)){
			while(!start_stop){__delay_ms(250);}	//until sw is open
			run=1;  //out=1;

		}
        
        while(run){
            if(second){
                second=0; 
                --sec;
                if(min==0 && sec==0){run=0; display(); bell(); break;}	//alarm
                if(sec == 255){sec=59; if(min){--min;}}
                display();            
            }
            if(!start_stop){
                while(!start_stop){__delay_ms(250);}	//until sw is open
                run=0;  	
                break;
            } 
            if(Tsec > 2){Tsec=0; temp();}   //update temp.
        }
        if(Tsec > 2){Tsec=0; temp();} 

	}
}
 


void display(void){			//display the clock

	d[0]=(min/10) %10;		//10 min d
	d[1]=min %10;
    d[2]=(sec/10) %10;		//10 sec
	d[3]=sec %10;
        //draw time
    drawChar2(d[0]+48, 1, 2); //
	drawChar2(d[1]+48, 1, 3);
	drawChar2(d[2]+48, 1, 5);
	drawChar2(d[3]+48, 1, 6);       

}

void temp(void){
    int tempL, tempH,tempS;
    unsigned int temp;
	unsigned char  minus;//    
    
        //read sensor
	sensor_rst_in();	//sensor init
	cmnd_w_in(0xCC);	//skip ROM command
	cmnd_w_in(0xBE);	//read pad command
	tempL=reply_in();	//LSB of temp
	tempH=reply_in();	//MSB of temp
	sensor_rst_in();
	sensor_rst_in();
	cmnd_w_in(0xCC);	//skip ROM command
	cmnd_w_in(0x44);	//start conversion command
    
    tempS = tempL + (tempH << 8);	//calculate temp
    if(tempS<0){
        minus=1; 
        temp=65536-tempS;   //convert to unsigned integer
    }else{
        minus=0;
        temp=(unsigned int)tempS;
    }
    
    d_in[2]=((temp & 15)*10)/16;
    temp=temp/16;
    d_in[0]=(temp/10) %10; 	//digit on left
    d_in[1]=temp %10;	
    
 	if(minus){drawChar2(0x2D, 4, 1);}else{drawChar2(0x20, 4, 1);}	//- sign
    drawChar2(d_in[0]+48, 4, 2); //H 0x48}
	drawChar2(d_in[1]+48, 4, 3);
	drawChar2(d_in[2]+48, 4, 5);

}

void bell(void){			//alarm
	unsigned char i=0,j;
	
	drawChar2(0x81, 1, 8);
	bz1=0;
	while(i<30){				//soft bell
		if(!start_stop){drawChar2(0x20, 1, 8); break;} //stop the alarm
		i++; 
 			for(j=1;j<50;j++){		//buzzer output 400hz 
				bz2=1; //bz1=0;
				__delay_us(2000);	//
				bz2=0; //bz1=1;
				__delay_us(2000);
			}
		__delay_ms(1000);
        icon ^=1;
        if(icon) {drawChar2(0x20, 1, 8);}else{drawChar2(0x81, 1, 8);}
	}
	i=0;
	while(i<170){
		if(!start_stop){drawChar2(0x20, 1, 8); break;}	//
		i++;
			for(j=1;j<125;j++){		//buzzer output 2000hz 
				bz2=1; 
                bz1=0;
				__delay_us(150);	//150=3.2khz, 250=2khz
				bz2=0; 
                bz1=1;
				__delay_us(150);
			}
		__delay_ms(1000);
        icon ^=1;
        if(icon) {drawChar2(0x20, 1, 8);}else{drawChar2(0x81, 1, 8);}
	}	
	display(); //
    drawChar2(0x20, 1, 8);
}

unsigned char reply_in (void){			//reply from sensor
	unsigned char ret=0,i;
	
	for(i=0;i<8;i++){	//read 8 bits
		si_out; si=0; __delay_us(2); si_in; __delay_us(6);
		if(si){ret += 1 << i;} __delay_us(80);	//output high=bit is high
	}
	si_in;
	return ret;
}

void cmnd_w_in(unsigned char cmnd){	//send command temperature sensor
	unsigned char i;
	
	for(i=0;i<8;i++){	//8 bits 
		if(cmnd & (1 << i)){si_out; si=0; __delay_us(2); si_in; __delay_us(80);}
		else{si_out; si=0; __delay_us(80); si_in; __delay_us(2);}	//hold output low if bit is low 
	}
	si_in;
	
}

unsigned char sensor_rst_in(void){			//reset the temperature
	
	si_out;
	si=0; __delay_us(600);
	si_in; __delay_us(100);
	__delay_us(600);
	return si;	//return 0 for sensor present
	
}

unsigned int addr=0x78;  //

void sendData(unsigned int dataB)
{
    SSPIF=0;          // clear interrupt
    SSPBUF = dataB;              // send dataB
    while(!SSPIF);    // Wait to send
}

void startBit(void)
{
    SSPIF=0;
    SEN=1;          // start bit
    while(!SSPIF);
}

void stopBit(void)
{
    SSPIF=0;
    PEN=1;          // send stop
    while(!SSPIF);
}

void command( unsigned char comm){
    
    startBit();
    sendData(addr);            // address
    sendData(0x00);
    sendData(comm);             // command code
    stopBit();
}

void oled_init() {
    
    command(0xAE);   // DISPLAYOFF
    command(0x8D);         // CHARGEPUMP *
    command(0x14);     //0x14-pump on
    command(0x20);         // MEMORYMODE
    command(0x0);      //0x0=horizontal, 0x01=vertical, 0x02=page
    command(0xA1);        //SEGREMAP * A0/A1=top/bottom 
    command(0xC8);     //COMSCANDEC * C0/C8=left/right
    command(0xDA);         // SETCOMPINS *
    command(0x12);   //0x22=4rows, 0x12=8rows
    command(0x81);        // SETCONTRAST
    command(0x9F);     //0x8F
//    command(0xD5);  //SETDISPLAYCLOCKDIV 
//    command(0x80);  
//    command(0xA8);       // SETMULTIPLEX
//    command(0x3F);     //0x1F
//    command(0xD3);   // SETDISPLAYOFFSET
//    command(0x0);  
//    command(0x40); // SETSTARTLINE  
//    command(0xD9);       // SETPRECHARGE
//    command(0xF1);
//    command(0xDB);      // SETVCOMDETECT
//    command(0x40);
//    command(0xA4);     // DISPLAYALLON_RESUME
 //   command(0xA6);      // NORMALDISPLAY
    command(0xAF);          //DISPLAYON

}

void clrScreen()    //fill screen with 0
{
    unsigned char y, i;
    
    for ( y = 0; y < 8; y++ ) {
    command(0xB0 + y);     //0 to 7 pages
    command(0x00); //low nibble
    command(0x10);  //high nibble
    
        startBit();
        sendData(addr);            // address
        sendData(0x40);
        for (i = 0; i < 128; i++){
             sendData(0x0);
        }
        stopBit();
    }    
}

    //size 1 chars
void drawChar(char fig, unsigned char y, unsigned char x)
{
    
    command(0x21);     //col addr
    command(7 * x); //col start
    command(7 * x + 4);  //col end
    command(0x22);    //0x22
    command(y); // Page start
    command(y); // Page end
    
        startBit();
        sendData(addr);            // address
        sendData(0x40);
        for (unsigned char i = 0; i < 5; i++){
             sendData(font[5*(fig-32)+i]);
        }
        stopBit();
        
 
 }
    //size 2 chars
void drawChar2(char fig, unsigned char y, unsigned char x)
{
    unsigned char i, line, btm, top;    //
    
    command(0x20);    // vert mode
    command(0x01);

    command(0x21);     //col addr
    command(13 * x); //col start
    command(13 * x + 9);  //col end
    command(0x22);    //0x22
    command(y); // Page start
    command(y+1); // Page end
    
        startBit();
        sendData(addr);            // address
        sendData(0x40);
        
        for (i = 0; i < 5; i++){
            line=font[5*(fig-32)+i];
            btm=0; top=0;
                // expend char    
            if(line & 64) {btm +=192;}
            if(line & 32) {btm +=48;}
            if(line & 16) {btm +=12;}           
            if(line & 8) {btm +=3;}
            
            if(line & 4) {top +=192;}
            if(line & 2) {top +=48;}
            if(line & 1) {top +=12;}        

             sendData(top); //top page
             sendData(btm);  //second page
             sendData(top);
             sendData(btm);
        }
        stopBit();
        
    command(0x20);      // horizontal mode
    command(0x00);    
        
}





