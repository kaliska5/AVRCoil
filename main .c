/*
 * main.c
 *
 *  Created on: 07-09-2014
 *      Author: piotr
 */

#include <util/delay.h>
#include <avr/io.h>
#include <stdlib.h>
#include "t6963c.h"
#include "touchscreen.h"
#include "graphic.h"
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "uart.h"
#include <string.h>

#define BAUD 9600
#define MYUBRR F_CPU/16/BAUD-1

#define RIGHT 1
#define LEFT 0
#define START (!(PINA&_BV(1)))
#define STOP (!(PINE&_BV(4)))
#define ZERO_WOZEK (!(PINA&_BV(4)))
#define UP_KEY (!(PINE&_BV(4)))
#define OK_KEY (!(PINE&_BV(3)))
#define DOWN_KEY (!(PINE&_BV(2)))
#define MOTOR_ON PORTG |=(1<<PG4)
#define MOTOR_CHANGE PORTG ^=(1<<PG4)
#define MOTOR_OFF PORTG &= ~(1<<PG4)
#define MOTOR_LEFT PORTG &= ~(1<<PG2)
#define MOTOR_RIGTH PORTG |=(1<<PG2)
#define HAMULEC_ON PORTA &=~(1<<PA6)
#define HAMULEC_OFF PORTA |=(1<<PA6)
#define HAMULEC_DC_ON PORTA |=(1<<PA7)
#define HAMULEC_DC_OFF PORTA &=~(1<<PA7)
#define SM_Enb_Low() PORTG|=_BV(0)
#define SM_Enb_High()  PORTG&=~_BV(0)
#define SM_Dir_Low() PORTG|=_BV(1)
#define SM_Dir_High()  PORTG&=~_BV(1)
#define SM_Clk_Low() PORTB|=_BV(7)
#define SM_Clk_High() PORTB&=~_BV(7)
#define SM_Step SM_Clk_High();_delay_us(10);SM_Clk_Low();_delay_us(10);
#define START (!(PINA&_BV(1)))
#define WaitForStart do{_delay_ms(50);}while(!START);Beep(1);
typedef uint8_t bool;
enum
{
  FALSE, TRUE
};

typedef struct
{

  char name[15];                        //nazwa programu
  int16_t data[50][6];                //tablica z programem
} Wind_Prog;

typedef struct
{
  uint8_t screw;                        //skok sruby
  char service_password[5];            //haslo serwisowe
  char programming_password[5];        //haslo programowania
  uint8_t encoder;                      //imp enkodera
  uint16_t stepper_controller;          //podzial kroku na sterowniku
  uint16_t stepper_motor;               //ilosc krokow silnika na obrot
  uint16_t shift;        //przekladnia (kolo zebate silnika)/(kolo zebate sruby)
} GlobalSetting;

volatile unsigned long pozycja_wozka = 0;
void
InitPeripherals (void);                     //inicjacja peryferii procesora
void
delayMicroseconds (uint16_t microseconds);  //funkcja opozniajaca
void
Move_Support (int16_t pozycja);             //przesun support
unsigned char
ReadProgrammingKeyboard (void);
bool
Edit_Program (Wind_Prog program_RAM);
void
Reset_Support (void);
bool
Run_Command (char command, int16_t arg1, int16_t arg2, int16_t arg3,
	     int16_t arg4, int16_t arg5);
void
Beep (uint8_t pulse);
void
Rotate_Spindle (int16_t impulsy, uint8_t predkosc);
char
Read_Prog_Key (void);
void
Wind (unsigned char kierunek_nawiniecia, unsigned char kierunek_posuwu,
      uint16_t zwoje, uint8_t drut, uint8_t PWM);

volatile long enkoder = 0;
char bufor[30];
char KeyA = ' ';
char KeyB = ' ';
int tabela[65];
volatile uint8_t tick = 0;
volatile long licznik_krokow = 0;
volatile long licznik_impulsow = 0;
volatile uint16_t STALA_KROKU = 0;
volatile uint8_t ostatnie_flagi = 0;
volatile uint8_t aktualne_flagi = 0;
volatile long ostatnia_pozycja;
volatile long imp_zadany = 0;
volatile uint16_t enkoder_temp = 0;
const unsigned char commands[] =
  { 'O', 'P', 'Z', 'z', 'N', 'C', 'R', 'K', 'W' };

