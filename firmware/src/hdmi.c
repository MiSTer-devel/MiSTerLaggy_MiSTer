#include <stdint.h>
#include <stdbool.h>

#include "hdmi.h"
#include "clock.h"

typedef volatile struct
{
    uint16_t ce_numer;
    uint16_t ce_denom;

    uint16_t hact;
    uint16_t hfp;
    uint16_t hs;
    uint16_t hbp;
    uint16_t vact;
    uint16_t vfp;
    uint16_t vs;
    uint16_t vbp;

    uint32_t pll_data;
    uint16_t pll_address;
    uint16_t pll_io;

    uint16_t hcnt;
    uint16_t vcnt;
} CRTC;

CRTC *crtc = (CRTC *)0x800000;

#define VIO_SET_MODE 1
#define VIO_SET_PLL 2
#define VIO_SET_OVERRIDE 3
#define VIO_SET_CFG 4

volatile uint16_t *vio_data = (volatile uint16_t *)0x600000;
volatile uint16_t *vio_en = (volatile uint16_t *)0x608000;

static void vio_enable()
{
    *vio_en = 0xffff;
}

static void vio_disable()
{
    *vio_en = 0x0000;
}

static uint16_t vio_w16(uint16_t d)
{
    *vio_data = d;
    return *vio_data;
}

static uint16_t vio_cmd(uint16_t cmd, uint16_t data)
{
    vio_enable();
    *vio_data = cmd;
    *vio_data = data;
    vio_disable();
    return *vio_data;
}

static uint16_t vio_cmd_cont(uint16_t cmd)
{
    vio_enable();
    *vio_data = cmd;
    return *vio_data;
}

static void vio_write_pll(uint16_t addr, uint32_t value)
{
    vio_cmd_cont(VIO_SET_PLL);
    vio_w16(addr);
    vio_w16(value & 0xffff);
    vio_w16(value >> 16);
    vio_disable();
}


static uint32_t getPLLdiv(uint32_t div)
{
	if (div & 1) return 0x20000 | (((div / 2) + 1) << 8) | (div / 2);
	return ((div / 2) << 8) | (div / 2);
}

static int findPLLpar(double Fout, uint32_t *pc, uint32_t *pm, double *pko)
{
	uint32_t c = 1;
	while ((Fout*c) < 400) c++;

	while (1)
	{
		double fvco = Fout*c;
		uint32_t m = (uint32_t)(fvco / 50);
		double ko = ((fvco / 50) - m);

		fvco = ko + m;
		fvco *= 50.f;

		if (ko && (ko <= 0.05f || ko >= 0.95f))
		{
			//printf("Fvco=%f, C=%d, M=%d, K=%f ", fvco, c, m, ko);
			if (fvco > 1500.f)
			{
				//printf("-> No exact parameters found\n");
				return 0;
			}
			//printf("-> K is outside allowed range\n");
			c++;
		}
		else
		{
			*pc = c;
			*pm = m;
			*pko = ko;
			return 1;
		}
	}

	//will never reach here
	return 0;
}

static void setPLL(double Fout)
{
	double Fpix;
	double fvco, ko;
	uint32_t m, c;

	//printf("Calculate PLL for %.4f MHz:\n", Fout);

	if (!findPLLpar(Fout, &c, &m, &ko))
	{
		c = 1;
		while ((Fout*c) < 400) c++;

		fvco = Fout*c;
		m = (uint32_t)(fvco / 50);
		ko = ((fvco / 50) - m);

		//Make sure K is in allowed range.
		if (ko <= 0.05f)
		{
			ko = 0;
		}
		else if (ko >= 0.95f)
		{
			m++;
			ko = 0;
		}
	}

	uint32_t k = ko ? (uint32_t)(ko * 4294967296) : 1;

	fvco = ko + m;
	fvco *= 50.f;
	Fpix = fvco / c;

	//printf("Fvco=%f, C=%d, M=%d, K=%f(%u) -> Fpix=%f\n", fvco, c, m, ko, k, Fpix);

    vio_write_pll(0, 0);
    vio_write_pll(4, getPLLdiv(m));
    vio_write_pll(3, 0x10000);
    vio_write_pll(5, getPLLdiv(c));
    vio_write_pll(9, 2);
    vio_write_pll(8, 7);
    vio_write_pll(7, k);
}

