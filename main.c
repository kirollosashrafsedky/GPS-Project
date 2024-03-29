#include "stdint.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "tm4c123gh6pm.h"

#define PI 3.14159265359
#define R 6371000
#define DISTINATION_OFFSET 10
#define MAX_WIFI_TRIES 10
#define THRESHOLD_SPEED 0.5

uint8_t  numbersArr[10] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
double distance = 0;
uint8_t currentNumber=0;
uint8_t reachedDistination=0;
double lastPoint[2]={0,0};
double currentPoint[2]={0,0};
double destinationPoint[2]={30.063921528763213,31.280019999858254};
char wifiDataBuffer[500];
char *wifiDataPtr = wifiDataBuffer;
uint8_t currentWifiCommand = 0;
char currentWifiCommandCheck[20];
uint8_t wifiTries = 0;


void SystemInit(void){
	NVIC_CPAC_R |= 0x00F00000;		//enable FPU - Floating-point numbers
  }

void delay(uint32_t times){
	int i ;
	for( i = 0; i < times/5 ; i++)
		{while((NVIC_ST_CTRL_R & 0x10000) == 0){}
		}
	}

void systick_init(){
		NVIC_ST_CTRL_R = 0;									//disable timer
		NVIC_ST_RELOAD_R = (16000*5 - 1);		//time, every (16000-1) = 1 millisec
		NVIC_ST_CURRENT_R = 0;							//clear current r, clear count flag
		NVIC_ST_CTRL_R |= 0x07;							//enable timer and set clck src and start inturrept
    }

void portF_led_init(){
	SYSCTL_RCGCGPIO_R |= 0x20;   /* enable clock to GPIOF */
    while((SYSCTL_PRGPIO_R & 0x20)==0);

  GPIO_PORTF_LOCK_R = 0x4C4F434B; // unlock
  GPIO_PORTF_CR_R = 0x0F;
  GPIO_PORTF_AFSEL_R &=~(0x0E); // disable alternative functions
  GPIO_PORTF_PCTL_R = 0;
  GPIO_PORTF_DIR_R = 0x0E;        // set pins 1,2,3 as output

  GPIO_PORTF_AMSEL_R &= ~(0x0E);
  GPIO_PORTF_DEN_R = 0x0E;         /* set PORTF pins 1 to 4 as digital pins */
	}

void portE_enables_init (void){
	SYSCTL_RCGCGPIO_R |=0x10;
		while (( SYSCTL_PRGPIO_R &0x10) == 0);

	GPIO_PORTE_LOCK_R=0x4c4f434b ;
	GPIO_PORTE_CR_R |=0X1E ;
	GPIO_PORTE_AFSEL_R &=~ 0X1E;
	GPIO_PORTE_PCTL_R = 0;
	GPIO_PORTE_AMSEL_R &=~0X1E;

	GPIO_PORTE_DEN_R |=0X1E;
	GPIO_PORTE_DIR_R |=0X1E;

	}

void portB_segments_init(void){
  SYSCTL_RCGCGPIO_R |= 0x02;     //Enables port B
	  while((SYSCTL_PRGPIO_R & 0x02) == 0){}  //waits until port is into clock

	GPIO_PORTB_LOCK_R = 0x4C4F434B;
	GPIO_PORTB_CR_R|= 0x7F; // unlock first seven pins
	GPIO_PORTB_AMSEL_R &= ~0x7F;
	GPIO_PORTB_AFSEL_R&=~0x7F;
	GPIO_PORTB_DEN_R|=0x7F;

	GPIO_PORTB_PCTL_R = 0x00000000; // used as GPIO so PCTL and AMSEL are reset
	GPIO_PORTB_DIR_R|=0x7F;

  }

void uart_Gps_Init(void){
  SYSCTL_RCGCUART_R |= 0x08; //unlock UART3
	SYSCTL_RCGCGPIO_R |= 0x04;   // ENABLE clock for C
		while((SYSCTL_PRGPIO_R & 0x04) == 0){}

	GPIO_PORTC_LOCK_R = 0x4C4F434B; // remove lock
	GPIO_PORTC_CR_R = 0xC0;

	UART3_CTL_R&=~(0x01);
	UART3_IBRD_R=104;  // Baud Rate at 9600
	UART3_FBRD_R=11;   // fraction of Baud rate
	UART3_LCRH_R=0x0070; // 1 stop bit and 8 bit length and also parity off
	UART3_CTL_R=0x0301;

	GPIO_PORTC_AFSEL_R |=0xC0;
	GPIO_PORTC_PCTL_R = (GPIO_PORTC_PCTL_R & 0x00FFFFFF ) + 0x11000000; // use last two pins as UART
	GPIO_PORTC_DEN_R |= 0xC0;
	GPIO_PORTC_AMSEL_R &= ~0xC0;
	}

