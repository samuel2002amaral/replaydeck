#include "engine.hpp"
#include "replay-dock.hpp"
#include <obs-module.h>
#include <QApplication>
#include <QMouseEvent>
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
static void mouse(QWidget *w, QEvent::Type t, QPoint local) {
	QMouseEvent e(t, QPointF(local), w->mapToGlobal(QPointF(local)), Qt::LeftButton, t == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
	QApplication::sendEvent(w, &e);
}
int main(int argc, char **argv) {
	QApplication app(argc, argv);
	app.setStyle(QStyleFactory::create("Fusion"));
	obs_startup("en-US", nullptr, nullptr);
	obs_video_info ovi = {}; ovi.graphics_module = "libobs-opengl"; ovi.fps_num = 30; ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = 1280; ovi.base_height = ovi.output_height = 720; ovi.output_format = VIDEO_FORMAT_NV12;
	obs_reset_video(&ovi); registerReplaySource(); registerReplayCaptureOutput(); engineCreate();
	QSettings st("obs-instant-replay", "replay-dock"); st.clear();
	st.setValue("blockOrder", QStringList{"transport", "live", "mark", "speed", "br1", "loopaudio", "zoom", "cams", "air"});
	st.setValue("zoomParts", 1 | 4 | 8 | 16); st.setValue("zoomCross", true); st.sync();
	auto *d = new ReplayDock(); d->resize(980, 400); d->show(); pump();
	d->grab().save("/tmp/wiz/a0.png");
	d->setArranging(true); pump();
	d->grab().save("/tmp/wiz/a1.png");
	// arrasta o handle de "air" para antes de "transport"
	QWidget *hAir = nullptr, *hTr = nullptr;
	for (QWidget *w : d->findChildren<QWidget *>()) {
		if (strcmp(w->metaObject()->className(), "QWidget") != 0 && strcmp(w->metaObject()->className(), "BlockHandle") != 0) continue;
	}
	// localiza pelo tooltip/geom: handles são filhos diretos do ctrlW com cursor de mão
	QList<QWidget *> hs;
	for (QWidget *w : d->findChildren<QWidget *>())
		if (w->cursor().shape() == Qt::OpenHandCursor && w->isVisible() && w->toolTip().startsWith(QString::fromUtf8("Arraste"))) hs << w;
	printf("handles=%d\n", (int)hs.size());
	std::sort(hs.begin(), hs.end(), [](QWidget *a, QWidget *b) { return a->y() != b->y() ? a->y() < b->y() : a->x() < b->x(); });
	hTr = hs.first(); hAir = hs.last();
	QPoint c = hAir->rect().center();
	mouse(hAir, QEvent::MouseButtonPress, c);
	QPoint target = hTr->geometry().topLeft() + QPoint(4, hTr->height() / 2);
	QPoint local = hAir->mapFromParent(target);
	mouse(hAir, QEvent::MouseMove, local); pump(5);
	d->grab().save("/tmp/wiz/a2.png");
	mouse(hAir, QEvent::MouseButtonRelease, hAir->mapFromParent(target)); pump();
	d->grab().save("/tmp/wiz/a3.png");
	st.sync();
	printf("ordem: %s\n", QSettings("obs-instant-replay", "replay-dock").value("blockOrder").toStringList().join(",").toUtf8().constData());
	d->setArranging(false); pump();
	d->grab().save("/tmp/wiz/a4.png");
	fflush(stdout); _Exit(0);
}
