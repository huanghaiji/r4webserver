#include <Arduino.h>
#include "qrcode.h"
#include "qrencode.h"

int offsetsX = 10;
int offsetsY = 10;
int screenwidth = 320;
int screenheight = 170;
bool QRDEBUG = false;

QRcode::QRcode(){ 
}

void QRcode::init(){
	if(QRDEBUG)
		Serial.println("QRcode init");
}

void QRcode::debug(){
	QRDEBUG = true;
}

void QRcode::render(int x, int y, int color){
  x=x+offsetsX;
  y=y+offsetsY;
  if(color==1) {
    Paint_SetPixel(x*2 , y*2, BLACK);
    Paint_SetPixel(x*2, y*2+1, BLACK);
    Paint_SetPixel(x*2+1,y*2,BLACK);
    Paint_SetPixel(x*2+1,y*2+1,BLACK);
  }
  else {
    Paint_SetPixel(x*2, y*2, WHITE);
    Paint_SetPixel(x*2, y*2+1, WHITE);
    Paint_SetPixel(x*2+1,y*2,WHITE);
    Paint_SetPixel(x*2+1,y*2+1,WHITE);
  }
}

void QRcode::screenwhite(){
   Paint_Clear(WHITE);
}

void QRcode::create(String message,int offx,int offy) {
  offsetsX = offx;
  offsetsY = offy;

  // create QR code
  message.toCharArray((char *)strinbuf,260);
  qrencode();

  if(QRDEBUG){
	Serial.println("QRcode render");
	Serial.println();
  }
  // print QR Code
  for (byte x = 0; x < WD; x+=2) {
    for (byte y = 0; y < WD; y++) {
      if ( QRBIT(x,y) &&  QRBIT((x+1),y)) {
        // black square on top of black square
        render(x, y, 1);
        render((x+1), y, 1);
      }
      if (!QRBIT(x,y) &&  QRBIT((x+1),y)) {
        // white square on top of black square
        render(x, y, 0);
        render((x+1), y, 1);
      }
      if ( QRBIT(x,y) && !QRBIT((x+1),y)) {
        // black square on top of white square
        render(x, y, 1);
        render((x+1), y, 0);
      }
      if (!QRBIT(x,y) && !QRBIT((x+1),y)) {
        // white square on top of white square
        render(x, y, 0);
        render((x+1), y, 0);
      }
    }
  }
}
