	#include <avr/io.h>
	#include "t6963c.h"
	#include "adc.h"
	#include <util/delay.h>
	#include <avr/eeprom.h>
#define START (!(PINA&_BV(1)))
#define ZERO_WOZEK (!(PINA&_BV(4)))
#define STOP (!(PINE&_BV(4)))
#define RESET (!(PINE&_BV(3)))
#define MINUS (!(PINE&_BV(2)))
 volatile uint16_t F,ADC_Tmp,Xs,Ys,Xzakres,Yzakres;
 int sx = 0;
 int dx = 0;
 int jx = 0;
 uint16_t Xzero = 178;
 uint16_t Yzero = 280;
 uint16_t Xmax = 780;
 uint16_t Ymax = 628;
 volatile uint32_t Xx;
 volatile uint32_t Yy;
 volatile uint16_t X,Y;
EEMEM uint16_t EEP_Xzero,EEP_Yzero,EEP_Xzakres,EEP_Yzakres;
EEMEM uint8_t EEP_Kalibracja =0;
	void Czytaj_TS_NK(void)
	{
		DDRF  = 0b00001010;
		PORTF = 0b00001101;
		_delay_ms(50);
		Y = getADC(0);
		
		ADC_Tmp = getADC(2);
		Y = Y + ADC_Tmp;
		Y = Y/2;
		
		
		PORTF = 0;
		DDRF  = 0b00000101;
		PORTF = 0b00001110;
		_delay_ms(50);
		X = getADC(1);
		ADC_Tmp = getADC(3);
		X = X + ADC_Tmp;
		X = X/2;
		//	X = 1024 - X;
		PORTF = 0;

	}
	
	
void Kalibracja(void)
{
 if(eeprom_read_byte(&EEP_Kalibracja) ==( 0xFF) || (eeprom_read_byte(&EEP_Kalibracja)==0x00) || STOP)
		{
		eeprom_write_byte(&EEP_Kalibracja,1);
		GLCD_ClearText();
		GLCD_GraphicGoTo(0,0);
		GLCD_SetPixel(0,1,1);
		GLCD_SetPixel(1,1,1);
		GLCD_SetPixel(0,0,1);
		GLCD_SetPixel(1,0,1);
		
		GLCD_SetPixel(238,126,1);
		GLCD_SetPixel(238,127,1);
		GLCD_SetPixel(239,127,1);
		GLCD_SetPixel(239,126,1);
		
		
		GLCD_TextGoTo(1,1);
		GLCD_WriteString("Kalibracja ekranu dotykowego");
		GLCD_TextGoTo(1,3);
		GLCD_WriteString("Nacisnij lewy gorny rog");
		do
		{
			Czytaj_TS_NK();
		} while (X>1010 && Y>1010);
		Xs =0;
		Ys =0;
		for(F=2;F>0;F--)
		{
			Czytaj_TS_NK();
			Xs=Xs+X;
			Ys=Ys+Y;
		}
		Xzero = Xs/2;
		Yzero = Ys/2;
		GLCD_WriteString("OK!");
		
		PORTA |=1<<PA0;
		_delay_ms(500);
		PORTA &= ~1<<PA0;
		GLCD_TextGoTo(1,7);
		GLCD_WriteString("Nacisnij prawy dolny rog");
		Xs=0;
		Ys=0;
		do
		{
			Czytaj_TS_NK();
		} while (X>1010 && Y>1010);
		for(F=2;F>0;F--)
		{
			Czytaj_TS_NK();
			Xs=Xs+X;
			Ys=Ys+Y;
		}
		Xmax = Xs/2;
		Ymax = Ys/2;
		Xzakres = Xmax - Xzero;
		Yzakres = Ymax - Yzero;
		GLCD_WriteString("OK!");
		PORTA |=1<<PA0;
		_delay_ms(500);
		PORTA &= ~1<<PA0;
		eeprom_write_word(&EEP_Xzero,Xzero);
		eeprom_write_word(&EEP_Xzakres,Xzakres);
		eeprom_write_word(&EEP_Yzero,Yzero);
		eeprom_write_word(&EEP_Yzakres,Yzakres);
		}else{
			
			
			Xzero = eeprom_read_word(&EEP_Xzero);
			Yzero = eeprom_read_word(&EEP_Yzero);
			Xzakres = eeprom_read_word(&EEP_Xzakres);
			Yzakres = eeprom_read_word(&EEP_Yzakres);
		}
		GLCD_ClearGraphic();
		GLCD_ClearText();
	}
	

	void Czytaj_Po_Kalibracji(void)
	{
		Czytaj_TS_NK();
		Xx = X-Xzero;
		Xx=Xx*240;
		Xx = Xx/Xzakres;
		X=Xx;
		if (X>2000) X=0;
		if(X>240) X=240;
		Yy = Y-Yzero;
		Yy=Yy*128;
		Yy = Yy/Yzakres;
		Y=Yy;
		if (Y>2000) Y=0;
		if (Y>128) Y=128;
	}
