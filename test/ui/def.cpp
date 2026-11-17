#include "engine.hpp"
#define private public
#define protected public
#include "replay-dock.hpp"
#undef private
#undef protected
#include <obs-module.h>
#include <QApplication>
#include <QSettings>
#include <QStyleFactory>
#include <QThread>
#include <cstdio>
OBS_DECLARE_MODULE()
size_t pluginHotkeyCount() { return 0; }
const char *pluginHotkeyDesc(size_t) { return ""; }
void pluginRunHotkey(size_t) {}
void registerReplaySource();
static void pump(int n = 40) { for (int i = 0; i < n; i++) { QApplication::processEvents(); QThread::msleep(15); } }
int main(int argc, char **argv) {
	QApplication app(argc, argv);
	app.setStyle(QStyleFactory::create("Fusion"));
	obs_startup("en-US", nullptr, nullptr);
	obs_video_info ovi = {}; ovi.graphics_module = "libobs-opengl"; ovi.fps_num = 30; ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = 1280; ovi.base_height = ovi.output_height = 720; ovi.output_format = VIDEO_FORMAT_NV12;
	obs_reset_video(&ovi); registerReplaySource(); registerReplayCaptureOutput(); engineCreate();
	QSettings st("obs-instant-replay", "replay-dock"); st.clear(); st.sync();
	auto *d = new ReplayDock(); d->resize(620, 460); d->show(); pump();
	d->grab().save("/tmp/wiz/def1.png");
	st.setValue("groupTitleStyle", 4); st.sync();
	d->rewrapBlocks(); d->buildZoom(); d->applyBlockOrder(); d->applyDockLayout(); d->applyParts(); pump();
	d->grab().save("/tmp/wiz/def2.png");
	st.setValue("groupTitleStyle", 0); st.sync();
	d->rewrapBlocks(); d->buildZoom(); d->applyBlockOrder(); d->applyDockLayout(); d->applyParts(); pump();
	d->grab().save("/tmp/wiz/def3.png");
	printf("ok\n"); fflush(stdout); _Exit(0);
}
