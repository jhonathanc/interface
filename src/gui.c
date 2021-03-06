#include <stdio.h>
#include <string.h>

#include <zlib.h>

#include <packet.h>
#include <dma.h>

#include <font.h>
#include <graph.h>
#include <draw.h>
#include <gs_psm.h>
#include <image.h>

#include "gui.h"
#include "gzip.h"
#include "tar.h"

extern unsigned char skin_tgz[];
extern unsigned int  size_skin_tgz;

static float gui_screen_height = 512.0f;

static char bg = 0;
static char fg = 0;

static gui_vram_t vram;
static fsfont_t *gui_font = NULL;

// Sends image to vram
//void gui_send_image(image_t *image,int texture, int clut);

// Loads image from tar and sends to vram
//int gui_load_image(char *tar, int tar_size, char *file, int texaddr, int clutaddr);

// Loads font ini
//int gui_load_font_ini(char *tar, int tar_size);

// Allocates and initializes the vram map
//void gui_vram_init(void);

// Allocates gui font and loads ini from skin.tgz
//void gui_font_init(int height);

void gui_vram_init(void)
{

	graph_vram_clear();

	// Make room for framebuffers
	graph_vram_allocate(512,512,GS_PSM_16S,GRAPH_ALIGN_PAGE);
	graph_vram_allocate(512,512,GS_PSM_16S,GRAPH_ALIGN_PAGE);

	vram.bg = graph_vram_allocate(512,512,GS_PSM_32,GRAPH_ALIGN_BLOCK);
	vram.fg = graph_vram_allocate(512,512,GS_PSM_8,GRAPH_ALIGN_BLOCK);
	vram.fg_clut = graph_vram_allocate(16,16,GS_PSM_32,GRAPH_ALIGN_BLOCK);
	vram.misc = graph_vram_allocate(256,256,GS_PSM_32,GRAPH_ALIGN_BLOCK);
	vram.skin = graph_vram_allocate(512,128,GS_PSM_32,GRAPH_ALIGN_BLOCK);
	vram.font = graph_vram_allocate(1024,1024,GS_PSM_4,GRAPH_ALIGN_BLOCK);
	vram.font_clut = graph_vram_allocate(8,2,GS_PSM_32,GRAPH_ALIGN_BLOCK);

}

gui_vram_t gui_vram_get(void)
{
	return vram;
}

void gui_font_init(int height)
{

	gui_font = fontstudio_init(height);

	gui_font->charmap = NULL;
	gui_font->chardata = NULL;

}

void gui_init(int font_height)
{

	gui_vram_init();

	if (gui_font != NULL)
	{
		gui_font_free();
	}

	gui_font_init(font_height);

}

void gui_free()
{

	if (gui_font != NULL)
	{
		gui_font_free();
	}

}

fsfont_t *gui_font_get(void)
{
	return gui_font;
}

void gui_font_free(void)
{
	fontstudio_free(gui_font);
}

void gui_set_screen_height(float height)
{
	gui_screen_height = height;
}

float gui_get_screen_height(void)
{
	return gui_screen_height;
}

char gui_background_exists()
{
	return bg;
}

char gui_foreground_exists()
{
	return fg;
}

char *gui_skin_tgz(const char *path, int *gz_size)
{

	char *gz;

	if (path != NULL)
	{
		gz = gzip_load_file(path,gz_size);
	}
	else
	{
		gz = skin_tgz;
		*gz_size = size_skin_tgz;
	}

	return gz;

}

void gui_send_image(image_t *image,int texture, int clut)
{

	packet_t *packet;
	qword_t *q;

	// Shouldn't need more than 50
	packet = packet_init(50,PACKET_UCAB);

	q = packet->data;

	q = draw_texture_transfer(q,image->texture.data,image->texture.width,image->texture.height,image->texture.psm,texture,image->texture.width);

	if (image->texture.psm == GS_PSM_4 || image->texture.psm == GS_PSM_8)
	{
		q = draw_texture_transfer(q,image->palette.data,image->palette.width,image->palette.height,image->palette.psm,clut,64);
	}

	q = draw_texture_flush(q);

	dma_channel_send_chain_ucab(DMA_CHANNEL_GIF,packet->data, q - packet->data, 0);
	dma_wait_fast();

	packet_free(packet);

}

