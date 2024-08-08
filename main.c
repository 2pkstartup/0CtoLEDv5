#define F_CPU 16000000UL
//WS2812_CONFIG
#define ws2812_resettime  300 
#define ws2812_port C     // Data port 
#define ws2812_pin  0     // Data out pin 
#define CONCAT(a, b)            a ## b
#define CONCAT_EXP(a, b)   CONCAT(a, b)
#define ws2812_PORTREG  CONCAT_EXP(PORT,ws2812_port)
#define ws2812_DDRREG   CONCAT_EXP(DDR,ws2812_port)
#define interrupt_is_disabled
// Timing in ns
#define w_zeropulse   350
#define w_onepulse    900
#define w_totalperiod 1250

// Fixed cycles used by the inner loop
#if defined(__LGT8F__)     // LGT8F88A
#define w_fixedlow    4
#define w_fixedhigh   6
#define w_fixedtotal  10   
#elif __AVR_ARCH__ == 100  // reduced core AVR
#define w_fixedlow    2
#define w_fixedhigh   4
#define w_fixedtotal  8   
#else                      // all others
#define w_fixedlow    3
#define w_fixedhigh   6
#define w_fixedtotal  10   
#endif


// Insert NOPs to match the timing, if possible
#define w_zerocycles    (((F_CPU/1000)*w_zeropulse          )/1000000)
#define w_onecycles     (((F_CPU/1000)*w_onepulse    +500000)/1000000)
#define w_totalcycles   (((F_CPU/1000)*w_totalperiod +500000)/1000000)

// w1 - nops between rising edge and falling edge - low
#define w1 (w_zerocycles-w_fixedlow)
// w2   nops between fe low and fe high
#define w2 (w_onecycles-w_fixedhigh-w1)
// w3   nops to complete loop
#define w3 (w_totalcycles-w_fixedtotal-w1-w2)

#if w1>0
  #define w1_nops w1
#else
  #define w1_nops  0
#endif

// The only critical timing parameter is the minimum pulse length of the "0"
// Warn or throw error if this timing can not be met with current F_CPU settings.
#define w_lowtime ((w1_nops+w_fixedlow)*1000000)/(F_CPU/1000)
#if w_lowtime>550
   #error "Light_ws2812: Sorry, the clock speed is too low. Did you set F_CPU correctly?"
#elif w_lowtime>450
   #warning "Light_ws2812: The timing is critical and may only work on WS2812B, not on WS2812(S)."
   #warning "Please consider a higher clockspeed, if possible"
#endif   

#if w2>0
#define w2_nops w2
#else
#define w2_nops  0
#endif

#if w3>0
#define w3_nops w3
#else
#define w3_nops  0
#endif

#define w_nop1  "nop      \n\t"
#ifdef interrupt_is_disabled
#define w_nop2  "brid .+0 \n\t"
#else
#define w_nop2  "brtc .+0 \n\t"
#endif
#define w_nop4  w_nop2 w_nop2
#define w_nop8  w_nop4 w_nop4
#define w_nop16 w_nop8 w_nop8

#define MAXPIX 23
//#define COLORLENGTH (MAXPIX/2)
//#define FADE (256/COLORLENGTH)

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

//prototypy

struct cRGB  { uint8_t g; uint8_t r; uint8_t b; };
//void ws2812_setleds     (struct cRGB  *ledarray, uint16_t number_of_leds);
//void ws2812_setleds_pin (struct cRGB  *ledarray, uint16_t number_of_leds,uint8_t pinmask);

//void ws2812_sendarray     (uint8_t *array,uint16_t length);
void ws2812_sendarray_mask(uint8_t *array,uint16_t length, uint8_t pinmask);

// Normally ws2812_sendarray_mask() runs under disabled-interrupt condition,
// undefine if you want to accept interrupts in that function.
 /*
// Setleds for standard RGB 
void inline ws2812_setleds(struct cRGB *ledarray, uint16_t leds)
{
   ws2812_setleds_pin(ledarray,leds, _BV(ws2812_pin));
}

void inline ws2812_setleds_pin(struct cRGB *ledarray, uint16_t leds, uint8_t pinmask)
{
  ws2812_sendarray_mask((uint8_t*)ledarray,leds+leds+leds,pinmask);
  _delay_us(ws2812_resettime);
}

void ws2812_sendarray(uint8_t *data,uint16_t datlen)
{
  ws2812_sendarray_mask(data,datlen,_BV(ws2812_pin));
}


  This routine writes an array of bytes with RGB values to the Dataout pin
  using the fast 800kHz clockless WS2811/2812 protocol.
*/

