#include <stddef.h>
#include <stdarg.h>

#include "gfx.h"
#include "input.h"
#include "util.h"
#include "hdmi.h"
#include "printf/printf.h"

#define TILE_MAX_W 128

#define CHR_HORIZ 18
#define CHR_VERT 124
#define CHR_TR 5
#define CHR_TL 17
#define CHR_BR 3
#define CHR_BL 26


typedef struct
{
    uint16_t hofs;
    uint16_t vofs;
} TilemapCtrl;

typedef struct
{
    uint16_t value;
    uint16_t span;
    uint16_t skip;
    uint16_t repeat;

    uint16_t addr;
    uint16_t pad0;
    uint16_t pad1;
    volatile uint16_t submit;
} Blitter;

TilemapCtrl *tile_ctrl = (TilemapCtrl *)0x910000;
Blitter *blitter = (Blitter *)0x930000;

TilemapCtrl page_tile_ctrl[2];
uint16_t *page_vram[2];
uint16_t *vram_base = (uint16_t *)0x900000; // 128 * 128 = 16384 words
uint16_t *vram = (uint16_t *)0x900000;
uint8_t page_front = 0;
uint16_t tile_w;
uint16_t tile_h;

typedef struct
{
    uint8_t pen;
    bool sameline;
    uint16_t rx, ry;
    uint16_t rw, rh;
    uint16_t y;
    uint16_t prevx;
    int highlighted;
    MenuContext *menuctx;
} Context;

#define NUM_CTX 8
Context contexts[NUM_CTX];
int context_idx = 0;
Context *ctx = &contexts[0];

void gfx_set_240p(float hz, bool wide)
{
    VideoMode mode;

    if (wide)
    {
        mode.hact = 384;
        mode.hfp = 16;
        mode.hs = 32;
        mode.hbp = 48;
        mode.vact = 216;
        mode.vfp = 3;
        mode.vs = 5;
        mode.vbp = 6;

        contexts[0].rx = 0;
        contexts[0].ry = 0;
        contexts[0].rw = 48;
        contexts[0].rh = 27;
    }
    else
    {
        mode.hact = 320;
        mode.hfp = 8;
        mode.hs = 32;
        mode.hbp = 40;
        mode.vact = 240;
        mode.vfp = 3;
        mode.vs = 4;
        mode.vbp = 6;

        contexts[0].rx = 0;
        contexts[0].ry = 0;
        contexts[0].rw = 40;
        contexts[0].rh = 30;
    }

    uint32_t pixels = video_mode_pixels(&mode);
    mode.mhz = (hz * pixels) / 1000000.0;
    crt_set_mode(&mode, wide);

    page_tile_ctrl[0].hofs = -(mode.hs + mode.hfp- 9);
    page_tile_ctrl[0].vofs = -mode.vbp;
    page_vram[0] = vram_base;

    page_tile_ctrl[1].hofs = -(mode.hs + mode.hfp - 9);
    page_tile_ctrl[1].vofs = (64 * 8) - mode.vbp;
    page_vram[1] = vram_base + (TILE_MAX_W * 64);

    gfx_pageflip();
}

void gfx_pageflip()
{
    uint8_t page_back = page_front;
    page_front = page_front ? 0 : 1;

    tile_ctrl->hofs = page_tile_ctrl[page_front].hofs;
    tile_ctrl->vofs = page_tile_ctrl[page_front].vofs;
    vram = page_vram[page_back];

    context_idx = 0;
    gfx_push();
}

void gfx_clear()
{
    uint16_t base_addr = (((uint32_t)vram) >> 1) & 0xffff;
    uint16_t ofs = ( ctx->ry * TILE_MAX_W ) + ctx->rx;
    const uint16_t skip = TILE_MAX_W - (ctx->rw - 1);
    blitter->addr = base_addr + ofs;
    blitter->value = 0x0020;
    blitter->span = ctx->rw - 1;
    blitter->repeat = ctx->rh - 1;
    blitter->skip = TILE_MAX_W - (ctx->rw - 1);
    blitter->submit = 1;
}

