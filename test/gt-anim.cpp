/* Fonte GT dentro da libobs: captura quadros durante a entrada e a saída. */
#include "../src/gt/gt-source.hpp"
#include <obs-module.h>
#include <util/platform.h>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

OBS_DECLARE_MODULE()

static std::mutex fm;
static std::vector<uint8_t> frame;
static uint32_t fw = 0, fh = 0, fls = 0;
static void outCb(void *, struct video_data *f)
{
	std::lock_guard<std::mutex> lk(fm);
	fls = f->linesize[0];
	frame.assign(f->data[0], f->data[0] + (size_t)fls * fh);
}
static void grab(const char *path)
{
	std::lock_guard<std::mutex> lk(fm);
	QImage img(frame.data(), (int)fw, (int)fh, (int)fls, QImage::Format_RGBA8888);
	img.copy().scaled(960, 540, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).save(path);
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
	fw = ovi.base_width = ovi.output_width = 1920;
	fh = ovi.base_height = ovi.output_height = 1080;
	ovi.output_format = VIDEO_FORMAT_NV12; ovi.gpu_conversion = true;
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS) { printf("video falhou\n"); return 1; }
	registerGtSource();

	obs_data_t *st = obs_data_create();
	obs_data_set_string(st, "file", argv[1]);
	obs_source_t *gt = obs_source_create(GT_SOURCE_ID, "ESCALACAO", st, nullptr);
	obs_data_release(st);
	printf("titulo: %s %ux%u paginas=%d\n", gtTitleName(gt).toUtf8().constData(), obs_source_get_width(gt), obs_source_get_height(gt), (int)gtPages(gt).size());

	struct video_scale_info conv = {VIDEO_FORMAT_RGBA, 1920, 1080, VIDEO_RANGE_FULL, VIDEO_CS_709};
	obs_add_raw_video_callback(&conv, outCb, nullptr);

	QMap<QString, QString> v;
	v["TITULO TIME.Text"] = QString::fromUtf8("FORÇA JOVEM");
	v["TITULO TIME1.Text"] = "01     GBRAND\r\n02     SURICATE\r09     LUCAS ALVES\r07     VIDAL\r10     BODE\r08     SAMI\r17     JOSIEL\r05     PAULO ANDRE\r04     BRUNO\r11     JEFFERSON";
	v["TITULO TIME2.Text"] = "TECNICO ADERSON";
	gtSetValues(gt, v);

	obs_set_output_source(0, gt); /* vai ao ar: a entrada começa sozinha */
	const int inMs[] = {100, 250, 400, 550, 700, 1300};
	int last = 0;
	for (int ms : inMs) {
		os_sleep_ms(ms - last);
		last = ms;
		char p[64];
		snprintf(p, sizeof(p), "/tmp/gt-in-%04d.png", ms);
		grab(p);
		printf("entrada %d ms: estado=%d\n", ms, gtState(gt));
	}
	gtPlayOut(gt);
	last = 0;
	const int outMs[] = {150, 350, 600, 1300};
	for (int ms : outMs) {
		os_sleep_ms(ms - last);
		last = ms;
		char p[64];
		snprintf(p, sizeof(p), "/tmp/gt-out-%04d.png", ms);
		grab(p);
		printf("saida %d ms: estado=%d\n", ms, gtState(gt));
	}
	obs_remove_raw_video_callback(outCb, nullptr);
	obs_set_output_source(0, nullptr);
	obs_source_release(gt);
	obs_shutdown();
	printf("fim ok\n");
	return 0;
}
