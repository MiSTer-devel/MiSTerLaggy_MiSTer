#if !defined(GFX_H)
#define GFX_H 1

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    ALIGN_LEFT = 1 << 0,
    ALIGN_RIGHT = 1 << 1,
    ALIGN_CENTER = 1 << 2,
    ALIGN_TOP = 1 << 3,
    ALIGN_MIDDLE = 1 << 4,
    ALIGN_BOTTOM = 1 << 5
} TextAlign;

typedef struct
{
    int index;
    int count;
    int tmp_option_idx;
} MenuContext;

#define INIT_MENU_CONTEXT { .index = -1, .count = -1, .tmp_option_idx = -1 }

void gfx_set_240p(float hz);

void gfx_pageflip();

void gfx_clear();

void gfx_push();
void gfx_pop();

void gfx_begin_region(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void gfx_end_region();
void gfx_begin_menu(const char *name, uint16_t w, uint16_t h, MenuContext *menuctx);
void gfx_end_menu();
bool gfx_menuitem_select(const char *label, const char **options, int num_options, int *option_index);
bool gfx_menuitem_button(const char *label);

void gfx_pen(int color256);

void gfx_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void gfx_frame(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

#define gfx_text(x) gfx_text_aligned(ALIGN_LEFT, x);
#define gfx_textf(x, ...) gfx_textf_aligned(ALIGN_LEFT, x, __VA_ARGS__);
void gfx_textf_aligned(TextAlign align, const char *fmt, ...);
void gfx_text_aligned(TextAlign align, const char *str);
void gfx_sameline();
void gfx_newline(int n);

#define TEXT_INVERT      0x01
#define TEXT_STRAW       0x00
#define TEXT_TAN         0x02
#define TEXT_BROWN       0x04
#define TEXT_DARK_BROWN  0x06
#define TEXT_DARK_RED    0x08
#define TEXT_RED         0x0a
#define TEXT_ORANGE      0x0c
#define TEXT_YELLOW      0x0e
#define TEXT_LIME        0x10
#define TEXT_GREEN       0x12
#define TEXT_DARK_GREEN  0x14
#define TEXT_SLATE       0x16
#define TEXT_GRAY        0x18
#define TEXT_WHITE       0x1a
#define TEXT_AQUA        0x1c
#define TEXT_BLUE        0x1e




#endif // GFX_H