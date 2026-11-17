#include "../src/gt/gt-source.hpp"
#include "../src/overlay-channels.hpp"
#include <obs-module.h>
#include <util/platform.h>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QFileInfo>
#include <QImage>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

OBS_DECLARE_MODULE()
bool gtConvertToScene(const QString &file, QString &message);

static std::mutex fm;
static std::vector<uint8_t> frame;
static uint32_t fls = 0;
static void outCb(void *, struct video_data *f)
{
	std::lock_guard<std::mutex> lk(fm);
	fls = f->linesize[0];
	frame.assign(f->data[0], f->data[0] + (size_t)fls * 1080);
}
static void grab(const char *path)
{
	std::lock_guard<std::mutex> lk(fm);
	QImage(frame.data(), 1920, 1080, (int)fls, QImage::Format_RGBA8888).copy().scaled(960, 540, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).save(path);
}
static int lumaAt(int x, int y)
{
	std::lock_guard<std::mutex> lk(fm);
	const uint8_t *p = frame.data() + (size_t)y * fls + x * 4;
	return (p[0] + p[1] + p[2]) / 3;
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

int main(int argc, char **argv)
{
	QGuiApplication app(argc, argv);
	for (const char *w : {"400", "600", "700"})
		QFontDatabase::addApplicationFont(QString("/home/claude/gt/fonts/Montserrat-%1.ttf").arg(w));
	obs_startup("en-US", nullptr, nullptr);
	struct obs_video_info ovi = {};
	ovi.graphics_module = "libobs-opengl";
	ovi.fps_num = 30; ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = 1920;
	ovi.base_height = ovi.output_height = 1080;
	ovi.output_format = VIDEO_FORMAT_NV12; ovi.gpu_conversion = true;
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS) return 1;
	loadModule("image-source");
	loadModule("text-freetype2");
	registerGtSource();
	struct video_scale_info conv = {VIDEO_FORMAT_RGBA, 1920, 1080, VIDEO_RANGE_FULL, VIDEO_CS_709};
	obs_add_raw_video_callback(&conv, outCb, nullptr);

	/* 1) conversor (sem a interface do OBS, o teste segura a cena criada) */
	static obs_source_t *kept = nullptr;
	signal_handler_connect(obs_get_signal_handler(), "source_create", [](void *, calldata_t *cd) {
		obs_source_t *s = (obs_source_t *)calldata_ptr(cd, "source");
		if (!kept && obs_source_is_scene(s) && strncmp(obs_source_get_name(s), "GT - ", 5) == 0)
			kept = obs_source_get_ref(s);
	}, nullptr);
	QString msg;
	bool ok = gtConvertToScene(argv[1], msg);
	printf("converter: %d\n%s\n", ok, msg.toUtf8().constData());
	obs_source_t *scene = kept;
	printf("cena criada: %s\n", scene ? "sim" : "nao");
	if (scene) {
		obs_set_output_source(0, scene);
		os_sleep_ms(800);
		grab("/tmp/gt-converted.png");
		obs_set_output_source(0, nullptr);
		obs_source_release(scene);
	}

	/* 2) título GT num canal de overlay */
	obs_data_t *st = obs_data_create();
	obs_data_set_string(st, "file", argv[1]);
	obs_source_t *gt = obs_source_create(GT_SOURCE_ID, "ESCALACAO", st, nullptr);
	obs_data_release(st);
	overlaysCreate();
	os_sleep_ms(300);
	printf("antes: centro=%d estado=%d\n", lumaAt(960, 540), gtState(gt));
	overlays().assignOn(0, "ESCALACAO");
	os_sleep_ms(1500);
	printf("overlay ligado: centro=%d estado=%d canal=%d\n", lumaAt(960, 540), gtState(gt), overlays().channelOf("ESCALACAO"));
	grab("/tmp/gt-overlay-on.png");
	overlays().setOn(0, false);
	os_sleep_ms(400);
	printf("saindo: estado=%d\n", gtState(gt));
	os_sleep_ms(1200);
	obs_source_t *still = obs_get_output_source(50);
	printf("depois da saida: centro=%d estado=%d canal50=%s\n", lumaAt(960, 540), gtState(gt), still ? "ocupado" : "livre");
	obs_source_release(still);
	overlaysDestroy();
	obs_source_release(gt);

	obs_remove_raw_video_callback(outCb, nullptr);
	obs_shutdown();
	printf("fim ok\n");
	return 0;
}
