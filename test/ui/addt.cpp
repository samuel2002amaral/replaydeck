#include "engine.hpp"
#include "setup-wizard.hpp"
#include <obs-module.h>
#include <QApplication>
#include <QComboBox>
#include <QTimer>
#include <cstdio>
#include <cstring>
OBS_DECLARE_MODULE()
size_t pluginHotkeyCount() { return 0; }
const char *pluginHotkeyDesc(size_t) { return ""; }
void pluginRunHotkey(size_t) {}
void registerReplaySource();
static const char *fn(void *) { return "fake"; }
static void *fc(obs_data_t *, obs_source_t *s) { return s; }
static void fd(void *) {}
static uint32_t fw(void *) { return 640; }
static uint32_t fh(void *) { return 360; }
static void reg(const char *id) { obs_source_info i = {}; i.id = id; i.type = OBS_SOURCE_TYPE_INPUT; i.output_flags = OBS_SOURCE_VIDEO; i.get_name = fn; i.create = fc; i.destroy = fd; i.get_width = fw; i.get_height = fh; obs_register_source(&i); }
int main(int argc, char **argv) {
	QApplication app(argc, argv);
	obs_startup("en-US", nullptr, nullptr);
	for (const char *id : {"dshow_input", "ffmpeg_source", "game_capture", "monitor_capture", "window_capture", "browser_source", "text_gdiplus", "color_source_v3", "image_source"}) reg(id);
	{ obs_source_info a = {}; a.id = "wasapi_input_capture"; a.type = OBS_SOURCE_TYPE_INPUT; a.output_flags = OBS_SOURCE_AUDIO; a.get_name = fn; a.create = fc; a.destroy = fd; obs_register_source(&a); }
	obs_video_info ovi = {}; ovi.graphics_module = "libobs-opengl"; ovi.fps_num = 30; ovi.fps_den = 1; ovi.base_width = ovi.output_width = 640; ovi.base_height = ovi.output_height = 360; ovi.output_format = VIDEO_FORMAT_NV12; obs_reset_video(&ovi);
	engineCreate();
	QTimer::singleShot(300, [] { fprintf(stderr,"T\n"); for (QWidget *w : QApplication::topLevelWidgets()) fprintf(stderr,"w %s vis=%d\n", w->windowTitle().toUtf8().constData(), w->isVisible());
		for (QWidget *w : QApplication::topLevelWidgets())
			if (w->isVisible() && w->windowTitle().contains(QString::fromUtf8("Adicionar"))) {
				for (QComboBox *c : w->findChildren<QComboBox *>()) { printf("--\n");
				for (int i = 0; i < c->count(); i++) printf("[%s] %s\n", c->itemData(i).toString().toUtf8().constData(), c->itemText(i).toUtf8().constData()); }
				QMetaObject::invokeMethod(w, "reject");
			}
	});
	setup::addCameraDialog(nullptr);
	fflush(stdout); _Exit(0);
}