void uart_Wifi_Init(){
	SYSCTL_RCGCUART_R |= 0x04;            // activate UART2
  SYSCTL_RCGCGPIO_R |= 0x08;            // activate port D
   while((SYSCTL_PRGPIO_R&0x08) == 0){};
	GPIO_PORTD_LOCK_R = GPIO_LOCK_KEY;
	GPIO_PORTD_CR_R = 0xC0;
  GPIO_PORTD_AFSEL_R |= 0xC0;           // enable alt funct on PD6-7
  GPIO_PORTD_DEN_R |= 0xC0;             // enable digital I/O on PD6-7
                                        // configure PD6-7 as UART
	GPIO_PORTD_PCTL_R = (GPIO_PORTD_PCTL_R&0x00FFFFFF)+0x11000000;
  GPIO_PORTD_AMSEL_R &= ~0xC0;          // disable analog functionality on PD

  UART2_CTL_R &= ~(0x01);      					// disable UART
  UART2_IBRD_R = 8;                   	// IBRD = int(16,000,000 / (16 * 9600)) = 104
  UART2_FBRD_R = 44;                    // FBRD = int(0.1667 * 64 + 0.5) = 11
  UART2_LCRH_R = 0x60;									// 8 bit word length (no parity bits, one stop bit, FIFOs)
  UART2_CTL_R |= 0x301;                 // enable UART
	UART2_ICR_R = 0x10;
	UART2_IM_R |= 0x10;
	NVIC_EN1_R |= 0x02;
  }

char uartGpsReadChar(void){
	while((UART3_FR_R & 0x010)!=0){}
	return(char)(UART3_DR_R &0xff);
  }

void uartGpsReadLine(char *enteredStr, uint32_t maxSize){
	uint32_t length = 0;
	while(uartGpsReadChar() != '$'){}

	while(length < maxSize){
			*enteredStr = uartGpsReadChar();
			if(*enteredStr  == '*') 	break;
			enteredStr ++;
			length++;
		  }

	enteredStr ++;
	*enteredStr = '\0';
 }

void uartWifiWriteChar(char data){
	while((UART2_FR_R & 0x0020) != 0);
	UART2_DR_R = data;
  }

void uartWifiWriteString(char * data){
	int n = strlen(data);
	while(n>0){
		uartWifiWriteChar(data[strlen(data)-n]);
		n=n-1;
	  }
  }

double getDistance(double p1[],double p2[]){
   double lat1=p1[0];
   double lon1=p1[1];
   double lat2=p2[0];
   double lon2=p2[1];
	 double x1 = lat1*(PI/180);
   double x2 = lat2*(PI/180);
   double dx = (lat2-lat1)*(PI/180);
   double dy = (lon2-lon1)*(PI/180);

   double a=sin((dx/2))*sin((dx/2))+cos((x1))*cos((x2))
         *sin((dy/2))*sin((dy/2));
   double c=2*atan2(sqrt(a),sqrt((1-a)));
   double d=(R*c);

   return d;
   }

__irq void SysTick_Handler(){
	uint32_t number = 0;
	uint32_t intDistance = (uint32_t)distance;
	GPIO_PORTE_DATA_R &= ~(0x17);
	if(intDistance < 10000){
		if(currentNumber == 0){
			number = intDistance % 10;
		  }
		else if(currentNumber == 1){
			number = intDistance % 100;
			number /= 10;
		  }
		else if(currentNumber == 2){
			number = intDistance % 1000;
			number /= 100;
		  }
		else{
			number = intDistance / 1000;
		  }

	  GPIO_PORTB_DATA_R = numbersArr[number];
	  GPIO_PORTE_DATA_R = (1 << (currentNumber + 1));
	 }
  else{
		GPIO_PORTB_DATA_R = numbersArr[1];
		GPIO_PORTE_DATA_R = (1 << 4);
	  }
	if(currentNumber == 3) currentNumber = 0;
	else currentNumber++;
  }

void getCoordinates(char parsedStr[15][20], double coordinates[2]){
	double inputLat = atof(parsedStr[3]); //lat
	double inputLon = atof(parsedStr[5]); //lon
	double latDegrees = ((int)inputLat / 100) + (fmod(inputLat, 100) / 60);
	double lonDegrees = ((int)inputLon / 100)  + (fmod(inputLon, 100) / 60);

	if(strcmp(parsedStr[4], "S") == 0)
		coordinates[0] = -1 * latDegrees;
	else
		coordinates[0] = latDegrees;

	if(strcmp(parsedStr[6], "W") == 0)
		coordinates[1] = -1 * lonDegrees;
	else
		coordinates[1] = lonDegrees;
  }

uint8_t parseString(char * str, char parsedStr[15][20]){
	uint8_t i = 0, j = 0;
	while(*str != '\0'){
		if(*str == ','){
			parsedStr[i][j] = '\0';
			i++;
			j = 0;
		  }
		else{
			parsedStr[i][j] = *str;
			j++;
		    }
	str++;
	}

	if(strcmp(parsedStr[2], "A") != 0){
		return 0;
	  }

	return 1;
  }

uint8_t validateData(char *str){
		char x[85];
		char *y;
		char n[] = "GPRMC";
		int z;
		strcpy(x, str);
    y=strtok(x,",");
    z= strcmp ( n , y);
		if (z==0){
			return 1;
		  }
		return 0;
    }

