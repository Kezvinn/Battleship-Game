/*
Course: 		EEET2481 - Embedded System Design and Implementation
Lecturer: 	Dr. Hung Pham Viet
Assignment: 3
Student: 		Nhat Nguyen
sID: 				s3818247
Created: 		21 Jan. 2024
*/
#include <stdio.h>
#include <stdbool.h>
#include "NUC100Series.h"
#include "LCD.h"
#include "System_Config.h"

//clock status
#define HXT_STATUS 1<<0		//12 MHz
#define HIRC_STATUS 1<<4 	//22.1184 MHz
#define LIRC_STATUS 1<<3	//10KHz
#define PLL_STATUS 1<<2		

//timer count
#define TIMER0_COUNT 1000-1		//1 ms
#define TIMER1_COUNT 250000-1	//0.25s

//map dimensions
#define ROWs 8
#define COLs 8

static volatile uint8_t tmr0_count = 0;
static volatile uint8_t bits_count = 0;
static volatile uint8_t shot_count = 0;
static volatile uint8_t blinks = 0;
static volatile uint8_t beeps = 0;

static volatile bool game_start_flag = false;
static volatile bool game_over_flag = false;
static volatile bool LED_flag = false;
static volatile bool Buzzer_flag = false;
static volatile bool coord_lock = false;

//map
static volatile uint8_t map[ROWs][COLs];	//store map of ship
static uint8_t shot_map[ROWs][COLs];	//store map of shot

//state
enum state {GAME, SHOT, WIN, LOSE};
enum select {X,Y};
static volatile enum select selected = X;

//initial value of coordinate(x,y)
static int coord_x = -1;	
static int coord_y = -1;	

static uint8_t pattern[] = {	// pattern for 7-segments
	0b10000010,	// 0
	0b11101110,	// 1
	0b00000111,	// 2
	0b01000110,	// 3
	0b01101010,	// 4
	0b01010010,	// 5
	0b00010010,	// 6
	0b11100110,	// 7
	0b00000010,	// 8
	0b01000010	// 9
};


