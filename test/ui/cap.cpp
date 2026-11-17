#include "engine.hpp"
#include "replay-dock.hpp"
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
	QSettings st("obs-instant-replay", "replay-dock"); st.clear();
	st.setValue("parts/mark", false); st.setValue("groupTitleStyle", argc > 3 ? atoi(argv[3]) : 1); st.setValue("parts/camB", false);
	st.setValue("zoomParts", 2 | 4 | 16); st.setValue("zoomCross", true);
	st.setValue("blockOrder", QStringList{"transport", "live", "mark", "br1", "air", "cams", "br2", "speed", "loopaudio", "zoom"});
	if (argc > 3) st.setValue("groupTitles", false);
	st.sync();
	auto *d = new ReplayDock(); d->resize(argc > 1 ? atoi(argv[1]) : 520, 400); d->show(); pump();
	d->grab().save(argc > 2 ? argv[2] : "/tmp/wiz/cap.png");
	int top = 1e9, bot = 0;
	for (QWidget *w : d->findChildren<QWidget *>())
		if (w->isVisible() && (w->property("irCard").toBool() || w->property("irBlock").isValid())) {
			QRect r(w->mapTo(d, QPoint(0, 0)), w->size());
			top = std::min(top, r.top()); bot = std::max(bot, r.bottom());
		}
	printf("altura dos controles: %d px\n", bot - top + 1);
	fflush(stdout); _Exit(0);
}
