#include <iostm8l151f3.h>
#include <intrinsics.h>

#include "nRF_SPI.h"
//for nRF_SetMode_TX()
#include "nRF_Modes.h"

#include "nRF_Tx.h"

#include "ClockUartLed.h"

#include "i2c_m.h"
#include "commonTypes.h"


#define EEPROM_Offset 0x1000
#define NODE_ID       (char *) EEPROM_Offset;
unsigned char NodeId;

//---------------------- Active Halt Mode :
// - CPU and Peripheral clocks stopped, RTC running
// - wakeup from RTC, or external/Reset

BYTE sensorData[2];

void RfAlive()
{
      unsigned char Tx_Data[3];
      Tx_Data[0]=0x75;
      Tx_Data[1]=NodeId;
      Tx_Data[2]= Tx_Data[0] ^ NodeId;
      nRF_Transmit(Tx_Data,3);
}

void Rf_Light()
{
      unsigned char Tx_Data[5];
      Tx_Data[0]=0x3B;//Light is 0x3B
      Tx_Data[1]=NodeId;
      Tx_Data[2]=sensorData[0];
      Tx_Data[3]=sensorData[1];
      Tx_Data[4]= Tx_Data[0] ^ NodeId;
      nRF_Transmit(Tx_Data,5);
}

void RfSwitch(unsigned char state)
{
      unsigned char Tx_Data[4];
      Tx_Data[0]=0xC5;
      Tx_Data[1]=NodeId;
      Tx_Data[2]=state;
      Tx_Data[3]= Tx_Data[0] ^ NodeId ^ state;
      nRF_Transmit(Tx_Data,4);
}


void LogMagnets()
{
      delay_100us();
      unsigned char Magnet_B0,Magnet_D0;
      Magnet_B0 = PB_IDR_IDR0;
      Magnet_D0 = PD_IDR_IDR0;
      //UARTPrintf(" LVD0 ");
      UARTPrintf_uint(Magnet_D0);
      //UARTPrintf(" ; HH B0 ");
      UARTPrintf_uint(Magnet_B0);
      UARTPrintf("\n");
      delay_100us();
      delay_100us();
}

//bit 0 - pin interrupt
#pragma vector = EXTI0_vector
__interrupt void IRQHandler_Pin0(void)
{
  if(EXTI_SR1_P0F == 1)
  {
    //UARTPrintf("Pin0_Interrupt ");LogMagnets();
    RfSwitch(PB_IDR_IDR0);
  }
  EXTI_SR1 = 0xFF;//acknowledge all interrupts pins
}




void SMT8L_Switch_ToHSI()
{
  CLK_SWCR_SWEN = 1;                  //  Enable switching.
  CLK_SWR = 0x01;                     //  Use HSI as the clock source.
  while (CLK_SWCR_SWBSY != 0);        //  Pause while the clock switch is busy.
}

void Initialise_STM8L_Clock()
{
  //Set Clock to full speed
  CLK_CKDIVR_CKM = 0; // Set to 0x00 => /1 ; Reset is 0x03 => /8
  //unsigned char cal = CLK_HSICALR-45;//default is 0x66 lowered by 45
  //Unlock the trimming
  /*CLK_HSIUNLCKR = 0xAC;
  CLK_HSIUNLCKR = 0x35;
  CLK_HSITRIMR = cal;
  */
  
  
  //Enable RTC Peripheral Clock
  CLK_PCKENR2_PCKEN22 = 1;
  
  CLK_CRTCR_RTCDIV = 0;//reset value : RTC Clock source /1
  CLK_CRTCR_RTCSEL = 2;// 2:LSI; reset value 0
  while (CLK_CRTCR_RTCSWBSY != 0);        //  Pause while the RTC clock changes.
    
}