void gfx_push()
{
    if (context_idx < NUM_CTX)
    {
        context_idx++;
        memcpy(&contexts[context_idx], ctx, sizeof(Context));
        ctx = &contexts[context_idx];
    }
    else
    {
        context_idx++;
    }
}

void gfx_pop()
{
    if (context_idx > 0) context_idx--;
    if (context_idx < NUM_CTX) ctx = &contexts[context_idx];
}

void gfx_align_box(Align align, int16_t xadj, int16_t yadj, uint16_t w, uint16_t h, int16_t *x, int16_t *y)
{
    if( align & ALIGN_CENTER )
    {
        *x = ((ctx->rw - w) >> 1) + xadj;
    }
    else if (align & ALIGN_RIGHT)
    {
        *x = ((ctx->rw - w) - xadj);
    }
    else
    {
        *x = xadj;
    }

    if( align & ALIGN_MIDDLE )
    {
        *y = ((ctx->rh - h) >> 1) + yadj;
    }
    else if (align & ALIGN_BOTTOM)
    {
        *y = ((ctx->rh - h) - yadj);
    }
    else
    {
        *y = yadj;
    }
}

void gfx_begin_window(Align align, int16_t x, int16_t y, uint16_t w, uint16_t h, int16_t frame)
{
    gfx_push();
    gfx_align_box(align, x, y, w, h, &ctx->rx, &ctx->ry);
    ctx->rw = w;
    ctx->rh = h;

    if (frame > 0)
    {
        gfx_frame( 0, 0, w, h );
        ctx->rx += frame;
        ctx->ry += frame;
        ctx->rw -= frame << 1;
        ctx->rh -= frame << 1;
    }
}

void gfx_end_window()
{
    gfx_pop();
}

void gfx_begin_menu(const char *name, uint16_t w, uint16_t h, MenuContext *menuctx)
{
    uint16_t x = (ctx->rw - w) >> 1;
    uint16_t y = (ctx->rh - h) >> 1;

    gfx_pen(TEXT_BLUE);

    gfx_begin_window(ALIGN_CENTER | ALIGN_MIDDLE, 0, 0, w, h, 1);

    ctx->menuctx = menuctx;
    uint16_t pressed = input_pressed();
    if (pressed & INPUT_UP)
    {
        menuctx->index--;
        menuctx->tmp_option_idx = -1;
    }
    if (pressed & INPUT_DOWN)
    {
        menuctx->index++;
        menuctx->tmp_option_idx = -1;
    }

    if (menuctx->index >= menuctx->count) menuctx->index = 0;
    if (menuctx->index < 0) menuctx->index = menuctx->count - 1;
    menuctx->count = 0;

    gfx_pen(TEXT_DARK_GRAY);
    gfx_text_aligned(ALIGN_CENTER, name);
}

void gfx_end_menu()
{
    gfx_end_window();
}

bool gfx_menuitem_select_func(const char *label, const void *options, int num_options, MenuItemToString item_to_string, int *option_index)
{
    char txt[40];
    bool changed = false;
    MenuContext *menuctx = ctx->menuctx;
    if( menuctx == NULL ) return false;

    bool selected = menuctx->count == menuctx->index;
    if (selected)
    {
        uint16_t pressed = input_pressed();

        int index = *option_index;

        if (pressed & INPUT_RIGHT)
        {
            index++;
        }
        if (pressed & INPUT_LEFT)
        {
            index--;
        }
        if (index < 0) index = num_options - 1;
        if (index >= num_options) index = 0;

        if (index != *option_index)
        {
            *option_index = index;
            changed = true;
        }
    }

    const char *value_str = item_to_string(options, *option_index);
    int width = ctx->rw;
    int label_len = strlen(label);
    int value_len = strlen(value_str);
    int space_len = width - ( 2 + label_len + value_len );
    space_len = space_len < 0 ? 0 : space_len;
    char *p = txt;
    *p++ = 126; // <
    for( int i = 0; i < label_len; i++ )
        *p++ = label[i];
    for( int i = 0; i < space_len; i++ )
        *p++ = 0x20;
    for( int i = 0; i < value_len; i++ )
        *p++ = value_str[i];
    *p++ = 127; // >
    *p = '\0';

    gfx_newline(1);
    if (selected)
    {
        gfx_pen(TEXT_GRAY | TEXT_INVERT);
    }
    else
    {
        gfx_pen(TEXT_GRAY);
    }
    gfx_text(txt);

    menuctx->count++;
    return changed;
}

