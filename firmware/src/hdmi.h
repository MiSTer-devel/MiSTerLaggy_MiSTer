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
} VideoMode;

static inline uint32_t video_mode_pixels(const VideoMode *m)
{
    return ( m->hact + m->hbp + m->hfp + m->hs ) * ( m->vact + m->vbp + m->vfp + m->vs );
}

void hdmi_set_mode(uint16_t width, uint16_t height, float hz);

void crt_set_mode(const VideoMode *mode);


#endif // HDMI_H