void Initialise_STM8L_RTC_LowPower()
{
    //unlock the write protection for RTC
    RTC_WPR = 0xCA;
    RTC_WPR = 0x53;
    
    RTC_ISR1_INIT = 1;//Initialisation mode
    
    //RTC_SPRERH_PREDIV_S = 0;// 7bits 0x00 Sychronous prescaler factor MSB
    //RTC_SPRERL_PREDIV_S = 0;// 8bits 0xFF Sychronous prescaler factor MSB
    //RTC_APRER_PREDIV_A = 0;// 7bits 0x7F Asynchronous prescaler factor

    RTC_CR1_FMT = 0;//24h format
    RTC_CR1_RATIO = 0;//fsysclk >= 2x fRTCclk
    // N.A RTC_CR1_BYPSHAD = 0;//shadow used no direct read from counters
    
    RTC_CR2_WUTE = 0;//Wakeup timer Disable to update the timer
    while(RTC_ISR1_WUTWF==0);//
    
    RTC_CR1_WUCKSEL = 0;//-b00 RTCCCLK/16 ; -b011 RTCCCLK/2 
    
    //with 38KHz has about 61us resolution
    //225-0 with RTC_CR1_WUCKSEL = 3
    RTC_WUTRH_WUT = 255;// to match a bit less than 10s
    RTC_WUTRL_WUT = 255;//
    
    RTC_CR2_WUTE = 1;//Wakeup timer enable - starts downcounting
    RTC_CR2_WUTIE = 1;//Wakeup timer interrupt enable
    
    RTC_ISR1_INIT = 0;//Running mode

    //locking the write protection for RTC
    RTC_WPR = 0x00;
    RTC_WPR = 0x00;
    
    //wait that the internal VRef is stabilized before changing the WU options (Reference Manual)
    while(PWR_CSR2_VREFINTF == 0);
    PWR_CSR2_ULP = 1;//Internal Voltage Reference Stopped in Halt Active Halt
    PWR_CSR2_FWU = 1;//Fast wakeup time
    
    
}


void Init_Magnet_PB0()
{
    PB_DDR_bit.DDR0 = 0;//  0: Input
    PB_CR1_bit.C10 = 0; //  0: Floating
    PB_CR2_bit.C20 = 1; // Exernal interrupt enabled
    
    EXTI_CR1_P0IS = 3;//Rising and Falling edges, interrupt on events - bit 0
    //EXTI_CR3_PBIS = 00;//Falling edge and low level - Port B
    
}

void Init_Magnet_PD0()
{
    PD_DDR_bit.DDR0 = 0;//  0: Input
    PD_CR1_bit.C10 = 0; //  0: Floating
}


void Initialise_Test_GPIO_A2()
{
    PA_DDR_bit.DDR2 = 1;//  1: Output
    PA_CR1_bit.C12 = 1; //  1: Push-pull
}

void GPIO_B3_High()
{
    PB_ODR_bit.ODR3 = 1;
}

void GPIO_B3_Low()
{
    PB_ODR_bit.ODR3 = 0;
}

void PingColor()
{
      unsigned char Tx_Data[5];
      Tx_Data[0]=128;
      Tx_Data[1]=255;
      Tx_Data[2]=100;
      Tx_Data[3]=0x59;
      nRF_Transmit(Tx_Data,4);
}
void PingUart(unsigned char index)
{
      UARTPrintf("Ping Color STM8L ");
      UARTPrintf_uint(index);
      UARTPrintf(" \n");
}


BYTE iRL_count;
BYTE iRL_result;
BYTE iRL_address;
unsigned int SensorVal;

void i2c_ReadLight_StateMachine()
{
  //    sensorData[0] = ReadReg(0x03);
  //    sensorData[1] = ReadReg(0x04);

  /*UARTPrintf("rsm: ");
  UARTPrintf_uint(iRL_count);
  UARTPrintf("\n");*/

  switch(iRL_count)
  {
	case 0:                                       //set reg to 0x03
		{
			iRL_address = 0x03;
			I2C_Write(0x4A, &iRL_address,1);
		}
	break;
	case 1:                                       //read reg 0x03
		{
			I2C_Read(0x4A, &iRL_result,1); 
		}
	break;
	case 2:
		{
			sensorData[0] = iRL_result;             //result of reg[0x03]
			iRL_address = 0x04;                     //set reg to 0x04
			I2C_Write(0x4A, &iRL_address,1);
		}
	break;
	case 3:                                       //read reg 0x04
		{
			I2C_Read(0x4A, &iRL_result,1); 
		}
	break;
	case 4:                                       //read reg 0x04
		{
			sensorData[1] = iRL_result;             //result of reg[0x04]
			UARTPrintf("Light: ");
			SensorVal = sensorData[0];
			SensorVal <<= 4;//shift to make place for the 4 LSB
			SensorVal = SensorVal + (0x0F & sensorData[1]);
			UARTPrintf_uint(SensorVal);
			UARTPrintf("\n");
		}
	break;
	default:
		UARTPrintf("Unexpected default case\n");
	break;
  }
  iRL_count++;
}

