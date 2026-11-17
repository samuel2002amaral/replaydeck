#include "engine.hpp"
#include "replay-dock.hpp"
#include <obs-module.h>
#include <QApplication>
#include <QDialog>
#include <QSettings>
#include <QStyleFactory>
#include <QTabWidget>
#include <QThread>
#include <QTimer>
#include <cstdio>
#include <cstring>
OBS_DECLARE_MODULE()
size_t pluginHotkeyCount() { return 0; }
const char *pluginHotkeyDesc(size_t) { return ""; }
void pluginRunHotkey(size_t) {}
void registerReplaySource();
static void pump(int n = 30) { for (int i = 0; i < n; i++) { QApplication::processEvents(); QThread::msleep(15); } }
int main(int argc, char **argv) {
	QApplication app(argc, argv);
	app.setStyle(QStyleFactory::create("Fusion"));
	obs_startup("en-US", nullptr, nullptr);
	obs_video_info ovi = {}; ovi.graphics_module = "libobs-opengl"; ovi.fps_num = 30; ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = 1280; ovi.base_height = ovi.output_height = 720; ovi.output_format = VIDEO_FORMAT_NV12;
	obs_reset_video(&ovi); registerReplaySource(); registerReplayCaptureOutput(); engineCreate();
	QSettings st("obs-instant-replay", "replay-dock"); st.clear(); st.sync();
	engine().zoomSet(1.8f, 0.35f, 0.45f);
	auto *d = new ReplayDock(); d->resize(980, 360); d->show(); pump();
	d->grab().save("/tmp/wiz/dock0.png");
	// layout lado a lado + ordem custom
	st.setValue("dockLayout", 2); st.setValue("blockOrder", QStringList{"zoom", "cams", "transport"}); st.setValue("zoomLayout", 3); st.sync();
	QMetaObject::invokeMethod(d, [d] {}, Qt::DirectConnection);
	QTimer::singleShot(300, [] {
		fprintf(stderr, "TIMER tl=%d\n", (int)QApplication::topLevelWidgets().size());
		for (QWidget *w : QApplication::topLevelWidgets()) fprintf(stderr, " w %s vis=%d dlg=%d\n", w->metaObject()->className(), w->isVisible(), qobject_cast<QDialog *>(w) != nullptr);
		for (QWidget *w : QApplication::topLevelWidgets())
			if (strcmp(w->metaObject()->className(), "QDialog") == 0 && w->isVisible()) {
				auto *tabs = w->findChild<QTabWidget *>();
				for (int t = 0; t < tabs->count(); t++)
					if (tabs->tabText(t) == "Zoom" || tabs->tabText(t) == "Painel") {
						tabs->setCurrentIndex(t); pump(10);
						w->grab().save(QString("/tmp/wiz/cfg-%1.png").arg(tabs->tabText(t)));
					}
				QMetaObject::invokeMethod(w, "accept");
			}
	});
	QMetaObject::invokeMethod(d, "openConfig");
	pump();
	d->grab().save("/tmp/wiz/dock2.png");
	printf("ok\n"); fflush(stdout); _Exit(0);
}
