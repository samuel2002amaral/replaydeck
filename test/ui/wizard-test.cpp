#include "../../home/claude/obs-instant-replay/src/engine.hpp"
#include "../../home/claude/obs-instant-replay/src/overlay-channels.hpp"
#include "../../home/claude/obs-instant-replay/src/setup-wizard.hpp"
#include "../../home/claude/obs-instant-replay/src/gt/gt-source.hpp"
#include <obs-module.h>
#include <QApplication>
#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStyleFactory>
#include <QPalette>
#include <QTimer>
#include <cstdio>
OBS_DECLARE_MODULE()
size_t pluginHotkeyCount() { return 0; }
const char *pluginHotkeyDesc(size_t) { return ""; }
void pluginRunHotkey(size_t) {}
void registerReplaySource();
static const char *fn(void *) { return "fake"; }
static void *fc(obs_data_t *, obs_source_t *s) { return s; }
static void fd(void *) {}
static uint32_t fw(void *) { return 1920; }
static uint32_t fh(void *) { return 1080; }
static void reg(const char *id) {
	static std::vector<obs_source_info> infos; infos.reserve(4);
	obs_source_info i = {}; i.id = id; i.type = OBS_SOURCE_TYPE_INPUT; i.output_flags = OBS_SOURCE_VIDEO;
	i.get_name = fn; i.create = fc; i.destroy = fd; i.get_width = fw; i.get_height = fh;
	obs_register_source(&i);
}
int main(int argc, char **argv) {
	QApplication app(argc, argv);
	app.setStyle(QStyleFactory::create("Fusion"));
	QPalette pal; pal.setColor(QPalette::Window, QColor("#1d1f26")); pal.setColor(QPalette::WindowText, QColor("#e0e0e0"));
	pal.setColor(QPalette::Base, QColor("#16181d")); pal.setColor(QPalette::Text, QColor("#e0e0e0"));
	pal.setColor(QPalette::Button, QColor("#3b3e4a")); pal.setColor(QPalette::ButtonText, QColor("#e0e0e0")); app.setPalette(pal);
	obs_startup("en-US", nullptr, nullptr);
	obs_video_info ovi = {}; ovi.graphics_module = "libobs-opengl"; ovi.fps_num = 30; ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = 1920; ovi.base_height = ovi.output_height = 1080; ovi.output_format = VIDEO_FORMAT_NV12;
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS) return 1;
	registerReplaySource(); registerReplayCaptureOutput(); reg("dshow_input"); reg("image_source");
	engineCreate();
	const char *cams[] = {"Câmera Principal", "Câmera Gol Esquerdo", "Câmera Gol Direito", "Câmera Banco"};
	(void)cams;
	obs_source_create("image_source", "Logo Patrocinador", nullptr, nullptr);
	obs_scene_create("Jogo");
	signal_handler_connect(obs_get_signal_handler(), "source_create", [](void *, calldata_t *cd) {
		obs_source_t *s = (obs_source_t *)calldata_ptr(cd, "source");
		if (obs_source_is_scene(s)) obs_source_get_ref(s); /* como o OBS faz ao pôr na lista de cenas */
	}, nullptr);
	printf("needsSetup=%d\n", setup::needsSetup());
	auto top = [](const char *title) -> QWidget * {
		for (QWidget *w : QApplication::topLevelWidgets())
			if (w->isVisible() && w->windowTitle().contains(QString::fromUtf8(title))) return w;
		return nullptr; };
	auto click = [](QWidget *m, const char *t) {
		for (QPushButton *b : m->findChildren<QPushButton *>()) if (b->text().startsWith(QString::fromUtf8(t))) { b->click(); return; } };
	QTimer::singleShot(500, [=] {
		QWidget *m = top("configuração rápida");
		m->findChild<QLineEdit *>()->setText("/tmp/wiz/gravacoes");
		for (int i = 0; i < 10; i++) QApplication::processEvents();
		m->grab().save("/tmp/wiz/w1.png");
		QTimer::singleShot(400, [=] {
			QWidget *a = top("Adicionar câmera");
			if (!a) { printf("sem add\n"); return; }
			a->grab().save("/tmp/wiz/add.png");
			click(a, "Criar câmera");
			QTimer::singleShot(400, [=] {
				QWidget *m2 = top("configuração rápida");
				m2->grab().save("/tmp/wiz/w2.png");
				auto *l = m2->findChild<QListWidget *>();
				for (int r = 0; r < l->count(); r++) printf("item: %s\n", l->item(r)->text().toUtf8().constData());
				click(m2, "Concluir");
			});
		});
		click(m, "+ Adicionar");
	});
	setup::Result r = setup::showWizard(nullptr);
	ReplayConfig c = engine().config();
	printf("result=%d dir=%s cq=%d\n", r, c.outputDir.c_str(), c.rec.cq);
	for (int i = 0; i < 8; i++) if (!c.cameras[i].empty()) printf("cam%d=%s\n", i + 1, c.cameras[i].c_str());
	obs_source_t *sc = obs_get_source_by_name("Replay");
	printf("cena Replay=%s\n", sc ? "SIM" : "NAO");
	if (sc) {
		obs_scene_enum_items(obs_scene_from_source(sc), [](obs_scene_t *, obs_sceneitem_t *it, void *) {
			obs_source_t *s = obs_sceneitem_get_source(it); obs_data_t *st = obs_source_get_settings(s);
			vec2 b; obs_sceneitem_get_bounds(it, &b);
			printf("  item %s id=%s canal=%lld bounds=%.0fx%.0f tipo=%d\n", obs_source_get_name(s), obs_source_get_id(s), (long long)obs_data_get_int(st, "channel"), b.x, b.y, (int)obs_sceneitem_get_bounds_type(it));
			obs_data_release(st); return true; }, nullptr);
	}
	printf("needsSetup depois=%d\n", setup::needsSetup());
	fflush(stdout); _Exit(0);
}