int gui_load_image(char *tar, int tar_size, char *file, int texaddr, int clutaddr)
{

	char *png;
	int png_size;

	image_t *image = NULL;

	if (get_file_from_tar(tar,tar_size,file, &png, &png_size) >= 0)
	{
		image = image_load_png_buffer(png);
	}
	else
	{
		return -1;
	}

	if (image != NULL)
	{

		gui_send_image(image,texaddr,clutaddr);

		image_free(image);

	}

	return 0;
}

int gui_load_font_ini(char *tar, int tar_size)
{

	char *ini;
	int ini_size;

	// for loading new fonts
	if (gui_font->charmap != NULL)
	{
		free(gui_font->charmap);
	}

	if (gui_font->chardata != NULL)
	{
		free(gui_font->chardata);
	}

	if (get_file_from_tar(tar,tar_size,"font.ini", &ini, &ini_size) >= 0)
	{
		fontstudio_parse_ini(gui_font,ini,1024,1024);
	}
	else
	{
		return -1;
	}

	return 0;

}

void gui_load_skin(char *path)
{

	char *tar;
	int tar_size;

	char *gz;
	int gz_size;

	gz = gui_skin_tgz(path,&gz_size);

	if (gz == NULL)
	{
		return;
	}

	tar_size = gzip_get_size(gz,gz_size);

	tar = malloc(tar_size);

	if (gzip_uncompress(gz,tar) != Z_OK)
	{
		free(tar);
		if (gz != (char*)skin_tgz)
		{
			free(gz);
		}
		return;
	}

	if (gui_load_image(tar,tar_size,"bg.png",vram.bg,0) < 0)
	{
		bg = 0;
	}
	else
	{
		bg = 1;
	}

	if (gui_load_image(tar,tar_size,"skin.png",vram.skin,0) < 0)
	{
		free(tar);
		if (gz != (char*)skin_tgz)
		{
			free(gz);
		}
		gui_load_skin(NULL);
	}

	if (gui_load_image(tar,tar_size,"fg.png",vram.fg,vram.fg_clut) < 0)
	{
		fg = 0;
	}
	else
	{
		fg = 1;
	}

	if (gui_load_image(tar,tar_size,"font.png",vram.font,vram.font_clut) < 0)
	{
		free(tar);
		if (gz != (char*)skin_tgz)
		{
			free(gz);
		}
		gui_load_skin(NULL);
	}

	if (gui_load_font_ini(tar,tar_size) < 0)
	{
		free(tar);
		if (gz != (char*)skin_tgz)
		{
			free(gz);
		}
		gui_load_skin(NULL);
	}

	free(tar);

	if (gz != (char*)skin_tgz)
	{
		free(gz);
	}

}

qword_t *gui_setup_texbuffer(qword_t *q, int type)
{

	texbuffer_t tex;
	clutbuffer_t clut;
	lod_t lod;

	lod.calculation = LOD_USE_K;
	lod.max_level = 0;
	lod.mag_filter = LOD_MAG_LINEAR;
	lod.min_filter = LOD_MIN_LINEAR;
	lod.l = 0;
	lod.k = 0;

	tex.info.components = TEXTURE_COMPONENTS_RGBA;
	tex.info.function = TEXTURE_FUNCTION_MODULATE;
	tex.info.height = draw_log2(512);
	tex.info.width = draw_log2(512);
	tex.width = 512;
	tex.psm = GS_PSM_32;

	clut.storage_mode = 0;
	clut.start = 0;
	clut.psm = 0;
	clut.load_method = CLUT_NO_LOAD;
	clut.address = 0;

	switch (type)
	{
		case FONT:
		{
			tex.info.height = draw_log2(1024);
			tex.info.width = draw_log2(1024);
			tex.width = 1024;
			tex.psm = GS_PSM_4;
			tex.address = vram.font;
			clut.storage_mode = CLUT_STORAGE_MODE1;
			clut.psm = GS_PSM_32;
			clut.load_method = CLUT_LOAD;
			clut.address = vram.font_clut;
			break;
		}
		case FOREGROUND:
		{
			tex.psm = GS_PSM_8;
			tex.address = vram.fg;
			clut.storage_mode = CLUT_STORAGE_MODE1;
			clut.psm = GS_PSM_32;
			clut.load_method = CLUT_LOAD;
			clut.address = vram.fg_clut;
			break;
		}
		case SKIN:
		{
			tex.info.height = draw_log2(128);
			tex.address = vram.skin;
			break;
		}
		case BACKGROUND:
		{
			tex.address = vram.bg;
			break;
		}
	}

	q = draw_texture_sampling(q,0,&lod);
	q = draw_texturebuffer(q,0,&tex,&clut);

	return q;

}

