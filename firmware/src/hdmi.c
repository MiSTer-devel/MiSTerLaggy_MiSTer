#include <stdint.h>

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

#define NUM_HDMI_MODES 14

VideoMode hdmi_modes[NUM_HDMI_MODES] =
{
    { 1280, 110,  40, 220,  720,  5,  5, 20,  74.25 }, //1  1280x720@60
	{ 1024,  24, 136, 160,  768,  3,  6, 29,  65,   }, //2  1024x768@60
	{  720,  16,  62,  60,  480,  9,  6, 30,  27    }, //3  720x480@60
	{  720,  12,  64,  68,  576,  5,  5, 39,  27    }, //4  720x576@50
	{ 1280,  48, 112, 248, 1024,  1,  3, 38, 108    }, //5  1280x1024@60
	{  800,  40, 128,  88,  600,  1,  4, 23,  40    }, //6  800x600@60
	{  640,  16,  96,  48,  480, 10,  2, 33,  25.175 }, //7  640x480@60
	{ 1280, 440,  40, 220,  720,  5,  5, 20,  74.25  }, //8  1280x720@50
	{ 1920,  88,  44, 148, 1080,  4,  5, 36, 148.5   }, //9  1920x1080@60
	{ 1920, 528,  44, 148, 1080,  4,  5, 36, 148.5   }, //10  1920x1080@50
	{ 1366,  70, 143, 213,  768,  3,  3, 24,  85.5   }, //11 1366x768@60
	{ 1024,  40, 104, 144,  600,  1,  3, 18,  48.96  }, //12 1024x600@60
	{ 1920,  48,  32,  80, 1440,  2,  4, 38, 185.203 }, //13 1920x1440@60
	{ 2048,  48,  32,  80, 1536,  2,  4, 38, 209.318 }, //14 2048x1536@60
};


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

static const VideoMode *hdmi_find_mode(uint16_t width, uint16_t height, float hz)
{
    float min_diff = 999999999999;
    int min_index = -1;
    for( int i = 0; i < NUM_HDMI_MODES; i++ )
    {
        const VideoMode *mode = &hdmi_modes[i];
        if (mode->hact != width || mode->vact != height) continue;

        int32_t h = mode->hact + mode->hfp + mode->hs + mode->hbp;
        int32_t v = mode->vact + mode->vfp + mode->vs + mode->vbp;
        float mhz = h * v * hz;
        mhz /= 1000000.0;

        float diff = mode->mhz - mhz;
        diff = diff < 0.0 ? -diff : diff;
        if (min_index == -1 || min_diff > diff)
        {
            min_index = i;
            min_diff = diff;
        }
    }

    if (min_index >= 0)
    {
        return &hdmi_modes[min_index];
    }

    return 0;
}

void hdmi_set_mode(uint16_t width, uint16_t height, float hz)
{
    const VideoMode *mode = hdmi_find_mode(width, height, hz);

    if (mode == 0) return;

    int32_t h = mode->hact + mode->hfp + mode->hs + mode->hbp;
    int32_t v = mode->vact + mode->vfp + mode->vs + mode->vbp;
    double mhz = h * v * hz;
    mhz /= 1000000.0;

    vio_cmd(VIO_SET_OVERRIDE, 1);
    vio_cmd(VIO_SET_CFG, 0);
    vio_cmd_cont(VIO_SET_MODE);
    vio_w16(mode->hact);
    vio_w16(mode->hfp);
    vio_w16(mode->hs);
    vio_w16(mode->hbp);

    vio_w16(mode->vact);
    vio_w16(mode->vfp);
    vio_w16(mode->vs);
    vio_w16(mode->vbp);
    
    vio_w16(1);
    
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