void inline ws2812_sendarray_mask(uint8_t *data,uint16_t datlen,uint8_t maskhi)
{
  // `maskhi` is 0x80 if P?7 is LED DATA
  uint8_t curbyte,ctr,masklo;
  uint8_t sreg_prev;
#if __AVR_ARCH__ != 100  
  uint8_t *port = (uint8_t*) _SFR_MEM_ADDR(ws2812_PORTREG);
#endif

  ws2812_DDRREG |= maskhi; // Enable output
  
  // `masklo` and `maskhi` are written to PORT? to drive the DATA line low or
  // high (rather than setting or clearing the bit in PORT?)
  masklo	=~maskhi&ws2812_PORTREG;
  maskhi |=        ws2812_PORTREG;
  
  sreg_prev=SREG;

#ifdef interrupt_is_disabled
  cli();  
#endif  

  while (datlen--) {
    curbyte=*data++;
    
    __asm__ volatile(
    "       ldi   %0,8  \n\t"
#ifndef interrupt_is_disabled
    "       clt         \n\t"
#endif
    "loop%=:            \n\t"
#if __AVR_ARCH__ == 100     
    "       out   %2,%3 \n\t"    //  '1' [01] '0' [01] - re
#else
    "       st    X,%3 \n\t"    //  '1' [02] '0' [02] - re
#endif

#if (w1_nops&1)
w_nop1
#endif
#if (w1_nops&2)
w_nop2
#endif
#if (w1_nops&4)
w_nop4
#endif
#if (w1_nops&8)
w_nop8
#endif
#if (w1_nops&16)
w_nop16
#endif
#if defined(__LGT8F__)
    "       bst   %1,7  \n\t"    //  '1' [02] '0' [02]
    "       brts  1f    \n\t"    //  '1' [04] '0' [03]
    "       st    X,%4  \n\t"    //  '1' [--] '0' [04] - fe-low
    "1:     lsl   %1    \n\t"    //  '1' [05] '0' [05]
#elif __AVR_ARCH__ == 100     
    "       sbrs  %1,7  \n\t"    //  '1' [03] '0' [02]
    "       out   %2,%4 \n\t"    //  '1' [--] '0' [03] - fe-low
    "       lsl   %1    \n\t"    //  '1' [04] '0' [04]    
#else
    "       sbrs  %1,7  \n\t"    //  '1' [04] '0' [03]
    "       st    X,%4 \n\t"     //  '1' [--] '0' [05] - fe-low
    "       lsl   %1    \n\t"    //  '1' [05] '0' [06]
#endif
#if (w2_nops&1)
  w_nop1
#endif
#if (w2_nops&2)
  w_nop2
#endif
#if (w2_nops&4)
  w_nop4
#endif
#if (w2_nops&8)
  w_nop8
#endif
#if (w2_nops&16)
  w_nop16 
#endif
#if __AVR_ARCH__ == 100     
    "       out   %2,%4 \n\t"    //  '1' [+1] '0' [+1] - fe-high
#else
    "       brcc skipone%= \n\t"    //  '1' [+1] '0' [+2] - 
    "       st   X,%4      \n\t"    //  '1' [+3] '0' [--] - fe-high
    "skipone%=:               "     //  '1' [+3] '0' [+2] - 
#endif    
#if (w3_nops&1)
w_nop1
#endif
#if (w3_nops&2)
w_nop2
#endif
#if (w3_nops&4)
w_nop4
#endif
#if (w3_nops&8)
w_nop8
#endif
#if (w3_nops&16)
w_nop16
#endif

    "       dec   %0    \n\t"    //  '1' [+4] '0' [+3]
    "       brne  loop%=\n\t"    //  '1' [+5] '0' [+4]
    :	"=&d" (ctr)
#if __AVR_ARCH__ == 100    
    :	"r" (curbyte), "I" (_SFR_IO_ADDR(ws2812_PORTREG)), "r" (maskhi), "r" (masklo)
#else    
    :	"r" (curbyte), "x" (port), "r" (maskhi), "r" (masklo)
#endif  

    );
  }
  
  SREG=sreg_prev;
}

