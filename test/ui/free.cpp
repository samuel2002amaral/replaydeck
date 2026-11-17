#include "engine.hpp"
#include "replay-dock.hpp"
#include <obs-module.h>
#include <QApplication>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QStyleFactory>
#include <QThread>
#include <cstdio>
#include <algorithm>
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
static QList<QWidget *> handles(QWidget *d) {
	QList<QWidget *> hs;
	for (QWidget *w : d->findChildren<QWidget *>())
		if (w->isVisible() && w->toolTip().startsWith(QString::fromUtf8("Arraste"))) hs << w;
	std::sort(hs.begin(), hs.end(), [](QWidget *a, QWidget *b) { return a->y() != b->y() ? a->y() < b->y() : a->x() < b->x(); });
	return hs;
}
static void drag(QWidget *h, QPoint targetTopLeftInParent) {
	QPoint g(6, 6);
	mouse(h, QEvent::MouseButtonPress, g);
	mouse(h, QEvent::MouseMove, h->mapFromParent(targetTopLeftInParent + g)); pump(5);
	mouse(h, QEvent::MouseButtonRelease, h->mapFromParent(targetTopLeftInParent + g)); pump(10);
}
int main(int argc, char **argv) {
	QApplication app(argc, argv);
	app.setStyle(QStyleFactory::create("Fusion"));
	obs_startup("en-US", nullptr, nullptr);
	obs_video_info ovi = {}; ovi.graphics_module = "libobs-opengl"; ovi.fps_num = 30; ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = 1280; ovi.base_height = ovi.output_height = 720; ovi.output_format = VIDEO_FORMAT_NV12;
	obs_reset_video(&ovi); registerReplaySource(); registerReplayCaptureOutput(); engineCreate();
	QSettings st("obs-instant-replay", "replay-dock"); st.clear(); st.sync();
	auto *d = new ReplayDock(); d->resize(760, 400); d->show(); pump();
	d->setArranging(true); pump();
	for (QPushButton *b : d->findChildren<QPushButton *>()) if (b->text() == QString::fromUtf8("Posição livre")) b->click();
	pump();
	auto hs = handles(d);
	printf("handles=%d\n", (int)hs.size());
	QWidget *air = hs.last(); QWidget *tr = hs.first();
	// 1) solta "air" no canto direito da primeira linha (espaço vazio)
	drag(air, QPoint(tr->parentWidget()->width() - air->width() - 4, tr->y()));
	d->grab().save("/tmp/wiz/f1.png");
	// 2) solta "air" exatamente em cima do transporte
	hs = handles(d); air = nullptr;
	for (QWidget *h : hs) if (h->x() > 400 && h->y() < 30) air = h;
	if (air) drag(air, tr->pos());
	d->grab().save("/tmp/wiz/f2.png");
	st.sync();
	QSettings s2("obs-instant-replay", "replay-dock");
	printf("free=%d air=(%d,%d) transport=(%d,%d)\n", s2.value("freeLayout").toBool(), s2.value("freePos/air").toPoint().x(), s2.value("freePos/air").toPoint().y(), s2.value("freePos/transport").toPoint().x(), s2.value("freePos/transport").toPoint().y());
	d->setArranging(false); pump();
	d->grab().save("/tmp/wiz/f3.png");
	fflush(stdout); _Exit(0);
}
