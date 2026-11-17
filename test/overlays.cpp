/* Canais de overlay: a fonte aparece por cima do programa e sai de novo. */
#include "../src/overlay-channels.hpp"
#include <obs-module.h>
#include <graphics/vec4.h>
#include <util/platform.h>
#include <atomic>
#include <cstdio>
#include <string>

OBS_DECLARE_MODULE()

#define W 640
#define H 360

struct ColorData {
	float v;
};
static const char *ts_name(void *) { return "solid"; }
static void *ts_create(obs_data_t *s, obs_source_t *) { return new ColorData{(float)obs_data_get_double(s, "v")}; }
static void ts_destroy(void *d) { delete (ColorData *)d; }
static uint32_t ts_w(void *) { return W; }
static uint32_t ts_h(void *) { return H; }
static void ts_render(void *d, gs_effect_t *)
{
	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	struct vec4 c;
	float v = ((ColorData *)d)->v;
	vec4_set(&c, v, v, v, 1.0f);
	gs_effect_set_vec4(gs_effect_get_param_by_name(solid, "color"), &c);
	while (gs_effect_loop(solid, "Solid"))
		gs_draw_sprite(nullptr, 0, W, H / 2); /* metade de cima */
}
static std::atomic<int> top{-1}, bottom{-1};
static void outCb(void *, struct video_data *f)
{
	top = f->data[0][(H / 4) * f->linesize[0] + W / 2];
	bottom = f->data[0][(3 * H / 4) * f->linesize[0] + W / 2];
}
static void sample(const char *label, int n)
{
	std::string line = label;
	for (int i = 0; i < n; i++) {
		os_sleep_ms(60);
		line += " " + std::to_string(top.load()) + "/" + std::to_string(bottom.load());
	}
	printf("%s\n", line.c_str());
}
int main()
{
	obs_startup("en-US", nullptr, nullptr);
	struct obs_video_info ovi = {};
	ovi.graphics_module = "libobs-opengl";
	ovi.fps_num = 30; ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = W; ovi.base_height = ovi.output_height = H;
	ovi.output_format = VIDEO_FORMAT_NV12; ovi.gpu_conversion = true;
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS) return 1;
	obs_module_t *m = nullptr;
	if (obs_open_module(&m, "/usr/lib/x86_64-linux-gnu/obs-plugins/obs-transitions.so", "/usr/share/obs/obs-plugins/obs-transitions") == MODULE_SUCCESS)
		obs_init_module(m);

	struct obs_source_info ti = {};
	ti.id = "solid_test"; ti.type = OBS_SOURCE_TYPE_INPUT;
	ti.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
	ti.get_name = ts_name; ti.create = ts_create; ti.destroy = ts_destroy;
	ti.get_width = ts_w; ti.get_height = ts_h; ti.video_render = ts_render;
	obs_register_source(&ti);

	/* programa: cena com a metade de cima cinza escuro; overlay: metade de cima branca */
	obs_data_t *a = obs_data_create(); obs_data_set_double(a, "v", 0.2);
	obs_source_t *prog = obs_source_create("solid_test", "programa", a, nullptr);
	obs_data_release(a);
	obs_data_t *b = obs_data_create(); obs_data_set_double(b, "v", 1.0);
	obs_source_t *ov = obs_source_create("solid_test", "GT placar", b, nullptr);
	obs_data_release(b);
	obs_set_output_source(0, prog);
	struct video_scale_info conv = {VIDEO_FORMAT_NV12, W, H, VIDEO_RANGE_FULL, VIDEO_CS_709};
	obs_add_raw_video_callback(&conv, outCb, nullptr);
	os_sleep_ms(400);

	overlaysCreate();
	OverlaySlot s; s.source = "GT placar"; s.transition = ""; s.durationMs = 300;
	overlays().setSlot(0, s);
	sample("so programa (topo/baixo):", 3);
	overlays().toggle(0);
	sample("overlay 1 ligado (corte):", 3);
	overlays().toggle(0);
	sample("overlay 1 desligado:", 3);

	s.transition = "*fade"; s.durationMs = 600;
	overlays().setSlot(1, s);
	overlays().toggle(1);
	sample("overlay 2 fade entrando:", 12);
	overlays().toggle(1);
	sample("overlay 2 fade saindo:", 12);
	printf("estado: 1=%d 2=%d\n", overlays().isOn(0), overlays().isOn(1));
	overlays().toggle(1);
	os_sleep_ms(700);
	overlays().releaseAll();
	sample("releaseAll:", 3);

	overlaysDestroy();
	obs_remove_raw_video_callback(outCb, nullptr);
	obs_set_output_source(0, nullptr);
	obs_source_release(prog);
	obs_source_release(ov);
	obs_shutdown();
	printf("fim ok\n");
	return 0;
}
