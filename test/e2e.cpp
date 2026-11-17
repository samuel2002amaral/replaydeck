/* Teste ponta a ponta sem interface: libobs headless + fonte de teste (rampa
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
	float v = (float)(tickCount.load() % 200) / 255.0f + 0.1f;
	struct vec4 c;
	vec4_set(&c, v, v, v, 1.0f);
	gs_effect_set_vec4(color, &c);
	while (gs_effect_loop(solid, "Solid"))
		gs_draw_sprite(nullptr, 0, W, H);
}

static std::atomic<int> outLuma{-1};
static std::atomic<int> outFrames{0};
static void outCb(void *, struct video_data *f)
{
	outLuma = f->data[0][(H / 2) * f->linesize[0] + W / 2];
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

	os_sleep_ms(4000);
	sample("ao vivo (sobe 1/quadro)", 10);

	engine().autoEvent(3);
	os_sleep_ms(200);
	sample("evento 3s: cue (parado)", 6);

	engine().setSpeedPercent(100);
	engine().play();
	sample("play 100%", 10);
	engine().setSpeedPercent(50);
	sample("play 50%", 10);
	engine().setSpeedPercent(0);
	sample("play 0% (congela)", 5);
	engine().setSpeedPercent(100);
	engine().setReverse(true);
	sample("reverso 100%", 10);
	engine().setReverse(false);
	engine().pause();
	os_sleep_ms(100);
	sample("pausa", 3);
	engine().step(1);
	os_sleep_ms(100);
	sample("+1 quadro", 1);
	engine().step(1);
	os_sleep_ms(100);
	sample("+1 quadro", 1);
	engine().step(-1);
	os_sleep_ms(100);
	sample("-1 quadro", 1);
	engine().setAngle(3);
	os_sleep_ms(100);
	sample("angulo 4 (mesmo instante)", 1);
	engine().setAngle(5);
	os_sleep_ms(100);
	sample("angulo 6 (sem camera)", 1);
	engine().setAngle(0);
	engine().fastForward();
	sample("FF 2x", 8);
	engine().goLive();
	os_sleep_ms(200);
	sample("ao vivo de novo", 5);

	auto evs = engine().events(0);
	if (evs.empty()) {
		printf("SEM EVENTOS\n");
		return 1;
	}
	printf("eventos na lista 1: %zu (in=%lld out=%lld)\n", evs.size(), (long long)evs[0].in, (long long)evs[0].out);
	engine().playEvent(0, evs[0].id);
	os_sleep_ms(3500);
	ChannelState st = engine().channel(0).state();
	printf("apos tocar evento: pos=%.1f playing=%d (esperado pos=%lld, playing=0)\n", st.pos, st.playing,
	       (long long)evs[0].out);

	/* velocidade por evento + Play Events + Loop */
	engine().autoEvent(1);
	evs = engine().events(0);
	engine().setEventSpeed(0, evs[0].id, 50);
	engine().setEventAngleChecked(0, evs[1].id, 3, true);
	engine().setLoop(true);
	engine().playEvents(0, 0);
	os_sleep_ms(300);
	st = engine().channel(0).state();
	printf("play events: speed=%.2f (esperado 0.50) pos=%.1f range=%d\n", st.speed, st.pos, st.hasRange);
	os_sleep_ms(12000);
	st = engine().channel(0).state();
	printf("apos 12s com loop: playing=%d (esperado 1) pos=%.1f angle=%d\n", st.playing, st.pos, st.angle);
	engine().setLoop(false);
	engine().pause();

	/* transição entre eventos: mix de 500 ms na virada */
	{
		ReplayConfig c2 = engine().config();
		c2.eventTransition = getenv("XF") ? atoi(getenv("XF")) : 1;
		c2.eventDurationMs = 500;
		engine().setConfig(c2);
		evs = engine().events(0);
		engine().setEventSpeed(0, evs[0].id, 100);
		engine().setSpeedPercent(100);
		/* evento 1 curto (1 s) para a virada chegar logo */
		ReplayEvent e1 = evs[0];
		engine().pause();
		std::vector<PlayRange> rr(2);
		rr[0].in = e1.out - 30; rr[0].out = e1.out; rr[0].angle = 0;
		rr[1].in = evs[1].in;   rr[1].out = evs[1].out; rr[1].angle = 0;
		engine().channel(0).playList(rr, true);
		std::string line = "virada (fim ev1 -> ev2):";
		for (int i = 0; i < 40; i++) {
			os_sleep_ms(33);
			line += " " + std::to_string(outLuma.load());
		}
		printf("%s\n", line.c_str());
		engine().pause();
	}

	engine().exportEvents(0, 0);
	for (int i = 0; i < 100 && engine().exporting(); i++)
		os_sleep_ms(100);
	printf("export: %s\n", engine().exportStatus().c_str());

	/* nova gravação: os clipes antigos continuam e ainda tocam */
	{
		const size_t before = engine().events(0).size();
		const int64_t oldId = engine().events(0)[0].id;
		engine().stopRecording();
		os_sleep_ms(300);
		std::string e2;
		engine().startRecording(e2);
		os_sleep_ms(2500);
		engine().autoEvent(1);
		printf("eventos antes=%zu depois=%zu (esperado +1)\n", before, engine().events(0).size());
		engine().setLoop(false);
		engine().setSpeedPercent(100);
		engine().playEventAllAngles(0, oldId);
		os_sleep_ms(600);
		ChannelState o = engine().channel(0).state();
		printf("clipe antigo: pos=%.0f live(sessao tocando)=%lld recLive=%lld playing=%d\n", o.pos, (long long)o.live,
		       (long long)o.recLive, o.playing);
		sample("imagem do clipe antigo", 6);
		engine().goLive();
		os_sleep_ms(300);
		o = engine().channel(0).state();
		printf("ao vivo: live=%lld recLive=%lld (iguais)\n", (long long)o.live, (long long)o.recLive);
		uint64_t by = 0; double sc = 0;
		engine().recordingStats(by, sc);
		printf("stats: %.1f MB em %.1f s\n", by / 1e6, sc);
	}

	printf("quadros perdidos: %llu\n", (unsigned long long)engine().droppedFrames());
	engine().stopRecording();

	obs_source_release(cam);
	obs_source_release(cam2);
	/* fecha tudo e abre de novo: os clipes voltam do disco e tocam */
	engineDestroy();
	engineCreate();
	{
		auto again = engine().events(0);
		printf("apos reabrir: %zu eventos\n", again.size());
		if (!again.empty()) {
			engine().setSelection(SEL_A);
			engine().playEventAllAngles(0, again[0].id);
			os_sleep_ms(500);
			ChannelState o = engine().channel(0).state();
			printf("clipe reaberto do disco: pos=%.0f playing=%d fps=%.0f\n", o.pos, o.playing, o.fps);
			sample("imagem do clipe reaberto", 6);
		}
	}
	obs_remove_raw_video_callback(outCb, nullptr);
	obs_set_output_source(0, nullptr);
	obs_source_release(out);
	engineDestroy();
	obs_shutdown();
	printf("fim ok\n");
	return 0;
}
