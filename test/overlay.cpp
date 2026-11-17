/* Testa OverlayTransition com transições nativas do OBS: a transição roda por
 * cima do programa sem trocar de cena. */
#include "../src/overlay-transition.hpp"
#include "../src/common.hpp"
#include <graphics/vec4.h>
#include <util/platform.h>
#include <atomic>
#include <cstdio>
#include <string>

OBS_DECLARE_MODULE()
#define W 640
#define H 360

static const char *ts_name(void *) { return "grey"; }
static void *ts_create(obs_data_t *, obs_source_t *) { return (void *)1; }
static void ts_destroy(void *) {}
static uint32_t ts_w(void *) { return W; }
static uint32_t ts_h(void *) { return H; }
static void ts_render(void *, gs_effect_t *)
{
	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	struct vec4 c;
	vec4_set(&c, 0.4f, 0.4f, 0.4f, 1.0f);
	gs_effect_set_vec4(gs_effect_get_param_by_name(solid, "color"), &c);
	while (gs_effect_loop(solid, "Solid"))
		gs_draw_sprite(nullptr, 0, W, H);
}
static std::atomic<int> outLuma{-1};
static void outCb(void *, struct video_data *f)
{
	outLuma = f->data[0][(H / 2) * f->linesize[0] + W / 2];
}
static void loadModule(const char *name)
{
	std::string bin = std::string("/usr/lib/x86_64-linux-gnu/obs-plugins/") + name + ".so";
	std::string data = std::string("/usr/share/obs/obs-plugins/") + name;
	obs_module_t *m = nullptr;
	if (obs_open_module(&m, bin.c_str(), data.c_str()) == MODULE_SUCCESS)
		obs_init_module(m);
	else
		printf("modulo %s nao carregou\n", name);
}
static void run(OverlayTransition &ov, const char *label, int dur, int samples)
{
	std::string line = label;
	bool ok = ov.start(dur);
	line += ok ? " [start ok]" : " [start FALHOU]";
	for (int i = 0; i < samples; i++) {
		os_sleep_ms(100);
		line += " " + std::to_string(outLuma.load()) + (ov.finished() ? "f" : "");
	}
	ov.stop();
	os_sleep_ms(200);
	line += " | depois: " + std::to_string(outLuma.load());
	printf("%s\n", line.c_str());
}

int main()
{
	obs_startup("en-US", nullptr, nullptr);
	struct obs_video_info ovi = {};
	ovi.graphics_module = "libobs-opengl";
	ovi.fps_num = 30;
	ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = W;
	ovi.base_height = ovi.output_height = H;
	ovi.output_format = VIDEO_FORMAT_NV12;
	ovi.gpu_conversion = true;
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS)
		return 1;
	struct obs_audio_info oai = {48000, SPEAKERS_STEREO};
	obs_reset_audio(&oai);
	loadModule("obs-transitions");
	loadModule("obs-ffmpeg");

	struct obs_source_info ti = {};
	ti.id = "grey_test";
	ti.type = OBS_SOURCE_TYPE_INPUT;
	ti.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
	ti.get_name = ts_name;
	ti.create = ts_create;
	ti.destroy = ts_destroy;
	ti.get_width = ts_w;
	ti.get_height = ts_h;
	ti.video_render = ts_render;
	obs_register_source(&ti);
	obs_source_t *grey = obs_source_create("grey_test", "grey", nullptr, nullptr);
	obs_set_output_source(0, grey);
	struct video_scale_info conv = {VIDEO_FORMAT_NV12, W, H, VIDEO_RANGE_FULL, VIDEO_CS_709};
	obs_add_raw_video_callback(&conv, outCb, nullptr);
	os_sleep_ms(500);
	printf("programa sem transicao: %d\n", outLuma.load());

	{
		OverlayTransition ov;
		obs_source_t *fade = obs_source_create("fade_transition", "Esmaecer", nullptr, nullptr);
		ov.prepare(fade);
		run(ov, "fade entre cenas vazias (deve manter o programa):", 500, 8);
		run(ov, "de novo (B->A):", 500, 8);
		obs_source_release(fade);

		obs_data_t *st = obs_data_create();
		obs_data_set_string(st, "path", "/tmp/stinger.webm");
		obs_data_set_int(st, "transition_point", 700);
		obs_source_t *sting = obs_source_create("obs_stinger_transition", "Stinger", st, nullptr);
		obs_data_release(st);
		if (sting) {
			ov.prepare(sting);
			os_sleep_ms(500);
			run(ov, "stinger branco opaco 1.5s (deve cobrir e sumir):", 1000, 22);
			run(ov, "stinger de novo:", 1000, 22);
			obs_source_release(sting);
		} else
			printf("stinger nativo indisponivel\n");
	}

	obs_remove_raw_video_callback(outCb, nullptr);
	obs_set_output_source(0, nullptr);
	obs_source_release(grey);
	obs_shutdown();
	printf("fim ok\n");
	return 0;
}