qword_t *gui_foreground(qword_t *q, unsigned char alpha)
{

	texrect_t rect;

	rect.v0.x = 0.0f;
	rect.v0.y = 0.0f;
	rect.v0.z = 10;

	rect.v1.x = rect.v0.x + 511.0f;
	rect.v1.y = rect.v0.y + 511.0f;
	rect.v1.z = rect.v0.z;

	rect.t0.u = 0.5f;
	rect.t0.v = 0.5f;

	rect.t1.u = 510.5f;
	rect.t1.v = 510.5f;

	rect.color.r = 0x80;
	rect.color.g = 0x80;
	rect.color.b = 0x80;
	rect.color.a = alpha;
	rect.color.q = 1.0f;

	q = draw_rect_textured_strips(q,0,&rect);

	return q;

}

qword_t *gui_background(qword_t *q)
{

	texrect_t rect;

	rect.v0.x = 0.0f;
	rect.v0.y = 0.0f;
	rect.v0.z = 1;

	rect.v1.x = rect.v0.x + 511.0f;
	rect.v1.y = rect.v0.y + 511.0f;
	rect.v1.z = rect.v0.z;

	rect.t0.u = 0.5f;
	rect.t0.v = 0.5f;

	rect.t1.u = 510.5f;
	rect.t1.v = 510.5f;

	rect.color.r = 0x80;
	rect.color.g = 0x80;
	rect.color.b = 0x80;
	rect.color.a = 0x80;
	rect.color.q = 1.0f;

	q = draw_rect_textured_strips(q,0,&rect);

	return q;

}

qword_t *gui_header(qword_t *q, float width, unsigned char alpha)
{

	texrect_t rect;

	rect.v0.x = 0.0f;
	rect.v0.y = 0.0f;
	rect.v0.z = 2;

	rect.v1.x = rect.v0.x + width;
	rect.v1.y = rect.v0.y + 49.0f;
	rect.v1.z = rect.v0.z;

	rect.t0.u = BANNER_U0;
	rect.t0.v = BANNER_V0;

	rect.t1.u = BANNER_U1;
	rect.t1.v = BANNER_V1;

	rect.color.r = 0x80;
	rect.color.g = 0x80;
	rect.color.b = 0x80;
	rect.color.a = alpha;
	rect.color.q = 1.0f;

	q = draw_rect_textured_strips(q,0,&rect);

	return q;

}

qword_t *gui_footer(qword_t *q, float y, float width, unsigned char alpha)
{

	texrect_t rect;

	rect.v0.x = 0.0f;
	rect.v0.y = y;
	rect.v0.z = 2;

	rect.v1.x = rect.v0.x + width;
	rect.v1.y = rect.v0.y + 49.0f;
	rect.v1.z = rect.v0.z;

	rect.t0.u = BANNER_U1;
	rect.t0.v = BANNER_V1;

	rect.t1.u = BANNER_U0;
	rect.t1.v = BANNER_V0;

	rect.color.r = 0x80;
	rect.color.g = 0x80;
	rect.color.b = 0x80;
	rect.color.a = alpha;
	rect.color.q = 1.0f;

	q = draw_rect_textured_strips(q,0,&rect);


	return q;

}

