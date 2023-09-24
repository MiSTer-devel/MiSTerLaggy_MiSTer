#include <stddef.h>

#include "gfx.h"
#include "input.h"
#include "util.h"

#define TILE_MAX_W 128

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

void gfx_set_ntsc_224p()
{
    crtc->clk_numer = 1;
    crtc->clk_denom = 4;

    crtc->hact = 320;
    crtc->hfp = 8;
    crtc->hs = 32;
    crtc->hbp = 40;

    crtc->vact = 224;
    crtc->vfp = 10;
    crtc->vs = 6;
    crtc->vbp = 10;

    contexts[0].rx = 0;
    contexts[0].ry = 0;
    contexts[0].rw = 40;
    contexts[0].rh = 28;

    page_tile_ctrl[0].hofs = -40;
    page_tile_ctrl[0].vofs = -10;
    page_vram[0] = vram_base;

    page_tile_ctrl[1].hofs = -40;
    page_tile_ctrl[1].vofs = (64 * 8) -10;
    page_vram[1] = vram_base + (TILE_MAX_W * 64);

    gfx_pageflip();

    tile_ctrl->hofs = -40;
    tile_ctrl->vofs = -10;
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
    gfx_pen256(0);
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
    gfx_begin_region(x, y, w, h);

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

    gfx_pen16(6);
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
        gfx_pen16(4);
        gfx_text_aligned(label, ALIGN_LEFT);
        gfx_pen16(9);
        gfx_text_aligned(options[menuctx->tmp_option_idx], ALIGN_RIGHT);
    }
    else
    {
        gfx_pen16(2);
        gfx_text_aligned(label, ALIGN_LEFT);
        gfx_pen16(2);
        gfx_text_aligned(options[*option_index], ALIGN_RIGHT);
    }
    
    menuctx->count++;
    return changed;
}

bool gfx_menuitem_select(const char *label, const char **options, int num_options, int *option_index)
{
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

    gfx_newline(1);
    if (selected)
    {
        gfx_pen16(4);
        gfx_text_aligned(label, ALIGN_LEFT);
        gfx_pen16(9);
        gfx_text_aligned(options[*option_index], ALIGN_RIGHT);
    }
    else
    {
        gfx_pen16(2);
        gfx_text_aligned(label, ALIGN_LEFT);
        gfx_pen16(2);
        gfx_text_aligned(options[*option_index], ALIGN_RIGHT);
    }
    
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
        gfx_pen16(4);
        gfx_text_aligned(label, ALIGN_CENTER);
    }
    else
    {
        gfx_pen16(2);
        gfx_text_aligned(label, ALIGN_CENTER);
    }
    
    menuctx->count++;
    return changed;
}

void gfx_pen16(int color16)
{
    ctx->pen = color16 << 4;
}

void gfx_pen256(int color256)
{
    ctx->pen = color256;
}

void gfx_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    int ofs = ( (ctx->ry + y) * TILE_MAX_W ) + x + ctx->rx;
    uint16_t tile = ( ( ctx->pen & 0xf0 ) << 8 ) | ( 0x10 + ( ctx->pen & 0x0f ) );

    for( int i = 0; i < h; i++ )
    {
        memsetw(&vram[ofs], tile, w);
        ofs += TILE_MAX_W;
    }
}

void gfx_text_aligned(const char *str, TextAlign align)
{
    uint16_t x = 0;
    int len = strlen(str);

    if (ctx->sameline)
    {
        ctx->sameline = false;
        ctx->y = ctx->y > 0 ? ctx->y - 1 : 0;
        x = ctx->prevx;
    }
    else if (align & ALIGN_LEFT)
        x = 0;
    else if (align & ALIGN_RIGHT)
        x = ctx->rw - len;
    else if (align & ALIGN_CENTER)
        x = (ctx->rw - len) >> 1;

    uint16_t ofs = ( (ctx->ry + ctx->y) * TILE_MAX_W ) + x + ctx->rx;

    while(*str)
    {
        vram[ofs] = ((ctx->pen & 0xf0) << 8) | *str;
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
