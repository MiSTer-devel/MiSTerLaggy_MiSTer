#include <stdint.h>

#include "hdmi.h"

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

void hdmi_set_mode(HDMIMode *mode, int vsync_adjust, float hz)
{
    double mhz = mode->mhz;

    if (vsync_adjust != 0)
    {
        int32_t h = mode->hact + mode->hfp + mode->hs + mode->hbp;
        int32_t v = mode->vact + mode->vfp + mode->vs + mode->vbp;
        mhz = h * v * hz;
        mhz /= 1000000.0;
    }

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
    
    vio_w16(vsync_adjust == 2 ? 1 : 0);
    
    vio_disable();

    setPLL(mhz);

    vio_cmd(VIO_SET_CFG, 1);
}

void hdmi_clear_mode()
{
    vio_cmd(VIO_SET_OVERRIDE, 0);
}