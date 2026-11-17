#include "engine.hpp"
#include "replay-dock.hpp"
#include <obs-module.h>
#include <QApplication>
#include <QPushButton>
#include <QSettings>
#include <QThread>
#include <cstdio>
OBS_DECLARE_MODULE()
size_t pluginHotkeyCount() { return 0; }
const char *pluginHotkeyDesc(size_t) { return ""; }
void pluginRunHotkey(size_t) {}
void registerReplaySource();
int main(int argc, char **argv) {
	QApplication app(argc, argv);
	obs_startup("en-US", nullptr, nullptr);
	obs_video_info ovi = {}; ovi.graphics_module = "libobs-opengl"; ovi.fps_num = 30; ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = 1280; ovi.base_height = ovi.output_height = 720; ovi.output_format = VIDEO_FORMAT_NV12;
	obs_reset_video(&ovi); registerReplaySource(); registerReplayCaptureOutput(); engineCreate();
	QSettings st("obs-instant-replay", "replay-dock");
	for (const char *k : {"zoom", "clock", "sel", "speed"}) st.setValue(QString("parts/") + k, false);
	st.sync();
	auto *d = new ReplayDock(); d->resize(900, 330); d->show();
	for (int i = 0; i < 40; i++) { QApplication::processEvents(); QThread::msleep(20); }
	int vis = 0, tot = 0;
	for (QPushButton *b : d->findChildren<QPushButton *>())
		if (b->toolTip().startsWith("Zoom")) { tot++; vis += b->isVisibleTo(d); }
	printf("zoom visiveis %d de %d\n", vis, tot);
	d->grab().save("/tmp/wiz/parts.png");
	for (const char *k : {"zoom", "clock", "sel", "speed"}) st.remove(QString("parts/") + k);
	fflush(stdout); _Exit(0);
}
