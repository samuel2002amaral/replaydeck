#include "engine.hpp"
#define private public
#define protected public
#include "replay-dock.hpp"
#undef private
#undef protected
#include <obs-module.h>
#include <QApplication>
#include <QDockWidget>
#include <QMainWindow>
#include <QSettings>
#include <QStyleFactory>
#include <QThread>
#include <set>
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
	int style = argc > 1 ? atoi(argv[1]) : 2;
	QSettings st("obs-instant-replay", "replay-dock"); st.clear(); st.setValue("groupTitleStyle", style); st.sync();
	QMainWindow mw; mw.resize(1400, 900); mw.show();
	auto *dw = new QDockWidget("ReplayDeck", &mw);
	auto *d = new ReplayDock(); dw->setWidget(d); mw.addDockWidget(Qt::BottomDockWidgetArea, dw);
	dw->setFloating(true); dw->resize(300, 300); dw->show(); pump();
	d->fitFloating(dw, true); pump(); d->fitFloating(dw, true); pump();
	std::set<int> ys;
	for (auto it = d->blocks.begin(); it != d->blocks.end(); ++it)
		if (!it.key().startsWith("br") && !it.value()->isHidden() && it.value()->parentWidget() == d->ctrlW) ys.insert(it.value()->y());
	printf("estilo %d: dock %dx%d  linhas=%d\n", style, dw->width(), dw->height(), (int)ys.size());
	d->grab().save(QString("/tmp/wiz/fit%1.png").arg(style));
	fflush(stdout); _Exit(0);
}
