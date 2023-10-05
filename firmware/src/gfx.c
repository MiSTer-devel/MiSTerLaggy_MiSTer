#include <stddef.h>

#include "gfx.h"
#include "input.h"
#include "util.h"

#define TILE_MAX_W 128

#define CHR_HORIZ 18
#define CHR_VERT 124
#define CHR_TR 5
#define CHR_TL 17
#define CHR_BR 3
#define CHR_BL 26

typedef volatile struct
{
    uint16_t clk_numer;
    uint16_t clk_denom;

    uint16_t hact;
    uint16_t hfp;
    uint16_t hs;
    uint16_t hbp;
    uint16_t vact;
    uint16_t vfp;
    uint16_t vs;
    uint16_t vbp;

    uint16_t hcnt;
    uint16_t vcnt;
} CRTC;

typedef struct
{
    uint16_t hofs;
    uint16_t vofs;
} TilemapCtrl;


CRTC *crtc = (CRTC *)0x800000;
TilemapCtrl *tile_ctrl = (TilemapCtrl *)0x910000;

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

float gfx_set_240p(float hz)
{
    const float total_pix = 400.0f * 253.0f;
    double pix_hz = ( hz * total_pix );
    double num = pix_hz / 24000.0;
    int rounded_num = num + 0.5;

    float real_hz = (24000.0 * rounded_num) / total_pix;

    crtc->clk_numer = rounded_num;
    crtc->clk_denom = 1000;

    crtc->hact = 320;
    crtc->hfp = 8;
    crtc->hs = 32;
    crtc->hbp = 40;

    crtc->vact = 240;
    crtc->vfp = 3;
    crtc->vs = 4;
    crtc->vbp = 6;

    contexts[0].rx = 0;
    contexts[0].ry = 0;
    contexts[0].rw = 40;
    contexts[0].rh = 30;

    page_tile_ctrl[0].hofs = -31;
    page_tile_ctrl[0].vofs = -6;
    page_vram[0] = vram_base;

    page_tile_ctrl[1].hofs = -31;
    page_tile_ctrl[1].vofs = (64 * 8) - 6;
    page_vram[1] = vram_base + (TILE_MAX_W * 64);

    gfx_pageflip();

    return real_hz;
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
    gfx_push();
    gfx_pen(0);
    gfx_rect(0, 0, ctx->rw, ctx->rh);
    gfx_pop();
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

void gfx_begin_region(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    gfx_push();
    ctx->rx += x;
    ctx->ry += y;
    ctx->rw = w;
    ctx->rh = h;
}

void gfx_end_region()
{
    gfx_pop();
}

void gfx_begin_menu(const char *name, uint16_t w, uint16_t h, MenuContext *menuctx)
{
    uint16_t x = (ctx->rw - w) >> 1;
    uint16_t y = (ctx->rh - h) >> 1;

    gfx_pen(TEXT_LIME);

    gfx_frame( x, y, w, h );
    gfx_begin_region(x + 1, y + 1, w - 2, h - 2);

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

    gfx_text_aligned(name, ALIGN_CENTER);
}

void gfx_end_menu()
{
    gfx_end_region();
}

bool gfx_menuitem_select_deferred(const char *label, const char **options, int num_options, int *option_index)
{
    bool changed = false;
    MenuContext *menuctx = ctx->menuctx;
    if( menuctx == NULL ) return false;

    bool selected = menuctx->count == menuctx->index;
    if (selected)
    {
        uint16_t pressed = input_pressed();
        if (menuctx->tmp_option_idx == -1) menuctx->tmp_option_idx = *option_index;

        if (pressed & INPUT_RIGHT)
        {
            menuctx->tmp_option_idx++;
        }
        if (pressed & INPUT_LEFT)
        {
            menuctx->tmp_option_idx--;
        }
        if (menuctx->tmp_option_idx < 0) menuctx->tmp_option_idx = num_options - 1;
        if (menuctx->tmp_option_idx >= num_options) menuctx->tmp_option_idx = 0;

        if (pressed & INPUT_OK)
        {
            *option_index = menuctx->tmp_option_idx;
            changed = true;
        }
    }

    gfx_newline(1);
    if (selected)
    {
        gfx_pen(TEXT_GRAY | TEXT_INVERT);
        gfx_text_aligned(label, ALIGN_LEFT);
        gfx_pen(TEXT_GRAY | TEXT_INVERT);
        gfx_text_aligned(options[menuctx->tmp_option_idx], ALIGN_RIGHT);
    }
    else
    {
        gfx_pen(TEXT_GRAY);
        gfx_text_aligned(label, ALIGN_LEFT);
        gfx_pen(TEXT_GRAY);
        gfx_text_aligned(options[*option_index], ALIGN_RIGHT);
    }
    
    menuctx->count++;
    return changed;
}

bool gfx_menuitem_select(const char *label, const char **options, int num_options, int *option_index)
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

    const char *value_str = options[*option_index];
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
        gfx_text_aligned(label, ALIGN_CENTER);
    }
    else
    {
        gfx_pen(TEXT_GRAY);
        gfx_text_aligned(label, ALIGN_CENTER);
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
    int ofs = ( (ctx->ry + y) * TILE_MAX_W ) + x + ctx->rx;
    uint16_t tile = ctx->pen << 8 | 0x20;

    for( int i = 0; i < h; i++ )
    {
        memsetw(&vram[ofs], tile, w);
        ofs += TILE_MAX_W;
    }
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

void gfx_text_aligned(const char *str, TextAlign align)
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

void gfx_sameline()
{
    ctx->sameline = true;
}

void gfx_newline(int n)
{
    ctx->y += n;
}
