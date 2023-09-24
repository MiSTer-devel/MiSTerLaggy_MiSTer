#if !defined(HDMI_H)
#define HDMI_H 1

typedef struct
{
	uint16_t hact;
	uint16_t hfp;
	uint16_t hs;
	uint16_t hbp;
	uint16_t vact;
	uint16_t vfp;
	uint16_t vs;
	uint16_t vbp;

    float mhz;
} HDMIMode;

void hdmi_set_mode(HDMIMode *mode, int vsync_adjust, float hz);
void hdmi_clear_mode();

#endif // HDMI_H