static void calculate_cvt(int h_pixels, int v_lines, float refresh_rate, bool reduced_blanking, VideoMode *vmode);

static bool hdmi_find_mode(uint16_t width, uint16_t height, float hz, VideoMode *mode)
{
    bool rb = (width * height) > ( 1920 * 1080);

    calculate_cvt(width, height, hz, rb, mode);
}

void hdmi_set_mode(uint16_t width, uint16_t height, float hz)
{
    VideoMode mode;
    
    hdmi_find_mode(width, height, hz, &mode);

    double mhz = video_mode_pixels(&mode) * hz;
    mhz /= 1000000.0;

    vio_cmd(VIO_SET_OVERRIDE, 1);
    vio_cmd(VIO_SET_CFG, 0);
    vio_cmd_cont(VIO_SET_MODE);
    vio_w16(mode.hact);
    vio_w16(mode.hfp);
    vio_w16(mode.hs);
    vio_w16(mode.hbp);

    vio_w16(mode.vact);
    vio_w16(mode.vfp);
    vio_w16(mode.vs);
    vio_w16(mode.vbp);
    
    vio_w16(1); // low latency
    
    vio_disable();

    setPLL(mhz);

    vio_cmd(VIO_SET_CFG, 1);
}

static void crtc_write_pll(uint16_t address, uint32_t data)
{
    while (crtc->pll_io != 0) {};

    crtc->pll_data = data;
    crtc->pll_address = address;
    crtc->pll_io = 0xffff;
}

void crt_set_mode(const VideoMode *mode)
{
	double Fpix;
	double fvco, ko;
	uint32_t m, c;

	//printf("Calculate PLL for %.4f MHz:\n", Fout);

    float mhz = mode->mhz * 4;
	if (!findPLLpar(mhz, &c, &m, &ko))
	{
		c = 1;
		while ((mhz*c) < 400) c++;

		fvco = mhz*c;
		m = (uint32_t)(fvco / 50);
		ko = ((fvco / 50) - m);

		//Make sure K is in allowed range.
		if (ko <= 0.05f)
		{
			ko = 0;
		}
		else if (ko >= 0.95f)
		{
			m++;
			ko = 0;
		}
	}

	uint32_t k = ko ? (uint32_t)(ko * 4294967296) : 1;

	fvco = ko + m;
	fvco *= 50.f;
	Fpix = fvco / c;

	//printf("Fvco=%f, C=%d, M=%d, K=%f(%u) -> Fpix=%f\n", fvco, c, m, ko, k, Fpix);

    crtc_write_pll(0, 0);
    crtc_write_pll(4, getPLLdiv(m));
    crtc_write_pll(3, 0x10000);
    crtc_write_pll(5, getPLLdiv(c));
    crtc_write_pll(9, 2);
    crtc_write_pll(8, 7);
    crtc_write_pll(7, k);

    crtc_write_pll(2, 0); // reconfigure

    crtc->ce_numer = 1;
    crtc->ce_denom = 4;

    crtc->hact = mode->hact;
    crtc->hfp = mode->hfp;
    crtc->hs = mode->hs;
    crtc->hbp = mode->hbp;

    crtc->vact = mode->vact;
    crtc->vfp = mode->vfp;
    crtc->vs = mode->vs;
    crtc->vbp = mode->vbp;
}


static const int CELL_GRAN_RND = 4;

static int determine_vsync(int w, int h)
{
    const int arx[] =   {4, 16, 16, 5, 15};
    const int ary[] =   {3,  9, 10, 4, 9 };
	const int vsync[] = {4,  5,  6, 7, 7 };

    for (int ar = 0; ar < 5; ar++)
    {
        int w_calc = ((h * arx[ar]) / (ary[ar] * CELL_GRAN_RND)) * CELL_GRAN_RND;
        if (w_calc == w)
        {
            return vsync[ar];
        }
    }

    return 10;
}

