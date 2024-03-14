//configuration functions
void CLK_Config(void);	//configure all the clock neccessery for the program
void GPIO_Config(void);
void TMR0_Config(void);		// for 7segments 
void TMR1_Config(void);
void SPI_Config(void);	//setting SPI for transmitting and receiving signal
void UART_Config(void);	//set up UART to communicate with other module

//Functions
int scanKeyPad(void);
void initShotMap(void);
void displayNum(int num1, int num2);
void displayMap(void);
enum state nxtTrn(int x, int y);

//Interrupt
void UART02_IRQHandler (void);
void EINT1_IRQHandler (void);
void TMR0_IRQHandler (void);
void TMR1_IRQHandler (void);