void ReadLight_sm()
{
  iRL_count = 0;
  i2c_ReadLight_StateMachine();
  delay_100us();
  i2c_ReadLight_StateMachine();
  delay_100us();
  i2c_ReadLight_StateMachine();
  delay_100us();
  i2c_ReadLight_StateMachine();
  delay_100us();
  i2c_ReadLight_StateMachine();
}

void i2c_user_Rx_Callback(BYTE *userdata,BYTE size)
{
	/*UARTPrintf("I2C Transaction complete, received:\n\r");
	UARTPrintfHexTable(userdata,size);
	UARTPrintf("\n\r");*/
	//cannot call the state machine from interruption context
	//i2c_ReadLight_StateMachine();
        
}

void i2c_user_Tx_Callback(BYTE *userdata,BYTE size)
{
	/*UARTPrintf("I2C Transaction complete, Transmitted:\n\r");
	UARTPrintfHexTable(userdata,size);
	UARTPrintf("\n\r");*/
	//cannot call the state machine from interruption context
	//i2c_ReadLight_StateMachine();
}



void i2c_user_Error_Callback(BYTE l_sr2)
{
	if(l_sr2 & 0x01)
	{
		UARTPrintf("[I2C Bus Error]\n\r");
	}
	if(l_sr2 & 0x02)
	{
		UARTPrintf("[I2C Arbitration Lost]\n\r");
	}
	if(l_sr2 & 0x04)
	{
		UARTPrintf("[I2C no Acknowledge]\n\r");//this is ok for the slave
	}
	if(l_sr2 & 0x08)
	{
		UARTPrintf("[I2C Bus Overrun]\n\r");
	}
}


#pragma vector = RTC_WAKEUP_vector
__interrupt void IRQHandler_RTC(void)
{
  if(RTC_ISR2_WUTF)
  {
	delay_1ms_Count(10);//some time is needed to recover the right clock
	UARTPrintf("RTC IRQ\n\r");
    RTC_ISR2_WUTF = 0;
    
	
    RfAlive();
    
	/*UARTPrintf("Now Log Magnet\r\n");
    LogMagnets();
	
	delay_1ms_Count(1000);

	UARTPrintf("Initialise_STM8L_Clock\r\n");
	Initialise_STM8L_Clock();
	UARTPrintf("I2C Init\r\n");
	I2C_Init();

	delay_1ms_Count(1000);	
	UARTPrintf("Now Read Light\r\n");
	delay_1ms_Count(1);
    ReadLight_sm();
	delay_1ms_Count(1);*/
    
  }
  
}

int main( void )
{

    NodeId = *NODE_ID;
  //unsigned char index = 0;
    Initialise_STM8L_Clock();
    
    SYSCFG_RMPCR1_USART1TR_REMAP = 1; // Remap 01: USART1_TX on PA2 and USART1_RX on PA3
    InitialiseUART();//Tx only
    
    UARTPrintf("ws04_Node_LowSimple\\pr02_AmbientLight\n\r");
    delay_1ms_Count(1000);

    I2C_Init();
    __enable_interrupt();
    
    
    //Applies the compile time configured parameters from nRF_Configuration.h
    nRF_Config();

    //The TX Mode is independently set from nRF_Config() and can be changed on run time
    //nRF_SetMode_TX();
      
    
    //Init_Magnet_PB0();
    //Init_Magnet_PD0();
    
    Initialise_STM8L_RTC_LowPower();

    //
    // Main loop
    //
    while (1)
    {
		//RfAlive();
		UARTPrintf("main() ReadLight_sm\n\r");
		ReadLight_sm();
		UARTPrintf("RF_Light\n\r");
		Rf_Light();
		delay_1ms_Count(10);
		UARTPrintf("Back to __halt()\n\r");
		__halt();
      
    }
}