int main(void) {	
	CLK_Config();			//clock CPU
	GPIO_Config();		//GPIO
	TMR0_Config();		//for 7segments 
	TMR1_Config();		//for led and buzzer
	SPI_Config();			//setting SPI for transmitting and receiving signal
	UART_Config();		//for UART communication
		
	clear_LCD();
//	SYS->ALT_MFP |= 1<<24;
	
	printS_5x7(48,0,"Welcome");
	printS(5,10,"BATTLESHIP GAME");
	printS_5x7(20,45,"Input map to begin.");

	// Wait for the map to be sent
	while (bits_count < 64){
		PC->DOUT ^= (1<<12);
		CLK_SysTickDelay(120000);
		PC->DOUT ^= (1<<13);
		CLK_SysTickDelay(120000);
		PC->DOUT ^= (1<<14);
		CLK_SysTickDelay(120000);
		PC->DOUT ^= (1<<15);
	}
	PC->DOUT |= (0xF <<12);
		
	clear_LCD();
	printS_5x7(48,0,"Welcome");
	printS(5,10,"BATTLESHIP GAME");
	printS_5x7(8,40,"Map Loaded Successfully.");
	printS_5x7(12,50, "Press button to begin.");
	
	// Wait for player to start the game
	while(!game_start_flag);
	
	initShotMap();
	clear_LCD();
	displayMap();
	
	unsigned int c = 0, p_c = 0;
	
	while (1) {
		int key = scanKeyPad();
		c++;
		if (key && !game_over_flag && (c - p_c > 100000)) { 	//100000 = 2ms count delay
			p_c = c;	
			if (key == 9) {// XY selection
				if (selected == X){selected = Y;}
				else { selected = X;}
			} else {	// Coordinate selection
				if (selected == X){ coord_x = key; }
				else { coord_y = key; }
			}
		}
		
		if (coord_lock) {
			enum state stt = nxtTrn(coord_x-1, coord_y-1);	//get the state for the game
			switch (stt) {
				case GAME:
					clear_LCD();
					displayMap();
					LED_flag = true;
					break;
				case SHOT:	
					LED_flag = false;	//turn off LED
					break;
				case WIN:
					clear_LCD();
					printS(5,10,"BATTLESHIP GAME");
					printS(36, 24, "You Win");
					PC->DOUT &= ~(0xF<<4);	//turn off all segments
					game_over_flag = true;	//indicate for 
					Buzzer_flag = true;
					break;
				case LOSE:
					clear_LCD();
					printS(5,10,"BATTLESHIP GAME");
					printS(30, 24, "You Lose");								
					PC->DOUT &= ~(0xF<<4);	//turn off all segment
					game_over_flag = true;
					Buzzer_flag = true;
					break;
			}
			coord_lock = false;
			coord_x = -1;	//reset to start value
			coord_y = -1;	//reset to start value
			selected = X;	//back for coord_X
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------------------
// Function definitions
//------------------------------------------------------------------------------------------------------------------------------------
int scanKeyPad(void) {	//scan keypad for value
	PA0 = 1; PA1 = 1; PA2 = 0; PA3 = 1; PA4 = 1; PA5 = 1;
	if (PA3 == 0) return 1;
	if (PA4 == 0) return 4;
	if (PA5 == 0) return 7;
	PA0 = 1; PA1 = 0; PA2 = 1; PA3 = 1; PA4 = 1; PA5 = 1;
	if (PA3 == 0) return 2;
	if (PA4 == 0) return 5;
	if (PA5 == 0) return 8;
	PA0 = 0; PA1 = 1; PA2 = 1; PA3 = 1; PA4 = 1; PA5 = 1;
	if (PA3 == 0) return 3;
	if (PA4 == 0) return 6;
	if (PA5 == 0) return 9;
	return 0;
}

void initShotMap() {
	for(int i = 0; i < ROWs; i++) {
		for(int j = 0; j < COLs; j++) {
			shot_map[i][j] = 0;	//start shot map register as 0
		}
	}
}

void displayMap(void) {	//display 3 digits
	for (int row = 0; row < ROWs; row++) {
		for (int col = 0; col < COLs; col++) {
			char temp = (shot_map[row][col] && map[row][col])? 'X' : '-';
			printC_5x7(col*8+32, row*8, temp);
		}
	}
}

void displayNum(int num1, int num2) {
	PC->DOUT &= ~(0xFul << 4);	// Turn off all 4 digits
	if (tmr0_count == 1) {	// shot_10
		PC->DOUT |= (1 << 5);
		PE->DOUT = pattern[num2 / 10];
	} else if (tmr0_count == 2) {	// shot_1
		PC->DOUT |= (1 << 4);
		PE->DOUT = pattern[num2 % 10];		
	} else {												// coordinate 1-8
		PC->DOUT |= (1 << 7);
		PE->DOUT = pattern[num1 == -1 ? 0 : num1];
		if (selected == Y){
			PE->DOUT &= ~(0x1ul << 1);
		}
		tmr0_count = 0;
	}
}

enum state nxtTrn(int x, int y) {
	shot_count++;
	if (shot_count > 16) {
		return LOSE;
	}
	
	if (!map[y][x] || (map[y][x] && shot_map[y][x])) {	//if map != 1 or recursive shot (twice in same location)
		return SHOT;
	}
	
	
	shot_map[y][x] = 1;		//coordinate flip due to maping 
	for (int row = 0; row < ROWs; row++) {
		for (int col = 0; col < COLs; col++) {
			if (map[row][col] && !shot_map[row][col]) {	//shot in map and shotmap matched -> game
				return GAME;
			}
		}
	}
	return WIN;	//if no condition match --> win
}

//------------------------------------------------------------------------------------------------------------------------------------
// Interrupt definitions
//------------------------------------------------------------------------------------------------------------------------------------
void UART02_IRQHandler(void) {
	char bit = UART0->RBR;	//receive 
	if (bit == '0' || bit == '1') {
		map[bits_count / ROWs][bits_count % COLs] = bit - '0';
		bits_count++;
	}
}

// Timer 0 interrupt function for 7 segment scanning
void TMR0_IRQHandler(void) {
	// Display the selected coordinate and turn number
	if (game_start_flag && !game_over_flag){
		if (selected == X){
			displayNum(coord_x,shot_count);
		} else {
			displayNum(coord_y,shot_count);
		}
//		displayNum(selected == X ? coord_x : coord_y, shot_count);
	}
	tmr0_count++;
	
	TIMER0->TISR |= (1 << 0);	//clear timer interrupt
}
// Timer 1 interrupt function for LED and buzzer
void TMR1_IRQHandler(void) {
	if (LED_flag) {
		PC->DOUT ^= (1 << 12);
		blinks++;
		if (blinks >= 6) {
			PC->DOUT |= (1 << 12);
			LED_flag = false;
			blinks = 0;
		}
	}
	if (Buzzer_flag) {
		PB->DOUT ^= (1 << 11);
		beeps++;
		if (beeps >= 10) {
			PB->DOUT |= (1 << 11);
			Buzzer_flag = false;
			beeps = 0;
		}
	}
	TIMER1->TISR |= (1 << 0);
}

// GPIO-B15 interrupt function
void EINT1_IRQHandler(void) {
	if (bits_count >= 64) {
		game_start_flag = true;
	}
	if (game_start_flag) {
		if (coord_x != -1 && coord_y != -1){
			coord_lock = true;
		}
		
		if (game_over_flag) {
			game_over_flag = false;
			shot_count = 0;
			initShotMap();
			clear_LCD();
			displayMap();
		}
	}
	PB->ISRC |= (1 << 15);	//clear interrupt flag
}

//------------------------------------------------------------------------------------------------------------------------------------
// System configuration definitions
//------------------------------------------------------------------------------------------------------------------------------------
void CLK_Config(void){
	/*
	CPU_CLK = 50 MHz
	Enable Timer0 clock
	Timer1 for flashing frequency of LED5 (GPC12) 3 times
	Timer1 for multiplexing 7segments = 
	*/
	SYS_UnlockReg(); // Unlock protected registers
	
	CLK->PWRCON |= (1<<0);	//HXT = 12MHz
	while(!(CLK->CLKSTATUS & HXT_STATUS));
	CLK->PWRCON |= (1<<2); 	//HIRC_STATUS = 22.1184MHz
	while(!(CLK->CLKSTATUS & HIRC_STATUS));
	CLK->PWRCON |= (1<<3);	//LIRC = 10 kHz
	while(!(CLK->CLKSTATUS &(LIRC_STATUS)));
	
	CLK->PWRCON &= ~(1 << 7);	//normal mode
	
//PLL configuration starts
	CLK->PLLCON &= ~(1 << 19); //0: PLL input is HXT
	CLK->PLLCON &= ~(1 << 16); //PLL in normal mode
	CLK->PLLCON &= (~(0xFFFFul << 0));
//50MHz
	CLK->PLLCON |= 23 << 0;	//NF
	CLK->PLLCON |= 1 << 9;	//NR
	CLK->PLLCON |= 2 << 14;	//NO

	CLK->PLLCON &= (~(1 << 18));	//enable PLL
	while(!(CLK->CLKSTATUS & PLL_STATUS));
//PLL configuration ends
	

//clock source selection
	CLK->CLKSEL0 &= (~(0b111 << 0));	//clear
	CLK->CLKSEL0 |= (0x02 << 0);	//PLL source for CPU
	
//clock frequency division
	CLK->CLKDIV &= ~(0xFul << 0);	// no clock division = 1
	
//Enable clock for SPI
	CLK->APBCLK |= (1 << 15);	//SPI3 clock enable

//Enable clock for UART0
	CLK->CLKSEL1 &= (~(0b11 << 24));	//clear
	CLK->CLKSEL1 |= (0b11 << 24);	//use 22.1184MHz
	CLK->CLKDIV &= ~(0xFul << 8);	//UART clock divider = 1
	CLK->APBCLK |= (1 << 16);	//UART0 clock enable

//Enable clock for TIMER0
	CLK->CLKSEL1 &= (~(0b111 << 8));	//clear
	CLK->CLKSEL1 |= (0b010 << 8);		//use clock source of HCLK = 50MHz
	CLK->APBCLK |= (0b1 << 2);	//timer0 clock enable

//Enable clock for TIMER1
	CLK->CLKSEL1 &= (~(0b111 << 12));	//clear
	CLK->CLKSEL1 |= (0b010 << 12);		//use clock source of HCLK = 50MHz
	CLK->APBCLK |= (0b1 << 3);	//timer1 clock enable
	
	SYS_LockReg(); // Lock protected registers
}
void GPIO_Config(void){
//buzzer: output
	PB->PMD &= ~(0b11 << 22);	//clear
	PB->PMD |= (0b01 << 22);	//set output
	
//LED5: output
//	PC->PMD &= ~(0b11<<24);
//	PC->PMD |= (0b01<<24);	//output
	
//key matrix: bidirectional
	PA->PMD &= (~(0xFFF << 0));
	PA->PMD |= (0xFFF << 0);	//quasi-directional
	
//7-segmenets
	PC->PMD &= (~(0xFF << 8));	// 4 pin control 7-segments
	PC->PMD |= (0x55 << 8);		//set output
	PE->PMD &= (~(0xFFFF << 0));	//8 pin control each segment
	PE->PMD |= (0x5555ul << 0);				//set output

//4 LED 
	PC->PMD &= (~(0xFFul << 24));
	PC->PMD |= (0x55ul << 24);	

//button 
	PB->PMD &= (~(0b11 << 30));	//input
	PB->IMD &= (~(1 << 15));		//edge trigger
	PB->IEN |= (1 << 15); 			//fallin edge trigger
	PB->DBEN |= (1 << 15);	//enable debounce

	GPIO->DBNCECON |= (1<< 4);	//use LIRC = 10KHz
	GPIO->DBNCECON &= ~(0xF<<0);
	GPIO->DBNCECON &= ~(0xF<<0);	//11 = 8*256 --> 2048/10000 = 0.2s
	
// NVIC interrupt configuration for GPIO-B15 interrupt source
	NVIC->ISER[0] |= (1 << 3); //enable interrupt
//	NVIC->IP[0] &= (~(0b11 << 30)) ;	// priority 3 (lowest)	
}
void TMR0_Config(void){
//recalculate	
	//prescale = 49+1	--> 1 Mhz
	TIMER0->TCSR &= ~(0xFF << 0);
	TIMER0->TCSR |= 49 << 0;

	//reset Timer 0
	TIMER0->TCSR |= (1 << 26);

	//define Timer 0 operation mode
	TIMER0->TCSR &= ~(0b11 << 27);	//clear
	TIMER0->TCSR |= (0b01 << 27);	//periodic mode
	TIMER0->TCSR &= ~(1 << 24);	//disable counter mode
	//TDR to be updated continuously while timer counter is counting
	TIMER0->TCSR |= (1 << 16);	//update count value

//Enable TE bit (bit 29) of TCSR
//The bit will enable the timer interrupt flag TIF
	NVIC->ISER[0] |= (1 << 8);
	TIMER0->TCSR |= (1 << 29);

//TimeOut = 1ms --> top count == 1000-1
	TIMER0->TCMPR = TIMER0_COUNT;

//Start counting
	TIMER0->TCSR |= (1 << 30);
}

void TMR1_Config(void){
	//prescale = 49+1	--> 1 Mhz
	TIMER1->TCSR &= ~(0xFF << 0);
	TIMER1->TCSR |= 49 << 0;

	//reset Timer 0
	TIMER1->TCSR |= (1 << 26);

	//define Timer 0 operation mode
	TIMER1->TCSR &= ~(0b11 << 27);	//clear
	TIMER1->TCSR |= (0b01 << 27);	//periodic mode
	TIMER1->TCSR &= ~(1 << 24);	//disable counter mode
	//TDR to be updated continuously while timer counter is counting
	TIMER1->TCSR |= (1 << 16);	//update count value

//Enable TE bit (bit 29) of TCSR
//The bit will enable the timer interrupt flag TIF
	NVIC->ISER[0] |= (1 << 9);	
	TIMER1->TCSR |= (1 << 29);	//enable interrupt
	
//TimeOut = 0.25s --> Counter's TCMPR = 250000-1
	TIMER1->TCMPR = TIMER1_COUNT;

	PB->PMD &= ~(0b11<<18);
	PB->PMD |= (0b01<<18);
	
	SYS->GPB_MFP |= 1<<9;
//Start counting
	TIMER1->TCSR |= (1 << 30);
}

void SPI_Config(void){
//GPIO reconfigure
	SYS->GPD_MFP |= 1 << 11; //1: PD11 is configured for alternative function
	SYS->GPD_MFP |= 1 << 9; //1: PD9 is configured for alternative function
	SYS->GPD_MFP |= 1 << 8; //1: PD8 is configured for alternative function
	
	//SPI configuration
	SPI3->CNTRL &= ~(1 << 23); //0: disable variable clock feature
	SPI3->CNTRL &= ~(1 << 22); //0: disable two bits transfer mode
	SPI3->CNTRL &= ~(1 << 18); //0: select Master mode
	SPI3->CNTRL &= ~(1 << 17); //0: disable SPI interrupt

	SPI3->CNTRL |= 1 << 11; //1: SPI clock idle high
	SPI3->CNTRL &= ~(1 << 10); //0: MSB is sent first
	SPI3->CNTRL &= ~(0b11 << 8); //00: one transmit/receive word will be executed in one data transfer
	
	SPI3->CNTRL &= ~(0b11111 << 3);	//clear
	SPI3->CNTRL |= 9 << 3; //9 bits/word
	SPI3->CNTRL |= (1 << 2); //1: Transmit at negative edge of SPI CLK
	
	SPI3->DIVIDER = 24; // SPI clock divider. SPI clock = HCLK / ((DIVIDER+1)*2)
																					//SPI clock = 50 MHz/((25+1)*2) = 1 MHz
}

void UART_Config(void){
	PB->PMD &= ~(0b11 << 0);			// Set Pin Mode for GPB.0(RX - Input)
	SYS->GPB_MFP |= (1 << 0);			// GPB_MFP[0] = 1 -> PB.0 is UART0 RX pin	
	
	// UART0 operation configuration
	UART0->LCR |= (0b11 << 0);		// 8 data bits
	UART0->LCR &= ~(1 << 2);			// 1 stop bit	
	UART0->LCR &= ~(1 << 3);			// 0 parity bit

	UART0->FCR |= (1 << 2);	//reset TX
	UART0->FCR |= (1 << 1);	// reset RX
	
	UART0->FCR &= ~(0xF << 16);		// FIFO Trigger Level is 1 byte
	
	// UART0 interrupt configuration
	NVIC->ISER[0] |= (1 << 12);		// Enable UART0 IRQ
	NVIC->IP[7] &= ~(0b11 << 6);	// Set highest interrupt priority
	UART0->IER |= (1 << 0);				// Enable UART0 RDA interrupt
	UART0->FCR &= ~(0xFul << 4);		// Interrupt trigger level: 1 byte

	//BAUD rate = 115200
	UART0->BAUD &= ~(1 << 28);	//Mode 0
	UART0->BAUD &= ~(1 << 29);	//M = 16
	UART0->BAUD &= ~(0xFFFF << 0);//clear  divider x
	UART0->BAUD = 10;		//10
}

