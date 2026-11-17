#include "engine.hpp"
#include "replay-dock.hpp"
#include <obs-module.h>
#include <QApplication>
#include <QPushButton>
#include <QSettings>
#include <QStyleFactory>
#include <QThread>
#include <cstdio>
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
	auto *d = new ReplayDock(); d->resize(536, 380); d->show(); pump();
	d->grab().save("/tmp/wiz/m536.png");
	for (int w : {536, 420, 330, 200}) {
		d->resize(w, 150); pump();
		int hidden = 0, total = 0;
		for (QPushButton *b : d->findChildren<QPushButton *>()) {
			if (!b->isVisibleTo(d)) continue;
			total++;
			QRect r(b->mapTo(d, QPoint(0, 0)), b->size());
			if (!d->rect().contains(r)) hidden++;
		}
		printf("pedido %dx150 -> real %dx%d  min=%dx%d  botoes fora=%d de %d\n", w, d->width(), d->height(), d->minimumSizeHint().width(), d->minimumSizeHint().height(), hidden, total);
		d->grab().save(QString("/tmp/wiz/m%1.png").arg(w));
	}
	fflush(stdout); _Exit(0);
}
