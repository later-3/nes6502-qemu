#ifndef __HAL_H__
#define __HAL_H__

#define FPS 60
#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 240

struct Pixel {
    int x, y; // (x, y) coordinate
    int c; // RGB value of colors can be found in fce.h
};
typedef struct Pixel Pixel;

/* A buffer of pixels */
struct PixelBuf {
	Pixel buf[264 * 264];
	int size;
};
typedef struct PixelBuf PixelBuf;

extern PixelBuf bg, bbg, fg;

// clear a pixel buffer
#define pixbuf_clean(bf) \
	do { \
		(bf).size = 0; \
	} while (0)

// add a pending pixel into a buffer
#define pixbuf_add(bf, xa, ya, ca) \
	do { \
		if ((xa) < SCREEN_WIDTH && (ya) < SCREEN_HEIGHT) { \
			(bf).buf[(bf).size].x = (xa); \
			(bf).buf[(bf).size].y = (ya); \
			(bf).buf[(bf).size].c = (ca); \
			(bf).size++; \
		} \
	} while (0)

/* Type: NESFB_COLOR
 */
typedef struct NESFB_COLOR NESFB_COLOR;

struct NESFB_COLOR
{
   uint8_t r, g, b, a;
};

typedef enum {
    NESFB_SET_BG_COLOR,
	NFSFB_FLUSH_BBG,
	NFSFB_FLUSH_BG,
	NFSFB_FLUSH_FG,
	NFSFB_FLIP_DISPLAY,
} NESFB_OPCODE;

/* Type: NESFB_VERTEX
 */
typedef struct NESFB_VERTEX NESFB_VERTEX;

struct NESFB_VERTEX {
  int x, y, z;
  int u, v;
  unsigned int  color;
};

// Palette

typedef struct __pal {
	int r;
	int g;
	int b;
} pal;

void nes_flip_display(void *opaque);

#endif