const unsigned char Programs[] PROGMEM =
  {
#include "programs"
  };
const unsigned char logo[] PROGMEM =
  {
#include "logo";
  };
const unsigned char MainScreen[] PROGMEM =
  {
#include "glowny"
  };
const unsigned char Programming_bmp[] PROGMEM =
  {
#include "Prog_Bitmap"
  };

const unsigned char Letters_bmp[] PROGMEM ={
    #include "klawiatura_letters"
    };

Wind_Prog program_RAM;  //program w pamieci RAM
EEMEM Wind_Prog Program_EEP1;
EEMEM Wind_Prog Program_EEP2;
EEMEM Wind_Prog Program_EEP3;
//EEMEM Wind_Prog Program_EEP4;
//EEMEM Wind_Prog Program_EEP5;
//EEMEM Wind_Prog Program_EEP6;
uint16_t *ptr_RAM = (uint16_t *) &program_RAM;
Wind_Prog *programs[6] =
  { &Program_EEP1, &Program_EEP2, &Program_EEP3};
//, &Program_EEP4, &Program_EEP5,
 //     &Program_EEP6 };
EEMEM GlobalSetting SettingEEPROM =
  { 20, "1234", "4321", 100, 8, 200, 75 };
GlobalSetting MachineSettingRAM =
  { 20, "1234", "4321", 100, 8, 200, 75 };
