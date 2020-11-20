/* Tiny TFT Graphics Library - see http://www.technoblogy.com/show?L6I

   David Johnson-Davies - www.technoblogy.com - 13th June 2019
   ATtiny85 @ 8 MHz (internal oscillator; BOD disabled)
   
   CC BY 4.0
   Licensed under a Creative Commons Attribution 4.0 International license: 
   http://creativecommons.org/licenses/by/4.0/
*/

#define F_CPU 8000000UL                 // set the CPU clock
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/pgmspace.h>
#include <math.h>
#include <string.h>
#include "st7735.h"

#define BACKLIGHT_ON PORTB |= (1<<PB2)
#define BACKLIGHT_OFF PORTB &= ~(1<<PB2)						

#define LED_OFF PORTC &= ~(1<<PC3)
#define LED_ON PORTC |= (1<<PC3)

//Buttons 
#define T1 	!(PIND & (1<<PD6))
#define T2	!(PIND & (1<<PD2))
#define T3	!(PIND & (1<<PD5))

/* some RGB color definitions                                                 */
#define BLACK        0x0000      
#define RED          0x001F      
#define GREEN        0x07E0      
#define YELLOW       0x07FF      
#define BLUE         0xF800      
#define CYAN         0xFFE0      
#define White        0xFFFF     
#define BLUE_LIGHT   0xFD20      
#define TUERKISE     0xAFE5      
#define VIOLET       0xF81F		
#define WHITE		0xFFFF

#define SEK_POS 10,110

#define RELOAD_ENTPRELL 1 

// Pins already defined in st7735.c
extern int const DC;
extern int const MOSI;
extern int const SCK;
extern int const CS;
// Text scale and plot colours defined in st7735.c
extern int fore; 		// foreground colour
extern int back;      	// background colour
extern int scale;     	// Text size


volatile uint8_t ms10,ms100,sec,min, entprell;


char stringbuffer[20]; // buffer to store string 

const uint8_t size = 128;

ISR (TIMER1_COMPA_vect);

volatile uint8_t ISR_zaehler = 0;
volatile uint8_t ms100 = 0;
volatile uint32_t Time = 0;
volatile uint8_t IsPaused = 1;
ISR (TIMER0_OVF_vect)
{
	if (IsPaused == 0)
	{
		TCNT0 = 0;
		ISR_zaehler++;
		if(ISR_zaehler == 12)
		{
			ms100++;
			ISR_zaehler = 0;
			if (ms100 == 10)
			{
				Time++;
				ms100 = 0;
			}
		}
	}
}//End of ISR

void SPI_MasterTransmit(uint8_t cData)
{/* Start transmission */
	SPDR = cData;
	/* Wait for transmission complete */
	while(!(SPSR & (1<<SPIF)))
	;
}

struct Task
{
	uint8_t time[3];
	uint8_t Colour[3];
	uint32_t Seconds;
	char Name[20];
};

