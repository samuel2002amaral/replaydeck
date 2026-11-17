/* Gera imagens dos painéis do plugin (sem o OBS completo) para revisar o visual. */
#include "../src/engine.hpp"
#include "../src/overlay-channels.hpp"
#include "../src/replay-dock.hpp"
#include "../src/overlay-dock.hpp"
#include "../src/gt/gt-dock.hpp"
#include "../src/gt/gt-source.hpp"
void gtOpenEditor(obs_source_t *src, QWidget *parent);
#include <obs-module.h>
#include <QApplication>
#include <QDialog>
#include <QPalette>
#include <QStyleFactory>
#include <QTabWidget>
#include <QPushButton>
#include <QTimer>
#include <QThread>
#include <cstdio>
#include <cstdlib>

OBS_DECLARE_MODULE()
size_t pluginHotkeyCount() { return 0; }
const char *pluginHotkeyDesc(size_t) { return ""; }
void pluginRunHotkey(size_t) {}
void registerReplaySource();

static void shot(QWidget *w, const char *file, int width, int height, int waits = 20)
{
	w->resize(width, height);
	w->show();
	for (int i = 0; i < waits; i++) {
		QApplication::processEvents();
		QThread::msleep(20);
	}
	w->grab().save(file);
	printf("salvo %s (%dx%d)\n", file, w->width(), w->height());
}

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	app.setStyle(QStyleFactory::create("Fusion"));
	QPalette pal; /* aproximação do tema escuro padrão do OBS (Yami) */
	pal.setColor(QPalette::Window, QColor("#1d1f26"));
	pal.setColor(QPalette::WindowText, QColor("#e0e0e0"));
	pal.setColor(QPalette::Base, QColor("#16181d"));
	pal.setColor(QPalette::Button, QColor("#3b3e4a"));
	pal.setColor(QPalette::ButtonText, QColor("#e0e0e0"));
	pal.setColor(QPalette::Text, QColor("#e0e0e0"));
	app.setPalette(pal);

	obs_startup("en-US", nullptr, nullptr);
	struct obs_video_info ovi = {};
	ovi.graphics_module = "libobs-opengl";
	ovi.fps_num = 30;
	ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = 1920;
	ovi.base_height = ovi.output_height = 1080;
	ovi.output_format = VIDEO_FORMAT_NV12;
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS)
		return 1;
	registerReplaySource();
	registerReplayCaptureOutput();
	registerGtSource();
	engineCreate();
	overlaysCreate();
	const char *names[] = {"ESCALAÇÃO CASA", "ESCALACAO FORA", "PLACAR1", "GOL CASA", "RESUMO PARTIDA"};
	int ni = 0;
	for (int rep = 0; rep < 3 && ni < 5; rep++)
		for (int i = 1; i < argc && ni < 5; i++) {
			obs_source_t *s = gtLibraryAdd(argv[i]);
			gtRename(s, QString::fromUtf8(names[ni++]));
			obs_source_release(s);
		}
	{
		OverlaySlot sl;
		sl.source = "PLACAR1";
		sl.transition = "*fade";
		overlays().setSlot(0, sl);
		sl.source = "ESCALAÇÃO CASA";
		overlays().setSlot(1, sl);
		sl.source = "LOGO IURY FILMS";
		overlays().setSlot(3, sl);
	}

	const char *out = getenv("UI_OUT") ? getenv("UI_OUT") : "/tmp/uiprev";
	auto *dock = new ReplayDock();
	shot(dock, (std::string(out) + "/replay-wide.png").c_str(), 880, 330);
	shot(dock, (std::string(out) + "/replay-narrow.png").c_str(), 520, 420);

	overlays().setOn(0, true);
	auto *ov = new OverlayDock();
	shot(ov, (std::string(out) + "/overlays.png").c_str(), 330, 140);

	auto *gt = new GtDock();
	shot(gt, (std::string(out) + "/gt-list.png").c_str(), 345, 420, 120);
	for (QPushButton *b : gt->findChildren<QPushButton *>())
		if (b->text() == QString::fromUtf8("▦")) {
			b->click();
			break;
		}
	shot(gt, (std::string(out) + "/gt.png").c_str(), 345, 560, 150);

	{
		std::vector<obs_source_t *> lib = gtLibraryList();
		if (!lib.empty()) {
			gtOpenEditor(lib[0], nullptr);
			for (QWidget *w : QApplication::topLevelWidgets())
				if (w->windowTitle().startsWith(QString::fromUtf8("Editor")))
					shot(w, (std::string(out) + "/editor.png").c_str(), 900, 560, 60);
		}
		for (auto *s : lib)
			obs_source_release(s);
	}
	/* janela de configuração: uma imagem por aba */
	QTimer::singleShot(400, [out] {
		QWidget *m = QApplication::activeModalWidget();
		if (!m) {
			for (QWidget *w : QApplication::topLevelWidgets())
				if (qobject_cast<QDialog *>(w) && w->isVisible())
					m = w;
		}
		if (!m) {
			printf("sem diálogo\n");
			return;
		}
		auto *tabs = m->findChild<QTabWidget *>();
		for (int t = 0; tabs && t < tabs->count(); t++) {
			tabs->setCurrentIndex(t);
			for (int i = 0; i < 10; i++)
				QApplication::processEvents();
			const std::string f = std::string(out) + "/config-" + std::to_string(t) + ".png";
			m->grab().save(f.c_str());
			printf("salvo %s\n", f.c_str());
		}
		static_cast<QDialog *>(m)->reject();
	});
	QMetaObject::invokeMethod(dock, "openConfig");
	fflush(stdout);
	_Exit(0);
}