static const char *menuitem_to_string_simple(const void *options, int index)
{
    const char **options_str = (const char **)options;

    return options_str[index];
}

bool gfx_menuitem_select(const char *label, const char **options, int num_options, int *option_index)
{
    return gfx_menuitem_select_func(label, options, num_options, menuitem_to_string_simple, option_index);
}

bool gfx_menuitem_button(const char *label)
{
    bool changed = false;
    MenuContext *menuctx = ctx->menuctx;
    if( menuctx == NULL ) return false;

    bool selected = menuctx->count == menuctx->index;
    if (selected)
    {
        uint16_t pressed = input_pressed();
        if (pressed & INPUT_OK) changed = true;
    }

    gfx_newline(1);
    if (selected)
    {
        gfx_pen(TEXT_GRAY | TEXT_INVERT);
        gfx_text_aligned(ALIGN_CENTER, label);
    }
    else
    {
        gfx_pen(TEXT_GRAY);
        gfx_text_aligned(ALIGN_CENTER, label);
    }
    
    menuctx->count++;
    return changed;
}

void gfx_pen(int color)
{
    ctx->pen = color;
}

void gfx_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t base_addr = (((uint32_t)vram) >> 1) & 0xffff;
    uint16_t ofs = ( (ctx->ry + y) * TILE_MAX_W ) + x + ctx->rx;
    blitter->addr = base_addr + ofs;
    blitter->value = ctx->pen << 8 | 0x20;
    blitter->span = w - 1;
    blitter->repeat = h - 1;
    blitter->skip = TILE_MAX_W - (w - 1);
    blitter->submit = 1;
}

void gfx_frame(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t color = ctx->pen << 8;
    int ofs = ( (ctx->ry + y) * TILE_MAX_W ) + x + ctx->rx;

    vram[ofs] = color | CHR_TL;
    vram[ofs + w - 1] = color | CHR_TR;
    for( int c = 1; c < (w - 1); c++ )
        vram[ofs + c] = color | CHR_HORIZ;
    ofs += TILE_MAX_W;

    for( int r = 1; r < (h - 1); r++ )
    {
        vram[ofs] = color | CHR_VERT;
        vram[ofs + w - 1] = color | CHR_VERT;
        ofs += TILE_MAX_W;
    }

    vram[ofs] = color | CHR_BL;
    vram[ofs + w - 1] = color | CHR_BR;
    for( int c = 1; c < (w - 1); c++ )
        vram[ofs + c] = color | CHR_HORIZ;
}

void gfx_text_aligned(Align align, const char *str)
{
    uint16_t x = 0;
    int len = strlen(str);

    if (ctx->sameline)
    {
        ctx->sameline = false;
        ctx->y = ctx->y > 0 ? ctx->y - 1 : 0;
    }
    else
    {
        ctx->prevx = 0;
    }
    
    if (align & ALIGN_LEFT)
        x = ctx->prevx;
    else if (align & ALIGN_RIGHT)
        x = ctx->rw - len;
    else if (align & ALIGN_CENTER)
        x = (ctx->rw - len) >> 1;

    uint16_t ofs = ( (ctx->ry + ctx->y) * TILE_MAX_W ) + x + ctx->rx;

    while(*str)
    {
        vram[ofs] = (((uint16_t)ctx->pen) << 8) | *str;
        ofs++;
        x++;
        str++;
    }

    ctx->y++;
    ctx->prevx = x;
}

void gfx_textf_aligned(Align align, const char *fmt, ...)
{
    char tmp[40];
    va_list args;
   
    va_start(args, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);

    gfx_text_aligned(align, tmp);
}

void gfx_sameline()
{
    ctx->sameline = true;
}

void gfx_newline(int n)
{
    ctx->y += n;
}