void setLastPoint(void){
  lastPoint[0]=currentPoint[0];
  lastPoint[1]=currentPoint[1];
  }

void initFunc(void){

	systick_init();
	portE_enables_init();
	portB_segments_init();
	portF_led_init();
	uart_Gps_Init();
	uart_Wifi_Init();
	GPIO_PORTF_DATA_R|= 0x02;
  }

__irq void UART2_Handler(){
	char data = UART2_DR_R;
	if(wifiDataPtr - wifiDataBuffer < 490){
		*wifiDataPtr = data;
	  wifiDataPtr ++;
	  *wifiDataPtr = '\0';
	  }
  }

void WifiCommands(char dataToBeSent[85]){
	if(currentWifiCommand == 0 || strstr(wifiDataBuffer, currentWifiCommandCheck) != NULL){
		char postDataLengthStr[5], dataLengthStr[5], distanceStr[10];
		uint8_t dataLength, postDataLength;
		uint32_t intDistance = (uint32_t) distance;
		char postData[300] = "POST /postdata HTTP/1.1\r\nHost: gps-embedded-project.herokuapp.com\r\nAccept: */*\r\nContent-Length: ";
		sprintf(distanceStr, "%d", intDistance);
		dataLength = strlen(dataToBeSent) + strlen(distanceStr) + 15; //15 is due to additional chars data=&distance=
		sprintf(dataLengthStr, "%d", dataLength);
		strcat(postData, dataLengthStr);
		strcat(postData, "\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\ndata=");
		strcat(postData, dataToBeSent);
		strcat(postData, "&distance=");
		strcat(postData, distanceStr);
		postDataLength = strlen(postData);
		sprintf(postDataLengthStr, "%d", postDataLength);
		strcpy(wifiDataBuffer, "");
		wifiTries = 0;
		wifiDataPtr = wifiDataBuffer;
		switch(currentWifiCommand){
			case 0:
				uartWifiWriteString("AT\r\n");
				strcpy(currentWifiCommandCheck, "OK\r\n");
				currentWifiCommand = 1;
				break;
			case 1:
				uartWifiWriteString("AT+CWMODE=1\r\n");
				strcpy(currentWifiCommandCheck, "OK\r\n");
				currentWifiCommand = 2;
				break;
			case 2:
				uartWifiWriteString("AT+CWJAP=\"gps-project\",\"123456789\"\r\n");
				strcpy(currentWifiCommandCheck, "OK\r\n");
				currentWifiCommand = 3;
				break;
			case 3:
				uartWifiWriteString("AT+CIPSTART=\"TCP\",\"gps-embedded-project.herokuapp.com\",80\r\n");
				strcpy(currentWifiCommandCheck, "OK\r\n");
				currentWifiCommand = 4;
				break;
			case 4:
				uartWifiWriteString("AT+CIPSEND=");
				uartWifiWriteString(postDataLengthStr);
				uartWifiWriteString("\r\n");
				currentWifiCommand = 5;
				break;
			case 5:
				uartWifiWriteString(postData);
				strcpy(currentWifiCommandCheck, "terminator");
				currentWifiCommand = 6;
				break;
			case 6:
				uartWifiWriteString("AT+CIPCLOSE\r\n");
				strcpy(currentWifiCommandCheck, "CLOSED\r\n");
				currentWifiCommand = 3;
				break;
		}
	}
	else{
	 if(wifiTries <= MAX_WIFI_TRIES){
				wifiTries ++;
		}
		else{
			currentWifiCommand = 0;
			wifiTries = 0;
		}
	 }
 }

int main(void){
	 uint8_t firstReading = 1;
	 uint8_t secondReading = 0;
	 initFunc();
	 __enable_irq();
	delay(1000);
	WifiCommands("connecting");
	while(1){
		double speed;
		if(reachedDistination == 0){
			char receivedStr[85];
			uint8_t validData = 0;
			char parsedStr[15][20];
			while(validData == 0){
				uartGpsReadLine(receivedStr, 85);
				if(validateData(receivedStr) == 1) validData = 1;
			  }
			if(parseString(receivedStr, parsedStr) == 0) {
				GPIO_PORTF_DATA_R ^= 0x02;
				WifiCommands("connecting");
				firstReading = 1;
				continue;
			  }
			if(firstReading) {
				delay(3000);
				firstReading = 0;
				secondReading = 1;
				continue;
			  }
			getCoordinates(parsedStr, currentPoint);
			if(secondReading) {
				secondReading = 0;
				setLastPoint();
				GPIO_PORTF_DATA_R &= ~(0x02);
			  }
			speed = atof(parsedStr[7]);
			if(speed >= THRESHOLD_SPEED){
				distance  += getDistance(currentPoint, lastPoint);
			  }
			setLastPoint();
			if(getDistance(currentPoint, destinationPoint) <= DISTINATION_OFFSET) reachedDistination = 1;

			WifiCommands(receivedStr);
			delay(500);
			WifiCommands(receivedStr);
		}
		else{
			GPIO_PORTF_DATA_R |= 0x08;
			while(1){
				WifiCommands("reached distination");
				delay(1000);
			  }
		  }
		}
  }
