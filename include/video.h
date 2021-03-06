#ifndef __VIDEO_H__
#define __VIDEO_H__

#ifdef __cplusplus
extern "C" {
#endif

	// Initializes the packets
	void video_packets_init(void);

	// Frees the packets
	void video_packets_free(void);

	// Initializes the GIF dma channel
	void video_init_dmac(void);

	// Initializes framebuffers in vram
	void video_init_framebuffer(int width, int height);

	// Initializes screen and drawing environment and vsync handler (can be reused)
	void video_init_screen(int x, int y, int width, int height, int interlace, int mode);

	// Initializes the drawing environment
	void video_init_draw_env(int width, int height);

	// Initializes the texture and clut buffers in vram (call once after framebuffer init)
	void video_init_texbuffer(int width, int height, int tex_psm, int clut_psm);

	// Enable vsync handler
	void video_enable_vsync_handler();

	// Disable vsync handler
	void video_disable_vsync_handler(void);

	// Sets up the send packet
	void video_send_packet(int width, int height, void *texture,void *clut);

	// Sets up the draw packet
	void video_draw_packet(int width, int height, int tex_psm, int clut_psm);

	// Sends the texture and clut to vram
	void video_send_texture();

	// Draws the texture
	void video_draw_texture();

	// Check for vsync event and flip buffers after vsync handler
	void video_sync_flip();

	// Waits for vsync event and flips buffers without vsync handler
	void video_sync_wait();

#ifdef __cplusplus
};
#endif

#endif /*__VIDEO_H__*/