qword_t *gui_logo(qword_t *q, float x, float y, unsigned char alpha)
{

	texrect_t logo;

	logo.v0.x = x;
	logo.v0.y = y;
	logo.v0.z = 3;

	logo.v1.x = x + 288.0f;
	logo.v1.y = y + 46.0f;
	logo.v1.z = logo.v0.z;

	logo.color.r = 0x80;
	logo.color.g = 0x80;
	logo.color.b = 0x80;
	logo.color.a = alpha;
	logo.color.q = 1.0f;

	logo.t0.u = LOGO_U0;
	logo.t0.v = LOGO_V0;

	logo.t1.u = LOGO_U1;
	logo.t1.v = LOGO_V1;

	q = draw_rect_textured_strips(q,0,&logo);

	return q;

}

qword_t *gui_box(qword_t *q, float x, float y, int width, int height, int active)
{

	//int i;
	//texrect_t frame;
	rect_t bg;

	// background
	bg.v0.x = x + 1.0f;
	bg.v0.y = y + 1.0f;
	bg.v0.z = 2;

	bg.v1.x = bg.v0.x + (float)width - 1.0f;
	bg.v1.y = bg.v0.y + (float)height - 1.0f;
	bg.v1.z = bg.v0.z;

	bg.color.r = 0x00;
	bg.color.g = 0x00;
	bg.color.b = 0x00;
	bg.color.a = 0x40;
	bg.color.q = 1.0f;

	if (active)
	{
		draw_enable_blending();
	}
	else
	{
		draw_disable_blending();
	}

	q = draw_round_rect_filled(q,0,&bg);

	//top left corner
/*
	frame.v0.x = x;
	frame.v0.y = y;
	frame.v0.z = 3;

	frame.v1.x = frame.v0.x + 32.0f;
	frame.v1.y = frame.v0.y + 32.0f;
	frame.v1.z = frame.v0.z;

	if (active)
	{
		frame.color.r = 0xFF;
		frame.color.g = 0xFF;
		frame.color.b = 0xFF;
	}
	else
	{
		frame.color.r = 0x80;
		frame.color.g = 0x80;
		frame.color.b = 0x80;
	}
	frame.color.a = 0x80;
	frame.color.q = 1.0f;

	frame.t0.u = BOX_START_U0;
	frame.t0.v = BOX_V0;

	frame.t1.u = BOX_START_U0 + 31.0f;
	frame.t1.v = BOX_V0 + 31.0f;

	q = draw_rect_textured(q,0,&frame);

	frame.t0.u += 16.0f;
	frame.t1.u += 16.0f;

	//top
	for (i = 0; i < (width - 64); i += 32)
	{
		frame.v0.x += 32.0f;
		frame.v1.x += 32.0f;
		q = draw_rect_textured(q,0,&frame);
	}

	//top right corner
	frame.t0.u += 48.0f;
	frame.t1.u += 48.0f;

	frame.v0.x += 32.0f;
	frame.v1.x += 32.0f;

	q = draw_rect_textured(q,0,&frame);

	// right side
	frame.t0.v += 16.0f;
	frame.t1.v += 16.0f;

	for (i = 0; i < (height - 64); i += 32)
	{
		frame.v0.y += 32.0f;
		frame.v1.y += 32.0f;
		q = draw_rect_textured(q,0,&frame);
	}

	// bottom right corner
	frame.t0.v += 16.0f;
	frame.t1.v += 16.0f;

	frame.v0.y += 32.0f;
	frame.v1.y += 32.0f;

	q = draw_rect_textured(q,0,&frame);

	//left side
	frame.v0.x = x;
	frame.v0.y = y;

	frame.v1.x = frame.v0.x + 32.0f;
	frame.v1.y = frame.v0.y + 32.0f;

	frame.t0.u = BOX_START_U0;
	frame.t0.v = BOX_V0 + 16.0f;

	frame.t1.u = BOX_START_U0 + 31.0f;
	frame.t1.v = BOX_V0 + 31.0f + 16.0f;

	for (i = 0; i < (height - 64); i += 32)
	{
		frame.v0.y += 32.0f;
		frame.v1.y += 32.0f;
		q = draw_rect_textured(q,0,&frame);
	}

	// bottom left corner
	frame.t0.v += 16.0f;
	frame.t1.v += 16.0f;

	frame.v0.y += 32.0f;
	frame.v1.y += 32.0f;

	q = draw_rect_textured(q,0,&frame);

	// bottom
	frame.t0.u += 16.0f;
	frame.t1.u += 16.0f;

	for (i = 0; i < (width - 64); i += 32)
	{
		frame.v0.x += 32.0f;
		frame.v1.x += 32.0f;
		q = draw_rect_textured(q,0,&frame);
	}
*/
	return q;
}