struct cRGB colors[MAXPIX];
struct cRGB led[MAXPIX];


void start_sequence()
{
  uint8_t i,j;
  for(j=0;j<MAXPIX;j++)
      {
        for (i =0;i<MAXPIX;i++)
        {
          led[i].r= 0;
          led[i].g= 0;
          led[i].b= 0;
          
        }

        led[j].r=colors[j].r;
        led[j].g=colors[j].g;
        
      _delay_ms(10);
      ws2812_sendarray_mask((uint8_t *)led,MAXPIX*3,_BV(ws2812_pin));
      _delay_ms(50);
}
  for(j=MAXPIX;j>0;j--)
      {
        for (i =0;i<MAXPIX;i++)
        {
          led[i].r= 0;
          led[i].g= 0;
          led[i].b= 0;
          
        }

        led[j].r=colors[j].r;
        led[j].g=colors[j].g;
        
       _delay_ms(10);
      ws2812_sendarray_mask((uint8_t *)led,MAXPIX*3,_BV(ws2812_pin));
      _delay_ms(50);

      }

      for (i =0;i<MAXPIX;i++){
          led[i].r= 0;
          led[i].g= 0;
          led[i].b= 0;
          
        }
          _delay_ms(10);
      ws2812_sendarray_mask((uint8_t *)led,MAXPIX*3,_BV(ws2812_pin));

}


int main(void){
	

  colors[0].r=64; colors[0].g=0; //red0/22
  colors[1].r=64; colors[1].g=0; //red1/21
  colors[2].r=64; colors[2].g=0; //red2/20
  colors[3].r=64; colors[3].g=0; //red3/19
  colors[4].r=64; colors[4].g=5; //red4/18
  colors[5].r=56; colors[5].g=8; //red5/17
  colors[6].r=48; colors[6].g=12; //red6/16
  colors[7].r=32; colors[7].g=16; //red7/15
  colors[8].r=24; colors[8].g=24; //red8/14
  colors[9].r=12; colors[9].g=32; //red9/13
  colors[10].r=6; colors[10].g=32; //red 10/12
  colors[11].r=0; colors[11].g=128; //red
  colors[12].r=6; colors[12].g=32; //red
  colors[13].r=12; colors[13].g=32; //red
  colors[14].r=24; colors[14].g=24; //red
  colors[15].r=32; colors[15].g=16; //red7/15
  colors[16].r=48; colors[16].g=12; //red6/16
  colors[17].r=56; colors[17].g=8; //red5/17
  colors[18].r=64; colors[18].g=5; //red4/18
  colors[19].r=64; colors[19].g=0; //red3/19
  colors[20].r=64; colors[20].g=0; //red2/20
  colors[21].r=64; colors[21].g=0; //red1/21
  colors[22].r=64; colors[22].g=0; //red0/22
	

  //je nutne nastavit vstupne porty PD0 PD1 PD2 PD3 PD4
	DDRD= 0x00;
	// je nutne zapnout PULL-UP odpory, protoze tranzistory v optoclenech spinaji proti zemi
	PORTD=0xFF;

	DDRC|=_BV(ws2812_pin);
		
  uint8_t i;
  uint8_t output;

  for (i=0;i<3;i++)
  {
    start_sequence();
  }
  
  while(1){

    output=0xFF-PIND;

    if (output>MAXPIX-1)
    {
        output = MAXPIX-1;
    }  

    for (i =0;i<MAXPIX;i++)
    {
      led[i].r= 0;
      led[i].g= 0;
      led[i].b= 0;    
    }

    led[output].r=colors[output].r;
    led[output].g=colors[output].g;
          
    _delay_ms(10);
    ws2812_sendarray_mask((uint8_t *)led,MAXPIX*3,_BV(ws2812_pin));
    _delay_ms(50);

  }
}