static void calculate_cvt(int h_pixels, int v_lines, float refresh_rate, bool reduced_blanking, VideoMode *vmode)
{
	// Based on xfree86 cvt.c and https://tomverbeure.github.io/video_timings_calculator

	const int MIN_V_BPORCH = 6;
	const int V_FRONT_PORCH = 3;

	const int h_pixels_rnd = (h_pixels / CELL_GRAN_RND) * CELL_GRAN_RND;
	const int v_sync = determine_vsync(h_pixels_rnd, v_lines);

	int v_back_porch;
	int h_blank, h_sync, h_back_porch, h_front_porch;
	int total_pixels;
	float pixel_freq;

	if (reduced_blanking)
	{
		const int RB_V_FPORCH = 3;
		const float RB_MIN_V_BLANK = 460.0f;

		float h_period_est = ((1000000.0f / refresh_rate) - RB_MIN_V_BLANK) / (float)v_lines;
		h_blank = 160;

		int vbi_lines = (int)(RB_MIN_V_BLANK / h_period_est) + 1;

		int rb_min_vbi = RB_V_FPORCH + v_sync + MIN_V_BPORCH;
		int act_vbi_lines = (vbi_lines < rb_min_vbi) ? rb_min_vbi : vbi_lines;

		int total_v_lines = act_vbi_lines + v_lines;

		total_pixels = h_blank + h_pixels_rnd;

		pixel_freq = refresh_rate * (float)(total_v_lines * total_pixels) / 1000000.0f;

		v_back_porch = act_vbi_lines - V_FRONT_PORCH - v_sync;

		h_sync = 32;
		h_back_porch = 80;
		h_front_porch = h_blank - h_sync - h_back_porch;
	}
	else
	{
		const float MIN_VSYNC_BP = 550.0f;
		const float C_PRIME = 30.0f;
		const float M_PRIME = 300.0f;
		const float H_SYNC_PER = 0.08f;

		const float h_period_est = ((1.0f / refresh_rate) - MIN_VSYNC_BP / 1000000.0f) / (float)(v_lines + V_FRONT_PORCH) * 1000000.0f;

		int v_sync_bp = (int)(MIN_VSYNC_BP / h_period_est) + 1;
		if (v_sync_bp < (v_sync + MIN_V_BPORCH))
		{
			v_sync_bp = v_sync + MIN_V_BPORCH;
		}

		v_back_porch = v_sync_bp - v_sync;

		float ideal_duty_cycle = C_PRIME - (M_PRIME * h_period_est / 1000.0f);

		if (ideal_duty_cycle < 20)
		{
			h_blank = (h_pixels_rnd / 4 / (2 * CELL_GRAN_RND)) * (2 * CELL_GRAN_RND);
		}
		else
		{
			h_blank = (int)((float)h_pixels_rnd * ideal_duty_cycle / (100.0f - ideal_duty_cycle) / (2 * CELL_GRAN_RND)) * (2 * CELL_GRAN_RND);
		}

		total_pixels = h_pixels_rnd + h_blank;

		h_sync = (int)(H_SYNC_PER * (float)total_pixels / CELL_GRAN_RND) * CELL_GRAN_RND;
		h_back_porch = h_blank / 2;
		h_front_porch = h_blank - h_sync - h_back_porch;

		pixel_freq = total_pixels / h_period_est;
	}

	vmode->hact = h_pixels_rnd;
	vmode->hfp = h_front_porch;
	vmode->hs = h_sync;
	vmode->hbp = h_back_porch;
	vmode->vact = v_lines;
	vmode->vfp = V_FRONT_PORCH - 1;
	vmode->vs = v_sync;
	vmode->vbp = v_back_porch + 1;
	vmode->mhz = pixel_freq;
}
