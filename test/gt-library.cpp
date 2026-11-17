#include "../src/gt/gt-source.hpp"
#include "../src/overlay-channels.hpp"
#include <obs-module.h>
#include <util/platform.h>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <cstdio>
#include <mutex>
#include <vector>
OBS_DECLARE_MODULE()
static std::mutex fm;
static std::vector<uint8_t> frame;
static uint32_t fls = 0;
static void outCb(void *, struct video_data *f)
{
	std::lock_guard<std::mutex> lk(fm);
	fls = f->linesize[0];
	frame.assign(f->data[0], f->data[0] + (size_t)fls * 1080);
}
static int lumaAt(int x, int y)
{
	std::lock_guard<std::mutex> lk(fm);
	const uint8_t *p = frame.data() + (size_t)y * fls + x * 4;
	return (p[0] + p[1] + p[2]) / 3;
}
int main(int argc, char **argv)
{
	QGuiApplication app(argc, argv);
	obs_startup("en-US", nullptr, nullptr);
	struct obs_video_info ovi = {};
	ovi.graphics_module = "libobs-opengl";
	ovi.fps_num = 30; ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = 1920; ovi.base_height = ovi.output_height = 1080;
	ovi.output_format = VIDEO_FORMAT_NV12; ovi.gpu_conversion = true;
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS) return 1;
	registerGtSource();
	struct video_scale_info conv = {VIDEO_FORMAT_RGBA, 1920, 1080, VIDEO_RANGE_FULL, VIDEO_CS_709};
	obs_add_raw_video_callback(&conv, outCb, nullptr);
	overlaysCreate();

	obs_source_t *t = gtLibraryAdd(argv[1]);
	printf("biblioteca: '%s' titulo carregado=%d privado=%d\n", obs_source_get_name(t), !gtTitleName(t).isEmpty(), obs_obj_is_private(t));
	obs_source_t *pub = obs_get_source_by_name(obs_source_get_name(t));
	printf("aparece como fonte publica (cena): %s\n", pub ? "sim" : "nao");
	obs_source_release(pub);
	os_sleep_ms(300);
	printf("antes: %d\n", lumaAt(960, 300));
	overlays().assignOn(0, obs_source_get_name(t));
	os_sleep_ms(1500);
	printf("overlay 1 ligado: centro=%d estado=%d\n", lumaAt(960, 300), gtState(t));
	overlays().setOn(0, false);
	os_sleep_ms(1500);
	printf("desligado: centro=%d estado=%d\n", lumaAt(960, 300), gtState(t));
	overlaysDestroy();
	obs_source_release(t);
	gtLibraryShutdown();
	obs_remove_raw_video_callback(outCb, nullptr);
	obs_shutdown();
	printf("fim ok\n");
	return 0;
}