int main(void)
{
	DDRB |= (1<<DC) | (1<<CS) | (1<<MOSI) |( 1<<SCK); 	// All outputs
	PORTB = (1<<SCK) | (1<<CS) | (1<<DC);          		// clk, dc, and cs high
	DDRB |= (1<<PB2);									//lcd Backlight output
	PORTB |= (1<<CS) | (1<<PB2);                  		// cs high
	DDRC |= (1<<PC3);									//Reset Output
	DDRD |= (1<<PD7);									//Reset Output
	PORTD |= (1<<PD7);	
									//Reset High
	DDRD &= ~((1<<PD6) | (1<<PD2) | (1<<PD5)); 	//Taster 1-3
	PORTD |= ((1<<PD6) | (1<<PD2) | (1<<PD5)); 	//PUllups für Taster einschalten
	
		//Timer 1 Configuration
	OCR1A = 1249;	//OCR1A = 0x3D08;==1sec
	
	//Init SPI		CLK/2
	//==================================================================
	SPCR |= (1<<SPE) | (1<<MSTR);
	SPSR |= (1<<SPI2X);
	//==================================================================
		
    TCCR1B |= (1 << WGM12);
    // Mode 4, CTC on OCR1A

    TIMSK1 |= (1 << OCIE1A);
    //Set interrupt on compare match

    TCCR1B |= (1 << CS11) | (1 << CS10);
    // set prescaler to 64 and start the timer

    sei();
    // enable interrupts
    
    ms10=0;
    ms100=0;
    sec=0;
    min=0;
    entprell=0;
	
	BACKLIGHT_ON;
	LED_ON;

	setup();
	
	//Konfiguration Timer Overflow
	//==================================================================
	TCCR0A	= 0x00;
	TCCR0B	= 0x04;
	TIMSK0	|= (1 << TOIE0);
	TIFR0 |= (1 << TOV0);
	sei();
	//==================================================================
	
	uint16_t count1;	//Variable used for Counting in for loops
	uint16_t count2;	//Variable used for Counting in for loops
	
	char buffer[20];
	
	uint8_t isPushed = 0;
	
	struct Task AufgabenLesen;
	strcpy(AufgabenLesen.Name,"AufgabenLesen");
	AufgabenLesen.time[0] = 0;
	AufgabenLesen.time[1] = 15;
	AufgabenLesen.time[2] = 0;
	AufgabenLesen.Colour[0] = 0;
	AufgabenLesen.Colour[1] = 255;
	AufgabenLesen.Colour[2] = 0;
	
	struct Task Messprotokoll;
	strcpy(Messprotokoll.Name,"Messprotokoll");
	Messprotokoll.time[0] = 0;
	Messprotokoll.time[1] = 30;
	Messprotokoll.time[2] = 0;
	Messprotokoll.Colour[0] = 255;
	Messprotokoll.Colour[1] = 255;
	Messprotokoll.Colour[2] = 0;
	
	struct Task Struktogram;
	strcpy(Struktogram.Name,"Struktogram");
	Struktogram.time[0] = 0;
	Struktogram.time[1] = 45;
	Struktogram.time[2] = 0;
	Struktogram.Colour[0] = 0;
	Struktogram.Colour[1] = 255;
	Struktogram.Colour[2] = 255;
	
	struct Task Codieren;
	strcpy(Codieren.Name,"Codieren");
	Codieren.time[0] = 0;
	Codieren.time[1] = 30;
	Codieren.time[2] = 0;
	Codieren.Colour[0] = 0;
	Codieren.Colour[1] = 0;
	Codieren.Colour[2] = 255;
	
	struct Task Testen;
	strcpy(Testen.Name,"Testen");
	Testen.time[0] = 0;
	Testen.time[1] = 30;
	Testen.time[2] = 0;
	Testen.Colour[0] = 255;
	Testen.Colour[1] = 0;
	Testen.Colour[2] = 0;
	
	struct Task Vorfueren;
	strcpy(Vorfueren.Name,"Vorfueren");
	Vorfueren.time[0] = 0;
	Vorfueren.time[1] = 30;
	Vorfueren.time[2] = 0;
	Vorfueren.Colour[0] = 255;
	Vorfueren.Colour[1] = 255;
	Vorfueren.Colour[2] = 255;
	
	const uint8_t BarWidth = 10;
	const uint8_t BarHeight = 100;
	
	const uint8_t TaskCount = 6;
	
	uint8_t PosY = 0;
	
	uint32_t TotalTime = 0;
	uint32_t LastTime = 1;
	uint32_t target = 0;
	
	struct Task TaskList[] = {AufgabenLesen, Messprotokoll, Struktogram, Codieren, Testen, Vorfueren};
	
	for(count1 = 0; count1 < 40; count1++)
	{
		uint8_t temp = 255 * sin(((count1 * 4.5))*(M_PI/180));
		fore = Colour(0, temp, temp);
		glcd_draw_circle(size/1.7+1, size/2, count1);
		glcd_draw_circle(size/1.7, size/2+1, count1);
		glcd_draw_circle(size/1.7, size/2-1, count1);
		glcd_draw_circle(size/1.7-1, size/2, count1);
		glcd_draw_circle(size/1.7, size/2, count1);
	}
	
	TotalTime = 0;
	
	for (count1 = 0; count1 < TaskCount; count1++)
	{
		TaskList[count1].Seconds = (((TaskList[count1].time[0] * 60) + TaskList[count1].time[1]) * 60) + TaskList[count1].time[2];
		TotalTime += TaskList[count1].Seconds;
	}
	
	IsPaused = 1;
	Time = 0;
	for (count1 = 0; count1 < TaskCount; count1++)
	{
		Time += TaskList[count1].Seconds;
		PosY = (((size - BarHeight)) + (BarHeight - (BarHeight * ((float)Time / TotalTime))));
		MoveTo(0,PosY);
		fore = Colour(TaskList[count1].Colour[0], TaskList[count1].Colour[1], TaskList[count1].Colour[2]);
		FillRect(BarWidth, round(BarHeight * ((float)TaskList[count1].Seconds / TotalTime)));
	}
	Time = 0;
	
	count1 = -1;
	target = 0;
	
	while (1)
	{
		if(Time != LastTime)
		{
			LastTime = Time;
			fore = BLACK;
			PosY = (((size - BarHeight)) + (BarHeight - (BarHeight * ((float)Time / TotalTime))));
			MoveTo(0,PosY);
			FillRect(BarWidth, 10);
			for (count2 = 360; count2 > 360 * (1-(((float)Time - (target - TaskList[count1].Seconds)) / TaskList[count1].Seconds)); count2 -= 3)
			{
				MoveTo(40 * cos((count2 + 180) * (M_PI / 180)) + size/1.7, 40 * sin(count2 * (M_PI / 180)) + size/2);
				DrawTo(50 * cos((count2 + 180) * (M_PI / 180)) + size/1.7, 45 * sin(count2 * (M_PI / 180)) + size/2);
			}
			fore = WHITE;
			MoveTo(0, 8);
			sprintf(buffer, "%02d:%02d:%02d", (uint8_t)(target - Time)/3600, (uint8_t)((target - Time)/60)%60, (uint8_t)(target - Time)%60);
			PlotString(buffer);
			if (Time == target)
			{
				if(Time != TotalTime)
				{
					count1++;
					target += TaskList[count1].Seconds;
					fore = Colour(TaskList[count1].Colour[0], TaskList[count1].Colour[1], TaskList[count1].Colour[2]);
					MoveTo(0,0);
					PlotString("                             ");
					MoveTo(0,0);
					PlotString(TaskList[count1].Name);
					for (count2 = 0; count2 < 360; count2 += 3)
					{
						MoveTo(40 * cos((count2 + 180) * (M_PI / 180)) + size/1.7, 40 * sin(count2 * (M_PI / 180)) + size/2);
						DrawTo(50 * cos((count2 + 180) * (M_PI / 180)) + size/1.7, 45 * sin(count2 * (M_PI / 180)) + size/2);
					}
					IsPaused = 0;
				}
				else
				{
					fore = RED;
					for(count1 = 0; count1 < size; count1++)
					{
						for(count2 = 0; count2 <= count1; count2++)
						{
							PlotPoint(count2 + count1, count1 - count2);
							PlotPoint(count2 + count1 - 1, count1 - count2);
							PlotPoint(count1 - count2, count2 + count1);
							PlotPoint(count1 - count2, count2 + count1 - 1);
						}
					}
					return 0;
				}
			}
		}
	}
	  
	for (;;)
	{

	}//end of for()
}//end of main

ISR (TIMER1_COMPA_vect)
{
	
}
