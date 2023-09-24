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

void gfx_set_ntsc_224p();

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

void gfx_pen16(int color16);
void gfx_pen256(int color256);

void gfx_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

#define gfx_text(x) gfx_text_aligned(x, ALIGN_LEFT);
void gfx_text_aligned(const char *str, TextAlign align);
void gfx_sameline();
void gfx_newline(int n);

#endif // GFX_H