uint8_t current_program = 0;
EEMEM uint8_t support_speed = 40;                //predkosc supportu
EEMEM uint16_t support_offset = 2000;              //offset suportu od czujnika
char password[5];
uint8_t flag = 1;
uint8_t i = 0;
uint8_t menu = 0;
uint8_t clear = 0;
uint8_t end = 0;
uint8_t Wind_Line = 0;
uint8_t Num_of_Repeat = 1;
uint8_t Repeat_end = 1;
uint8_t Wind_STOP = 0;
int
main (void)
{
  eeprom_read_block (&MachineSettingRAM, &SettingEEPROM,
		     sizeof(MachineSettingRAM));
  InitPeripherals ();
  GLCD_Initalize ();
  GLCD_TextGoTo (0, 0);
  GLCD_ClearText ();
  GLCD_ClearGraphic ();
  GLCD_Bitmap (logo, 0, 0, 240, 128);
  _delay_ms (1000);
  GLCD_ClearText ();
  GLCD_ClearGraphic ();
  Kalibracja ();
  OCR1B = 0;
  HAMULEC_OFF;
  while (1)
    {
 
}
bool
Run_Command (char command, int16_t arg1, int16_t arg2, int16_t arg3,
	     int16_t arg4, int16_t arg5)
{

  switch (command)
    {
    case 'O':
      Rotate_Spindle (arg1, arg2);
      break;
    case 'P':
      Move_Support (arg1);
      break;
    case 'Z':
      Reset_Support ();
      break;
    case 'z':
      Reset_Spindle ();
      break;
    case 'N':
      Wind (arg1, arg2, arg3, arg4, arg5);
      break;
    case 'K':
      end = 1;
      break;
    case 'W':
      WaitForStart
      ;
      break;
    default:
      {
	asm("nop");
      }
    }

  return 0;
}
void
Service (void)
{
  GLCD_ClearGraphic ();
  GLCD_ClearText ();

}
bool
Edit_Program (Wind_Prog program_RAM)
{
  uint8_t CurrenteditColumn = 0;
  uint8_t CurrenteditLine = 0;
  char temp = ' ';
  uint8_t Ok_Flag = 0;
  uint8_t refresh_lcd = 1;
  uint8_t exit = 0;
  char *ConvertPointer;
  uint8_t count = 0;
  char Key_char;
  long tempenkoder = 0, tempsupport = 0;
  char New_Line[23] =
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  while (!exit)
    {
      //&&&&&&&&&&&&&&&&&		ODSWIERZANIE EKRANU 	&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
      if (refresh_lcd)
	{
	  GLCD_ClearGraphic ();
	  GLCD_ClearText ();
	  GLCD_Bitmap (Programming_bmp, 0, 0, 240, 128);
	  GLCD_WriteScaledText ("Krok:", 0, 0, 2, 2);
	  GLCD_WriteScaledText (itoa (CurrenteditLine, bufor, 10), 70, 0, 2, 2);
	  if (CurrenteditLine > 0)
	    {
	      GLCD_TextGoTo (0, 2);
	      GLCD_WriteString (itoa (CurrenteditLine - 1, bufor, 10));
	      GLCD_WriteChar (':');

	      for (uint8_t k = 0; k < 6; k++)
		{
		  if (k == 0)
		    {
		      GLCD_WriteChar (program_RAM.data[CurrenteditLine - 1][k]);
		      GLCD_WriteChar (' ');
		    }
		  else
		    {
		      GLCD_WriteString (
			  itoa (program_RAM.data[CurrenteditLine - 1][k], bufor,
				10));
		      GLCD_WriteChar (' ');
		    }

		}
	    }
	  GLCD_TextGoTo (0, 3);
	  GLCD_WriteString (itoa (CurrenteditLine, bufor, 10));
	  GLCD_WriteChar (':');
	  for (uint8_t k = 0; k < 6; k++)
	    {
	      if (k == 0)
		{
		  GLCD_WriteChar (program_RAM.data[CurrenteditLine][k]);
		  GLCD_WriteChar (' ');
		}
	      else
		{
		  GLCD_WriteString (
		      itoa (program_RAM.data[CurrenteditLine][k], bufor, 10));
		  GLCD_WriteChar (' ');
		}

	    }
	  GLCD_TextGoTo (0, 4);
	  if (CurrenteditLine < 49)
	    {
	      GLCD_WriteString (itoa (CurrenteditLine + 1, bufor, 10));
	      GLCD_WriteChar (':');
	      for (uint8_t k = 0; k < 6; k++)
		{
		  if (k == 0)
		    {
		      GLCD_WriteChar (program_RAM.data[CurrenteditLine + 1][k]);
		      GLCD_WriteChar (' ');
		    }
		  else
		    {
		      GLCD_WriteString (
			  itoa (program_RAM.data[CurrenteditLine + 1][k], bufor,
				10));
		      GLCD_WriteChar (' ');
		    }

		}
	    }

	}
      //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&7
      Czytaj_Po_Kalibracji ();

      if (enkoder != tempenkoder || pozycja_wozka != tempsupport || refresh_lcd)
	{
	  tempenkoder = enkoder;
	  tempsupport = pozycja_wozka;

	  GLCD_TextGoTo (0, 7);
	  GLCD_WriteString ("                        ");
	  GLCD_TextGoTo (0, 7);
	  GLCD_WriteString ("Enkoder:");
	  GLCD_WriteString (ltoa (tempenkoder / 2, bufor, 10));
	  GLCD_TextGoTo (0, 8);
	  GLCD_WriteString ("                        ");
	  GLCD_TextGoTo (0, 8);
	  GLCD_WriteString ("Support:");
	  GLCD_WriteString (ltoa (tempsupport / 12, bufor, 10));
	  refresh_lcd = 0;
	}

      if (Y == 128 && X == 240)
	{
	  temp = 0;
	}
      if ((Y > 0 && Y < 23) && (X > 220 && X < 240))
	{		//Wyjscie
	  exit = 1;
	}
      if ((Y > 108 && Y < 127) && (X > 173 && X < 197))
	{		//	OK
	  Ok_Flag = 1;
	  Beep (1);
	  _delay_ms (500);

	  for (uint8_t k = 0; k < sizeof(New_Line); k++)
	    {
	      New_Line[k] = ' ';
	    }
	  New_Line[21] = 0;
	  count = 0;
	  while (Ok_Flag)
	    {		//	Edycja danych
	      Key_char = Read_Prog_Key ();

	      GLCD_Rectangle (0, 58, 200, 30);
	      GLCD_TextGoTo (0, 9);
	      GLCD_WriteString (New_Line);
	      GLCD_TextGoTo (count, 9);
	      GLCD_WriteChar ('_');
	      if ((Key_char == '>') || (Key_char == '<') || Key_char == 0)
		{
		  if (Key_char == '>')
		    {
		      Beep (1);
		      count++;
		    }
		  if (Key_char == '<')
		    {
		      Beep (1);
		      count--;
		    }
		  if (Key_char == 0)
		    {

		    }

		}
	      else
		{
		  if (Key_char == '!' || Key_char == '#')
		    {
		      if (Key_char == '!')
			{
			  New_Line[count] = ' ';
			  count++;
			  Beep (1);
			  refresh_lcd = 1;
			}
		      if (Key_char == '#')
			{
			  //Zapisanie danych w lini
			  Ok_Flag = 0;
			  CurrenteditColumn = 0;
			  ConvertPointer = strtok (New_Line, " ");
			  program_RAM.data[CurrenteditLine][CurrenteditColumn] =
			      *ConvertPointer;
			  CurrenteditColumn++;
			  do
			    {
			      ConvertPointer = strtok (NULL, " ");
			      if (CurrenteditColumn < 6)
				{
				  program_RAM.data[CurrenteditLine][CurrenteditColumn] =
				      atoi (ConvertPointer);
				}

			      CurrenteditColumn++;

			    }
			  while (ConvertPointer);
			  eeprom_update_block (&program_RAM,
					       programs[current_program],
					       sizeof(program_RAM));

			  Beep (2);
			  refresh_lcd = 1;
			}
		    }
		  else
		    {
		      New_Line[count] = Key_char;
		      count++;
		      refresh_lcd = 1;
		      Beep (1);
		    }
		}

	    }
	}
      ////////// literki !!!!!

      //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
      if ((Y > 104 && Y < 128) && (X > 216 && X < 240))
	{	//	>
	  temp = '>';
	  refresh_lcd = 1;
	  CurrenteditLine++;
	}
      if ((Y > 104 && Y < 128) && (X > 1 && X < 23))
	{	//	>

	  temp = '<';
	  refresh_lcd = 1;
	  CurrenteditLine--;
	}
      if (Key_char == 'P')
	{
	  exit = 1;
	}
      if (START)
	{

	  Run_Command (program_RAM.data[CurrenteditLine][0],
		       program_RAM.data[CurrenteditLine][1],
		       program_RAM.data[CurrenteditLine][2],
		       program_RAM.data[CurrenteditLine][3],
		       program_RAM.data[CurrenteditLine][4],
		       program_RAM.data[CurrenteditLine][5]);
	}
    }

  return 1;
}
char
Read_Prog_Key (void)
{
  char temp = 0;
  Czytaj_Po_Kalibracji ();
  if (Y == 128 && X == 240)
    {
      temp = 0;
    }
  if ((Y > 104 && Y < 128) && (X > 216 && X < 240))
    {
      temp = '>';
    }
  if ((Y > 104 && Y < 128) && (X > 1 && X < 23))
    {
      temp = '<';
    }
  if ((Y > 0 && Y < 23) && (X > 203 && X < 220))
    {	//	O
      temp = 'O';

    }
  if ((Y > 0 && Y < 23) && (X > 220 && X < 240))
    {	//	P
      temp = 'P';

    }
  if ((Y > 23 && Y < 47) && (X > 203 && X < 220))
    {	// 	Z
      temp = 'Z';

    }
  if ((Y > 23 && Y < 47) && (X > 220 && X < 240))
    {	// 	z
      temp = 'z';
    }

  if ((Y > 47 && Y < 71) && (X > 203 && X < 220))
    {	//	W
      temp = 'W';

    }
  if ((Y > 47 && Y < 72) && (X > 220 && X < 240))
    {	//	R
      temp = 'R';

    }
  if ((Y > 71 && Y < 95) && (X > 203 && X < 220))
    {	//	N
      temp = 'N';

    }
  if ((Y > 71 && Y < 95) && (X > 220 && X < 240))
    {	//	K
      temp = 'K';
    }
  if ((Y > 108 && Y < 127) && (X > 173 && X < 197))
    {
      temp = '#';
    }
  if ((Y > 108 && Y < 128) && (X > 101 && X < 125))
    {	//	-
      temp = '-';

    }
  if ((Y > 108 && Y < 128) && (X > 125 && X < 149))
    {	//	+
      temp = '+';
    }

  if ((Y > 108 && Y < 128) && (X > 149 && X < 173))
    {	//	Space
      temp = '!';
    }

  if ((Y > 88 && Y < 108) && (X > 30 && X < 53))
    {	//	0
      temp = '0';
    }
  if ((Y > 88 && Y < 108) && (X > 53 && X < 77))
    {	//	1
      temp = '1';
    }
  if ((Y > 88 && Y < 108) && (X > 77 && X < 101))
    {	//	2
      temp = '2';
    }
  if ((Y > 88 && Y < 108) && (X > 102 && X < 125))
    {	//	3
      temp = '3';
    }
  if ((Y > 88 && Y < 108) && (X > 125 && X < 149))
    {	//	4
      temp = '4';
    }
  if ((Y > 88 && Y < 108) && (X > 150 && X < 174))
    {	//	5
      temp = '5';
    }
  if ((Y > 88 && Y < 108) && (X > 174 && X < 197))
    {	//	6
      temp = '6';
    }
  if ((Y > 108 && Y < 127) && (X > 30 && X < 53))
    {	//	7
      temp = '7';
    }
  if ((Y > 108 && Y < 127) && (X > 53 && X < 78))
    {	//	8
      temp = '8';
    }
  if ((Y > 108 && Y < 127) && (X > 78 && X < 101))
    {	//	9
      temp = '9';
    }

  return temp;
}

void
Beep (uint8_t pulse)
{
  do
    {
      PORTA |= 1 << PA0;
      _delay_ms (100);
      PORTA &= ~1 << PA0;
      _delay_ms (100);
      pulse--;
    }
  while (pulse);

}

unsigned char
ReadLetterKeyboard (void)
{
  if ((Y > 80 && Y < 104) && (X > 0 && X < 39))
    {
      return '1';
    }

  return '?';
}

unsigned char
ReadProgrammingKeyboard (void)
{

  Czytaj_Po_Kalibracji ();

//                      klawisze funkcyjne
  if ((Y > 80 && Y < 104) && (X > 0 && X < 39))
    {
      return '1';
    }
  if ((Y > 80 && Y < 104) && (X > 39 && X < 79))
    {
      return '2';
    }
  if ((Y > 80 && Y < 104) && (X > 80 && X < 119))
    {
      return '3';
    }
  if ((Y > 80 && Y < 104) && (X > 120 && X < 159))
    {
      return '4';
    }
  if ((Y > 80 && Y < 104) && (X > 159 && X < 199))
    {
      return '5';
    }
  if ((Y > 105 && Y < 127) && (X > 0 && X < 40))
    {
      return '6';
    }
  if ((Y > 105 && Y < 127) && (X > 39 && X < 79))
    {
      return '7';
    }
  if ((Y > 105 && Y < 127) && (X > 79 && X < 119))
    {
      return '8';
    }
  if ((Y > 105 && Y < 127) && (X > 119 && X < 159))
    {
      return '9';
    }
  if ((Y > 105 && Y < 127) && (X > 159 && X < 199))
    {
      return '0';
    }
  if ((Y > 61 && Y < 80) && (X > 181 && X < 240))
    {
      return 'E';
    }

//              Klawisze gora dol boki i ok
  if ((Y > 80 && Y < 92) && (X > 209 && X < 229))
    {
      return '^';
    }
  if ((Y > 114 && Y < 127) && (X > 209 && X < 229))
    {
      return 'v';
    }
  if ((Y > 114 && Y < 127) && (X > 209 && X < 229))
    {
      return 'v';
    }
  if ((Y > 92 && Y < 114) && (X > 229 && X < 240))
    {
      return '>';
    }
  if ((Y > 92 && Y < 114) && (X > 199 && X < 210))
    {
      return '<';
    }
  if ((Y > 92 && Y < 114) && (X > 209 && X < 229))
    {
      return '#';
    }

  if ((Y > 33 && Y < 55) && (X > 199 && X < 240))
    {
      return 'e';
    }
  if ((Y > 0 && Y < 17) && (X > 0 && X < 17))
    {
      return '@';
    }
  return '?';
}

void
InitPeripherals (void)
{

  DDRD = 0;
  PORTD = 0;
  DDRC = 255;
  PORTB = 0;
  PORTC = 0;
  DDRA = 0b11100001;
  DDRE = 0;
  PORTE |= 0b00111000;
  PORTA = 0b00001110;
  DDRB = 0xFF;
  DDRG = 0b11111111;
  PORTG = 0;
  EICRB = (1 << ISC60) | (1 << ISC70);
  EIMSK = (1 << INT6) | (1 << INT7);
  OCR1B = 0;
  TCCR1A = ((1 << COM1B1) | (1 << WGM11) | (1 << WGM10));
  TCCR1B = (1 << CS10) | (1 << WGM12);
  sei();
}
void
Reset_Support (void)
{

  uint16_t temp_offset = 0;
  //eeprom_write_word(&WOZEK_OFFSET,100);
  temp_offset = eeprom_read_word (&support_offset);
  uint8_t loop = 10;
  pozycja_wozka = 0;
  if (ZERO_WOZEK)
    {
      Move_Support (3000);
    }

  SM_Dir_Low();
  SM_Enb_High();
  do
    {

      SM_Step
      ;
      delayMicroseconds (100);
      //delayMicroseconds(400);

    }
  while (!ZERO_WOZEK);
  pozycja_wozka = 0;
  _delay_ms (500);
  GLCD_ClearText ();
  GLCD_TextGoTo (0, 7);
  GLCD_WriteString ("OFFSET=");
  GLCD_WriteString (itoa (temp_offset, bufor, 10));

  SM_Enb_High();
  SM_Dir_High();
  for (uint16_t a = 0; a < temp_offset; a++)
    {
      SM_Step
      ;
      _delay_us (400);

    }
  _delay_ms (1000);
  pozycja_wozka = 0;
  if (START)
    {
      PORTA |= 1 << PA0;
      _delay_ms (1000);
      PORTA &= ~1 << PA0;
      do
	{
	  SM_Enb_High();
	  GLCD_TextGoTo (0, 8);
	  GLCD_WriteString ("Nowy OFFSET:");
	  GLCD_WriteString (itoa (temp_offset, bufor, 10));
	  if (UP_KEY)
	    {
	      temp_offset += 10;
	      SM_Dir_High();
	      loop = 10;
	      while (loop)
		{
		  SM_Step
		  ;
		  _delay_ms (2);
		  loop--;
		}
	    }
	  if (DOWN_KEY)
	    {
	      if (temp_offset)
		{
		  temp_offset -= 10;
		  SM_Dir_Low();
		  loop = 10;
		  while (loop)
		    {
		      SM_Step
		      ;
		      _delay_ms (2);
		      loop--;
		    }
		}
	    }
	  _delay_ms (20);
	}
      while (!START);
      eeprom_write_word (&support_offset, temp_offset);
      uint8_t temp_max = eeprom_read_byte (&support_speed);
      PORTA |= 1 << PA0;
      _delay_ms (1000);
      PORTA &= ~1 << PA0;
      GLCD_ClearText ();
      do
	{
	  GLCD_TextGoTo (0, 8);
	  GLCD_WriteString ("USMAX_OFFSET:");
	  GLCD_WriteString (itoa (temp_max, bufor, 10));

	  if (UP_KEY)
	    {
	      temp_max++;
	      _delay_ms (200);

	    }
	  if (DOWN_KEY)
	    {
	      temp_max--;
	      _delay_ms (200);
	    }
	  _delay_ms (20);
	}
      while (!START);
      eeprom_update_byte (&support_speed, temp_max);
      pozycja_wozka = 0;

    }

  EIMSK = (1 << INT6) | (1 << INT7) | (1 << INT4);
  EICRB = (1 << ISC60) | (1 << ISC70) | (1 << ISC41);
  GLCD_ClearText ();

}

void
delayMicroseconds (uint16_t microseconds)
{
  for (uint16_t a = 0; a < microseconds; a++)
    {
      _delay_us (1);

    }

}

void
Move_Support (int16_t pozycja)
{
  /*
   steps - liczba krokow do wykonania
   NN    - parametr podzia³u, od 3 do 8, 8 - lagodnie, 3 ostro
   usmax - przerwa us przy maksymalnej prêdkoci
   usmin - przerwa us przy najmniejszej prêdkoci
   obliczenie ilości kroków
   skok śruby 5mm, ilość kroków na obrót SM - 200, z 1/16 kroku to 3200, przełożenie 24:18 czyli
   18/24*3200--jeden obrót śruby to 2400 kroków a to przesuniêcie o 5mm
   1mm - 480 kroków
   */
  int32_t poziomo = 0;                     // licznik kroków na pe³nej prêdkoci
  int32_t licznik = 0;                     // licznik elementów tabeli przerw
  int32_t NN = 20;
  int32_t usmax = eeprom_read_byte (&support_speed);
  int32_t usmin = 1000;
  int32_t przerwa = usmin;               // przerwa pocz¹tkowa
  //float shift_local = (float)(MachineSettingRAM.shift) / 100;
  int32_t steps = 0;
  uint8_t kierunek = 0;
  SM_Enb_High();
  //int32_t steps =((shift_local * (MachineSettingRAM.stepper_controller * MachineSettingRAM.stepper_motor)) / MachineSettingRAM.screw) * abs(pozycja);
  pozycja = pozycja * 12;

  if (pozycja > pozycja_wozka)
    {
      steps = pozycja - pozycja_wozka;
      kierunek = 1;
      SM_Dir_High();
    }
  else
    {
      steps = pozycja_wozka - pozycja;
      kierunek = 0;
      SM_Dir_Low();
    }
  if (pozycja != pozycja_wozka)
    {
      //  start rampy
      do
	{
	  licznik = licznik + 1;
	  przerwa = przerwa - (int) ((przerwa - usmax) / NN) - 1;
	  if (przerwa < usmax)
	    {
	      przerwa = usmax;
	    };
	  tabela[licznik] = przerwa;
	  SM_Step
	  ;

	  delayMicroseconds (przerwa);
	  if (kierunek == 1)
	    {
	      pozycja_wozka++;
	    }
	  else
	    {
	      pozycja_wozka--;
	    }

	}
      while (przerwa > usmax);
      // jazda na pe³nej prêdkoci
      poziomo = steps - 2 * licznik;
      przerwa = usmax;
      for (int FF = 1; FF < poziomo; FF++)
	{
	  SM_Step
	  ;
	  delayMicroseconds (przerwa);
	  if (kierunek == 1)
	    {
	      pozycja_wozka++;
	    }
	  else
	    {
	      pozycja_wozka--;
	    }

	}
      // pêtla hamowania
      for (int KK = licznik; KK > 1; KK--)
	{
	  przerwa = tabela[KK];
	  SM_Step
	  ;
	  delayMicroseconds (przerwa);
	  if (kierunek == 1)
	    {
	      pozycja_wozka++;
	    }
	  else
	    {
	      pozycja_wozka--;
	    }
	}
    }
  //SM_Enb_Low();

}

void
Rotate_Spindle (int16_t impulsy, uint8_t predkosc)
{
  ostatnia_pozycja = enkoder;
  imp_zadany = (long) impulsy * 2;

  if (imp_zadany > ostatnia_pozycja)
    {
      MOTOR_RIGTH;

    }
  if (imp_zadany < ostatnia_pozycja)
    {
      MOTOR_LEFT;

    }
  OCR1B = predkosc * 4;
  HAMULEC_OFF;
  licznik_impulsow = 0;

  _delay_ms (20);
  MOTOR_ON;
  do
    {

    }
  while (imp_zadany != enkoder);
  GLCD_TextGoTo (0, 6);
  GLCD_WriteString ("                        ");
  GLCD_TextGoTo (0, 6);
  GLCD_WriteString ("Imp_zad:");
  GLCD_WriteString (ltoa (imp_zadany, bufor, 10));
  GLCD_TextGoTo (0, 7);
  GLCD_WriteString ("                        ");
  GLCD_TextGoTo (0, 6);
  GLCD_WriteString ("enkoder:");
  GLCD_WriteString (ltoa (enkoder, bufor, 10));
  HAMULEC_ON;
  MOTOR_OFF;

}

void
Porownaj_enkoder (void)
{
  tick = 1;
  enkoder_temp++;    		//Odczyt wskazan enkodera dziala nie ruszac!!!!
  licznik_impulsow++;
  licznik_krokow += STALA_KROKU;
  uint8_t temp = PINE & (0b11000000);			//aktualny stan enkodera
  switch (ostatnie_flagi)
    {
    case 0b00000000:
      {

	if (temp == 0b01000000)
	  enkoder++;
	if (temp == 0b10000000)
	  enkoder--;
	ostatnie_flagi = temp;
	break;
      }
    case 0b01000000:
      {
	if (temp == 0b11000000)
	  enkoder++;
	if (temp == 0)
	  enkoder--;
	ostatnie_flagi = temp;
	break;
      }
    case 0b10000000:
      {
	if (temp == 0b11000000)
	  enkoder--;
	if (temp == 0)
	  enkoder++;
	ostatnie_flagi = temp;
	break;
      }
    case 0b11000000:
      {
	if (temp == 0b01000000)
	  enkoder--;
	if (temp == 0b10000000)
	  enkoder++;
	ostatnie_flagi = temp;
	break;
      }

    }
}
ISR(INT6_vect)
{
  //kanal A enkodera
  Porownaj_enkoder ();
}

ISR(INT7_vect)
{
  Porownaj_enkoder ();
}

ISR(INT4_vect)
{
  EIMSK &= ~(1 << INT4);
  uint8_t kopia = PORTG;
  Beep (1);

  while (1)
    {
      HAMULEC_ON;
      MOTOR_OFF;
      _delay_ms (300);
      if (DOWN_KEY)
	{
	  Wind_STOP = 1;
	  Beep (3);
	}
      if (STOP)
	{
	  EIFR |= (1 << INTF4);
	  Beep (1);
	  Beep (1);
	  PORTG = kopia;
	  HAMULEC_OFF;
	  EIMSK |= (1 << INT4);

	  break;
	}
    }

}
void
Wind (unsigned char kierunek_nawiniecia, unsigned char kierunek_posuwu,
      uint16_t zwoje, uint8_t drut, uint8_t PWM)
{
//#######################PRACA KROKOWA##############################################
  /*
   uint8_t screw;
   uint16_t service_password;
   uint8_t encoder;
   uint16_t stepper_controller;
   uint16_t stepper_motor;
   float shift;
   */
  OCR1B = PWM * 4;
  uint16_t impulsy = zwoje * 40;
  uint16_t deceleration = impulsy / 4;
  uint16_t step = deceleration / 15;
  uint16_t PWM_step = ((PWM * 4)) / 16;
  //   zmiana dla drutu 2x2mm dla disqusa srednica*2 !!!
  float drut_n = (float) (drut * 2) / 100;
  // zmiana dla nawijarki diskus ^
  float shift = 0.75;
  float temp =
      (((shift
	  * (MachineSettingRAM.stepper_controller
	      * MachineSettingRAM.stepper_motor)) / MachineSettingRAM.screw)
	  * 1000) / MachineSettingRAM.encoder;
  STALA_KROKU = drut_n * temp / 2;

//##################################################################################
  if (kierunek_posuwu == LEFT)
    SM_Dir_Low();
  if (kierunek_posuwu == RIGHT)
    SM_Dir_High();
  if (kierunek_nawiniecia == LEFT)
    MOTOR_LEFT;
  if (kierunek_nawiniecia == RIGHT)
    MOTOR_RIGTH;

  //STALA_KROKU = GlobalSetting.
  licznik_impulsow = 0;
  licznik_krokow = 0;
  SM_Enb_High();
  HAMULEC_OFF;
  MOTOR_ON;
  do
    {
      if (licznik_krokow >= 1000)
	{
	  SM_Step
	  ;
	  licznik_krokow = licznik_krokow - 1000;
	  if (kierunek_posuwu == RIGHT)
	    {
	      pozycja_wozka++;
	    }
	  else
	    {
	      pozycja_wozka--;
	    }

	}

      if (licznik_impulsow == impulsy - deceleration)
	{
	  deceleration -= step;

	  if (OCR1B > 40)
	    {
	      OCR1B -= PWM_step;
	    }

	}

    }
  while (licznik_impulsow != impulsy - 10);
  GLCD_TextGoTo (0, 7);
  GLCD_WriteString ("                        ");
  GLCD_TextGoTo (0, 6);
  GLCD_WriteString ("enkoder:");
  GLCD_WriteString (ltoa (enkoder, bufor, 10));
  MOTOR_OFF;
  HAMULEC_ON;

}

void
Reset_Spindle (void)
{
  if ((PIND & _BV(7)))
    {
      OCR1B = 0;
      HAMULEC_OFF;
      MOTOR_RIGTH;
      OCR1B = 30;
      MOTOR_ON;
      _delay_ms (700);

    }
  do
    {
      MOTOR_ON;
      HAMULEC_OFF;
      MOTOR_RIGTH;
      OCR1B = 25;
      MOTOR_ON;
    }
  while (!(PIND & _BV(7)));
  HAMULEC_ON;
  MOTOR_OFF;
  _delay_ms (1000);
  enkoder = 0;
  ostatnie_flagi = PINE & (0b11000000);

}
