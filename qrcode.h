#include "LCD_Driver.h"
#include "GUI_Paint.h"

class QRcode
{
	private:
		void render(int x, int y, int color);

	public:
		QRcode();

		void init();
		void debug();
		void screenwhite();
		void create(String message,int offsetX,int offsetY);
};
