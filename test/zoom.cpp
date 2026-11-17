/* Zoom do replay: metade esquerda escura, direita clara (rampa de 5 níveis por quadro) — derivado do teste ponta a ponta sem interface: libobs headless + fonte de teste (rampa
 * de cinza que sobe 1 nível por quadro) -> gravação -> canal de playback ->
 * fonte de replay -> leitura do que realmente aparece na saída. */
#include "../src/engine.hpp"
#include <graphics/vec4.h>
#include <util/platform.h>
#include <atomic>
#include <cmath>
#include <cstdio>

OBS_DECLARE_MODULE()
void registerReplaySource();

static std::atomic<int> tickCount{0};
#define W 640
#define H 360

static const char *ts_name(void *) { return "ramp"; }
static void *ts_create(obs_data_t *, obs_source_t *) { return (void *)1; }
static void ts_destroy(void *) {}
static uint32_t ts_w(void *) { return W; }
static uint32_t ts_h(void *) { return H; }
static void ts_tick(void *, float) { tickCount++; }
static void ts_render(void *, gs_effect_t *)
{
	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
	for (int half = 0; half < 2; half++) {
		float v = half ? 0.85f : 0.15f;
		struct vec4 c;
		vec4_set(&c, v, v, v, 1.0f);
		gs_effect_set_vec4(color, &c);
		gs_matrix_push();
		gs_matrix_translate3f(half ? W / 2.0f : 0.0f, 0.0f, 0.0f);
		while (gs_effect_loop(solid, "Solid"))
			gs_draw_sprite(nullptr, 0, W / 2, H);
		gs_matrix_pop();
	}
}

static std::atomic<int> outLuma{-1};
static std::atomic<int> outFrames{0};
static std::atomic<int> lumaL{-1}, lumaR{-1};
static void outCb(void *, struct video_data *f)
{
	outLuma = f->data[0][(H / 2) * f->linesize[0] + W / 2];
	lumaL = f->data[0][(H / 2) * f->linesize[0] + 4];
	lumaR = f->data[0][(H / 2) * f->linesize[0] + W - 5];
	outFrames++;
}

static void sample(const char *label, int n, int ms = 33)
{
	printf("%-28s", label);
	for (int i = 0; i < n; i++) {
		os_sleep_ms(ms);
		printf(" %3d", outLuma.load());
	}
	ChannelState st = engine().channel(0).state();
	printf("   | pos=%.1f live=%lld\n", st.pos, (long long)st.live);
	fflush(stdout);
}

int main()
{
	if (!obs_startup("en-US", nullptr, nullptr))
		return 1;
	struct obs_video_info ovi = {};
	ovi.graphics_module = "libobs-opengl";
	ovi.fps_num = 30;
	ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = W;
	ovi.base_height = ovi.output_height = H;
	ovi.output_format = VIDEO_FORMAT_NV12;
	ovi.colorspace = VIDEO_CS_709;
	ovi.range = VIDEO_RANGE_PARTIAL;
	ovi.scale_type = OBS_SCALE_BILINEAR;
	ovi.gpu_conversion = true;
	int r = obs_reset_video(&ovi);
	if (r != OBS_VIDEO_SUCCESS) {
		printf("obs_reset_video falhou: %d\n", r);
		return 1;
	}
	struct obs_audio_info oai = {48000, SPEAKERS_STEREO};
	obs_reset_audio(&oai);

	struct obs_source_info ti = {};
	ti.id = "ramp_test";
	ti.type = OBS_SOURCE_TYPE_INPUT;
	ti.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
	ti.get_name = ts_name;
	ti.create = ts_create;
	ti.destroy = ts_destroy;
	ti.get_width = ts_w;
	ti.get_height = ts_h;
	ti.video_tick = ts_tick;
	ti.video_render = ts_render;
	obs_register_source(&ti);

	engineCreate();
	registerReplaySource();
	registerReplayCaptureOutput();

	obs_source_t *cam = obs_source_create("ramp_test", "testcam", nullptr, nullptr);
	obs_source_t *cam2 = obs_source_create("ramp_test", "testcam2", nullptr, nullptr);
	obs_source_t *out = obs_source_create("instant_replay_output", "replay A", nullptr, nullptr);

	obs_set_output_source(0, out);
	struct video_scale_info conv = {VIDEO_FORMAT_NV12, W, H, VIDEO_RANGE_FULL, VIDEO_CS_709};
	obs_add_raw_video_callback(&conv, outCb, nullptr);

	ReplayConfig cfg;
	cfg.cameras[0] = "testcam";
	cfg.cameras[3] = "testcam2";
	cfg.outputDir = "/tmp/replaytest";
	cfg.rec.encoder = getenv("REPLAY_ENC") ? getenv("REPLAY_ENC") : "mjpeg";
	engine().setConfig(cfg);

	std::string err;
	if (!engine().startRecording(err)) {
		printf("startRecording falhou: %s\n", err.c_str());
		return 1;
	}
	printf("sessao: %s\n", engine().sessionDir().c_str());

	os_sleep_ms(2500);
	engine().autoEvent(1);
	os_sleep_ms(500);
	auto show = [](const char *label) {
		os_sleep_ms(300);
		PlaybackChannel::Zoom z = engine().channel(0).zoom();
		printf("%-34s esq=%3d centro=%3d dir=%3d   (zoom %.2f x=%.2f y=%.2f)\n", label, lumaL.load(), outLuma.load(),
		       lumaR.load(), z.z, z.x, z.y);
		fflush(stdout);
	};
	show("sem zoom");
	engine().channel(0).zoomStep(2.0f, 0.25f, 0.0f);
	show("2x, para a direita");
	engine().channel(0).zoomStep(1.0f, -0.6f, 0.0f);
	show("2x, para a esquerda (passa do limite)");
	engine().channel(0).setZoomMove(0, 1);
	os_sleep_ms(1500);
	engine().channel(0).setZoomMove(0, 0);
	show("segurou -> por 1,5 s");
	engine().channel(0).setZoomMove(2, 1);
	os_sleep_ms(4000);
	engine().channel(0).setZoomMove(2, 0);
	show("segurou zoom+ por 4 s (máx 5x)");
	engine().channel(0).resetZoom();
	show("reset");
	_Exit(0);
}
