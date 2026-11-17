// Idiomas: abre o painel e a ⚙ num idioma, troca na hora e lista textos que ficaram em português.
// Uso: REPLAYDECK_LANG_DIR=<repo>/data/lang tools/ui-test.sh test/ui/i18n.cpp en
#include "engine.hpp"
#include "replay-dock.hpp"
#include "setup-wizard.hpp"
#include "i18n.hpp"
#include <obs-module.h>
#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QGroupBox>
#include <QLabel>
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

static int g_left = 0;
/* texto ainda em português: tem acento típico ou palavra comum do pt */
static void check(const QString &where, const QString &t)
{
	static const char *words[] = {" de ", " da ", " do ", " para ", " com ", "ção", "ões", "ã", "õ", "ê", "ç", "Câmera", "Gravar",
				      "GRAVAR", "Evento", "Saída", "SAÍDA", "Pasta", "Configurar", "Salvar", nullptr};
	if (t.trimmed().isEmpty())
		return;
	for (int i = 0; words[i]; i++)
		if (t.contains(QString::fromUtf8(words[i]))) {
			printf("  PT? [%s] %s\n", where.toUtf8().constData(), t.left(120).replace('\n', ' ').toUtf8().constData());
			g_left++;
			return;
		}
}
static void scan(QWidget *root, const QString &where)
{
	QList<QWidget *> all = root->findChildren<QWidget *>();
	all.prepend(root);
	for (QWidget *w : all) {
		if (!w->isVisibleTo(root) && w != root)
			continue;
		check(where + " tip", w->toolTip());
		if (auto *l = qobject_cast<QLabel *>(w)) check(where, l->text());
		if (auto *b = qobject_cast<QAbstractButton *>(w)) check(where, b->text());
		if (auto *g = qobject_cast<QGroupBox *>(w)) check(where, g->title());
		if (auto *c = qobject_cast<QComboBox *>(w))
			for (int i = 0; i < c->count(); i++) check(where + " combo", c->itemText(i));
		if (auto *t = qobject_cast<QTabWidget *>(w))
			for (int i = 0; i < t->count(); i++) check(where + " tab", t->tabText(i));
	}
}

static void shootConfig(const char *tag)
{
	QTimer::singleShot(300, [tag] {
		for (QWidget *w : QApplication::topLevelWidgets())
			if (strcmp(w->metaObject()->className(), "QDialog") == 0 && w->isVisible()) {
				auto *tabs = w->findChild<QTabWidget *>();
				for (int t = 0; tabs && t < tabs->count(); t++) {
					tabs->setCurrentIndex(t);
					pump(8);
					w->grab().save(QString("/tmp/wiz/i18n-%1-cfg%2.png").arg(tag).arg(t));
					scan(tabs->widget(t), QString("cfg/%1").arg(tabs->tabText(t)));
				}
				scan(w, "cfg");
				QMetaObject::invokeMethod(w, "reject");
			}
	});
}

int main(int argc, char **argv)
{
	const char *lang = argc > 1 ? argv[1] : "en";
	QApplication app(argc, argv);
	app.setStyle(QStyleFactory::create("Fusion"));
	obs_startup("pt-BR", nullptr, nullptr);
	obs_video_info ovi = {};
	ovi.graphics_module = "libobs-opengl";
	ovi.fps_num = 30;
	ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = 1280;
	ovi.base_height = ovi.output_height = 720;
	ovi.output_format = VIDEO_FORMAT_NV12;
	obs_reset_video(&ovi);
	registerReplaySource();
	registerReplayCaptureOutput();
	engineCreate();
	i18n::setLanguage("pt-BR");

	auto *d = new ReplayDock();
	d->resize(560, 420);
	d->show();
	pump();
	d->grab().save("/tmp/wiz/i18n-pt-dock.png");

	/* troca com o painel aberto */
	i18n::setLanguage(lang);
	pump();
	d->grab().save(QString("/tmp/wiz/i18n-%1-dock.png").arg(lang));
	printf("== painel depois de trocar para %s (sem reabrir):\n", lang);
	scan(d, "painel");

	shootConfig(lang);
	QMetaObject::invokeMethod(d, "openConfig");
	pump();

	/* configuração rápida */
	QTimer::singleShot(400, [lang] {
		for (QWidget *w : QApplication::topLevelWidgets())
			if (strcmp(w->metaObject()->className(), "QDialog") == 0 && w->isVisible()) {
				w->grab().save(QString("/tmp/wiz/i18n-%1-wizard.png").arg(lang));
				scan(w, "wizard");
				QMetaObject::invokeMethod(w, "reject");
			}
	});
	setup::showWizard(d);
	pump();
	printf("== %d textos suspeitos em português\n", g_left);
	fflush(stdout);
	_Exit(0);
}