qword_t *gui_button(qword_t *q, float x, float y, float button, int active)
{

	texrect_t rect;

	rect.v0.x = x;
	rect.v0.y = y;
	rect.v0.z = 3;

	rect.v1.x = rect.v0.x + 127.0f;
	rect.v1.y = rect.v0.y + 31.0f;
	rect.v1.z = rect.v0.z;

	rect.t0.u = BUTTON_U0;
	rect.t0.v = BUTTON_V0 + (button * BUTTON_HEIGHT);

	rect.t1.u = BUTTON_U1;
	rect.t1.v = rect.t0.v + 31.0f;

	if (active)
	{
		rect.color.r = 0xFF;
		rect.color.g = 0xFF;
		rect.color.b = 0xFF;
	}
	else
	{
		rect.color.r = 0x80;
		rect.color.g = 0x80;
		rect.color.b = 0x80;
	}

	rect.color.a = 0x80;
	rect.color.q = 1.0f;

	q = draw_rect_textured(q,0,&rect);

	return q;

}

qword_t *gui_button_string(qword_t *q, float x, float y, char *str, fsfont_t *font, unsigned char active)
{

	rect_t rect;
	vertex_t v0;
	color_t c0;

	rect.v0.x = x;
	rect.v0.y = y;
	rect.v0.z = 10;

	rect.v1.x = rect.v0.x + 127.0f;
	rect.v1.y = rect.v0.y + 31.0f;
	rect.v1.z = rect.v0.z;

	v0 = rect.v0;
	v0.x = rect.v0.x + ((rect.v1.x - rect.v0.x)/2.0f);
	v0.z = 11;

	rect.color.r = 0x00;
	rect.color.g = 0x00;
	rect.color.b = 0x00;
	rect.color.a = 0x80;
	rect.color.q = 1.0f;


	q = draw_round_rect_filled(q,0,&rect);

	if (active)
	{
		rect.color.r = 0x80;
		rect.color.g = 0x80;
		
	}
	else
	{
		rect.color.r = 0x80;
		rect.color.g = 0x80;
		rect.color.b = 0x80;
	}


	rect.color.a = 0x80;
	rect.color.q = 1.0f;
	rect.v0.x -= 1.0f;
	rect.v1.x += 1.0f;
	rect.v0.z = 11;

	q = draw_round_rect_outline(q,0,&rect);

	rect.v0.x -= 1.0f;
	rect.v1.x += 1.0f;
	rect.v0.z++;

	q = draw_round_rect_outline(q,0,&rect);
	c0 = rect.color;

	// Adjust the centering point
	v0.x -= 4;
	v0.y += 4;

	q = fontstudio_print_string(q,0,str,CENTER_ALIGN,&v0,&c0,font);

	return q;

}

qword_t *gui_icon(qword_t *q, float x, float y, float type, color_t *color)
{

	texrect_t rect;

	rect.v0.x = x;
	rect.v0.y = y;
	rect.v0.z = 4;

	rect.v1.x = rect.v0.x + 32.0f;
	rect.v1.y = rect.v0.y + 32.0f;
	rect.v1.z = rect.v0.z;

	rect.t0.u = ICON_U0 + (type * ICON_WIDTH);
	rect.t0.v = ICON_V0;

	rect.t1.u = rect.t0.u + 31.0f;
	rect.t1.v = ICON_V1;

	rect.color = *color;

	q = draw_rect_textured(q,0,&rect);

	return q;

}

qword_t *gui_basic_layout(qword_t *q, unsigned char alpha)
{

	q = gui_header(q,512.0f,alpha);
	q = gui_footer(q,gui_screen_height-50.0f,512.0f,alpha);
	q = gui_logo(q,256.0f,16.0f,alpha);

	return q;
}
