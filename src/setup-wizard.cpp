#include "setup-wizard.hpp"
#include "i18n.hpp"
#include "engine.hpp"
#include "ui-theme.hpp"
#include "common.hpp"
#include <obs-frontend-api.h>
#include <obs.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QEventLoop>
#include <QFormLayout>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QVBoxLayout>
#include <cstring>
#include <algorithm>
#include <vector>

#define REPLAY_ID "instant_replay_output"

namespace setup {

/* ---------------- cena Replay automática ---------------- */

static bool itemHasChannelA(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	bool *found = static_cast<bool *>(param);
	obs_source_t *s = obs_sceneitem_get_source(item);
	if (s && strcmp(obs_source_get_id(s), REPLAY_ID) == 0) {
		obs_data_t *st = obs_source_get_settings(s);
		if (obs_data_get_int(st, "channel") == 0)
			*found = true;
		obs_data_release(st);
	}
	if (!*found && obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, itemHasChannelA, param);
	return !*found;
}

static bool anySceneHasChannelA()
{
	struct obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	bool found = false;
	for (size_t i = 0; i < scenes.sources.num && !found; i++)
		obs_scene_enum_items(obs_scene_from_source(scenes.sources.array[i]), itemHasChannelA, &found);
	obs_frontend_source_list_free(&scenes);
	return found;
}

/* fonte de replay do canal A que já exista (ex.: a pessoa tirou da cena), para reaproveitar */
static bool findChannelASource(void *param, obs_source_t *s)
{
	if (strcmp(obs_source_get_id(s), REPLAY_ID) != 0)
		return true;
	obs_data_t *st = obs_source_get_settings(s);
	const bool a = obs_data_get_int(st, "channel") == 0;
	obs_data_release(st);
	if (a) {
		*static_cast<obs_source_t **>(param) = obs_source_get_ref(s);
		return false;
	}
	return true;
}

static std::string freeName(const char *base)
{
	std::string name = base;
	for (int n = 2;; n++) {
		obs_source_t *s = obs_get_source_by_name(name.c_str());
		if (!s)
			return name;
		obs_source_release(s);
		name = std::string(base) + " " + std::to_string(n);
	}
}

QString ensureReplayScene()
{
	if (anySceneHasChannelA())
		return QString();

	obs_source_t *src = nullptr;
	obs_enum_sources(findChannelASource, &src);
	if (!src) {
		obs_data_t *st = obs_data_create();
		obs_data_set_int(st, "channel", 0);
		src = obs_source_create(REPLAY_ID, freeName("Replay A").c_str(), st, nullptr);
		obs_data_release(st);
	}
	if (!src)
		return QString();

	const std::string sceneName = freeName("Replay");
	obs_scene_t *scene = obs_scene_create(sceneName.c_str());
	if (!scene) {
		obs_source_release(src);
		return QString();
	}
	obs_sceneitem_t *item = obs_scene_add(scene, src);
	if (item) {
		/* ocupa a tela toda, mantendo a proporção */
		struct obs_video_info ovi = {};
		obs_get_video_info(&ovi);
		struct vec2 pos = {0.0f, 0.0f};
		struct vec2 bounds = {(float)ovi.base_width, (float)ovi.base_height};
		obs_sceneitem_set_pos(item, &pos);
		obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
		obs_sceneitem_set_bounds_alignment(item, OBS_ALIGN_CENTER);
		obs_sceneitem_set_bounds(item, &bounds);
	}
	obs_scene_release(scene);
	obs_source_release(src);
	rlog(LOG_INFO, "cena \"%s\" criada com o replay (canal A)", sceneName.c_str());
	return QString::fromUtf8(sceneName.c_str());
}

/* ---------------- câmera nova ---------------- */

struct CamType {
	const char *id;
	const char *fallbackLabel;
	bool openProps; /* abre as propriedades do OBS para escolher o dispositivo */
};

static const CamType kCamTypes[] = {
	{"dshow_input", N_("Dispositivo de captura de vídeo (webcam, placa de captura)"), true},
	{"v4l2_input", N_("Dispositivo de captura de vídeo (V4L2)"), true},
	{"av_capture_input_v2", N_("Dispositivo de captura de vídeo"), true},
	{"av_capture_input", N_("Dispositivo de captura de vídeo"), true},
	{"decklink-input", N_("Blackmagic DeckLink"), true},
	{"aja_source", N_("AJA"), true},
	{"ndi_source", N_("NDI"), true},
	{"ffmpeg_source", N_("Câmera IP / SRT / RTMP (endereço)"), false},
	/* outras fontes de vídeo que fazem sentido como ângulo de replay */
	{"game_capture", N_("Captura de jogo (eSports)"), true},
	{"monitor_capture", N_("Captura de monitor"), true},
	{"window_capture", N_("Captura de janela"), true},
	{"screen_capture", N_("Captura de tela"), true},
	{"xshm_input_v2", N_("Captura de tela"), true},
	{"pipewire-screen-capture-source", N_("Captura de tela"), true},
	{"ffmpeg_source#file", N_("Mídia (arquivo de vídeo — bom para testar)"), true},
};

static bool typeRegistered(const char *id)
{
	const char *t = nullptr;
	for (size_t i = 0; obs_enum_input_types(i, &t); i++)
		if (strcmp(t, id) == 0)
			return true;
	return false;
}

static void addFullScreen(obs_scene_t *scene, obs_source_t *src)
{
	obs_sceneitem_t *item = obs_scene_add(scene, src);
	if (!item)
		return;
	struct obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	struct vec2 pos = {0.0f, 0.0f};
	struct vec2 bounds = {(float)ovi.base_width, (float)ovi.base_height};
	obs_sceneitem_set_pos(item, &pos);
	obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
	obs_sceneitem_set_bounds_alignment(item, OBS_ALIGN_CENTER);
	obs_sceneitem_set_bounds(item, &bounds);
}

static QString nextCameraName()
{
	for (int n = 1;; n++) {
		const std::string name = tl("Câmera %1").arg(n).toUtf8().constData();
		obs_source_t *s = obs_get_source_by_name(name.c_str());
		if (!s)
			return QString::fromUtf8(name.c_str());
		obs_source_release(s);
	}
}

QString addCameraDialog(QWidget *parent)
{
	QDialog dlg(parent);
	dlg.setWindowTitle(tl("Adicionar câmera"));
	dlg.setMinimumWidth(460);
	ui::applyDialog(&dlg);
	auto *v = new QVBoxLayout(&dlg);
	v->setContentsMargins(16, 14, 16, 12);
	auto *hint = new QLabel(tl("Cria a fonte da câmera e uma cena só com ela. Depois o OBS abre as "
						  "propriedades para você escolher o aparelho."),
				&dlg);
	hint->setProperty("role", "muted");
	hint->setWordWrap(true);
	v->addWidget(hint);
	auto *f = new QFormLayout();
	f->setVerticalSpacing(10);
	v->addLayout(f);

	auto *type = new QComboBox(&dlg);
	bool sep = false;
	for (const CamType &t : kCamTypes) {
		const QString key = QString::fromLatin1(t.id);
		const QByteArray baseId = key.section('#', 0, 0).toLatin1();
		if (!typeRegistered(baseId.constData()))
			continue;
		const bool other = strcmp(t.id, "game_capture") == 0 || strcmp(t.id, "monitor_capture") == 0 ||
				   strcmp(t.id, "window_capture") == 0 || strcmp(t.id, "screen_capture") == 0 ||
				   strcmp(t.id, "xshm_input_v2") == 0 || strcmp(t.id, "pipewire-screen-capture-source") == 0 ||
				   key.endsWith("#file");
		if (other && !sep) {
			type->insertSeparator(type->count());
			sep = true;
		}
		QString label = tl(t.fallbackLabel);
		const bool keepOwn = key.startsWith("ffmpeg_source") || key == "dshow_input" || key == "game_capture";
		if (!keepOwn) {
			const char *dn = obs_source_get_display_name(baseId.constData());
			if (dn && *dn)
				label = QString::fromUtf8(dn);
		}
		type->addItem(label, key);
	}
	if (type->count() == 0) {
		QMessageBox::warning(parent, "ReplayDeck", tl("Nenhum tipo de câmera disponível neste OBS."));
		return QString();
	}
	/* qualquer outra fonte de vídeo do OBS (navegador, texto, imagem, plugins...) */
	auto *other = new QComboBox(&dlg);
	{
		struct Item {
			QString label, id;
		};
		std::vector<Item> items;
		const char *t = nullptr;
		for (size_t i = 0; obs_enum_input_types(i, &t); i++) {
			const uint32_t fl = obs_get_source_output_flags(t);
			if (!(fl & OBS_SOURCE_VIDEO) || (fl & OBS_SOURCE_CAP_DISABLED) || (fl & OBS_SOURCE_DEPRECATED))
				continue;
			if (strncmp(t, "instant_replay", 14) == 0 || strncmp(t, "gt_", 3) == 0 || strstr(t, "instant-replay"))
				continue;
			bool listed = false;
			for (const CamType &c : kCamTypes)
				listed |= QString::fromLatin1(c.id).section('#', 0, 0) == QLatin1String(t);
			if (listed)
				continue;
			const char *dn = obs_source_get_display_name(t);
			items.push_back({QString::fromUtf8(dn && *dn ? dn : t), QString::fromLatin1(t)});
		}
		std::sort(items.begin(), items.end(),
			  [](const Item &a, const Item &b) { return a.label.localeAwareCompare(b.label) < 0; });
		for (const Item &it : items)
			other->addItem(it.label, it.id);
		if (!items.empty()) {
			type->insertSeparator(type->count());
			type->addItem(tl("Outra fonte do OBS…"), QString::fromLatin1("__other"));
		}
	}
	f->addRow(tl("Tipo"), type);
	f->addRow(tl("Fonte"), other);
	auto *name = new QLineEdit(nextCameraName(), &dlg);
	f->addRow(tl("Nome"), name);
	auto *url = new QLineEdit(&dlg);
	url->setPlaceholderText(tl("rtsp://usuario:senha@192.168.0.50:554/stream  ou  srt://..."));
	f->addRow(tl("Endereço"), url);
	auto *mkScene = new QCheckBox(tl("Criar também uma cena com esta câmera"), &dlg);
	mkScene->setChecked(true);
	f->addRow(QString(), mkScene);
	auto updUrl = [&] {
		const bool ip = type->currentData().toString() == "ffmpeg_source";
		url->setVisible(ip);
		f->labelForField(url)->setVisible(ip);
		const bool oth = type->currentData().toString() == "__other";
		other->setVisible(oth);
		f->labelForField(other)->setVisible(oth);
	};
	updUrl();
	QObject::connect(type, &QComboBox::currentIndexChanged, &dlg, [&] { updUrl(); });

	auto *row = new QHBoxLayout();
	auto *cancel = new QPushButton(tl("Cancelar"), &dlg);
	auto *ok = new QPushButton(tl("Criar câmera"), &dlg);
	ui::setKind(ok, "primary");
	ok->setDefault(true);
	row->addStretch(1);
	row->addWidget(cancel);
	row->addWidget(ok);
	v->addLayout(row);
	QObject::connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
	QObject::connect(ok, &QPushButton::clicked, &dlg, [&] {
		const QString n = name->text().trimmed();
		if (n.isEmpty())
			return;
		obs_source_t *ex = obs_get_source_by_name(n.toUtf8().constData());
		if (ex) {
			obs_source_release(ex);
			QMessageBox::information(&dlg, "ReplayDeck", tl("Já existe uma fonte ou cena com esse nome."));
			return;
		}
		if (type->currentData().toString() == "ffmpeg_source" && url->text().trimmed().isEmpty()) {
			QMessageBox::information(&dlg, "ReplayDeck", tl("Informe o endereço da câmera."));
			return;
		}
		dlg.accept();
	});
	if (dlg.exec() != QDialog::Accepted)
		return QString();

	QString key = type->currentData().toString();
	if (key == "__other")
		key = other->currentData().toString();
	const std::string id = key.section('#', 0, 0).toUtf8().constData();
	const bool mediaFile = key.endsWith("#file");
	const std::string srcName = name->text().trimmed().toUtf8().constData();
	obs_data_t *st = obs_data_create();
	bool openProps = true;
	if (mediaFile) {
		obs_data_set_bool(st, "is_local_file", true);
		obs_data_set_bool(st, "looping", true);
		obs_data_set_bool(st, "restart_on_activate", false);
		obs_data_set_bool(st, "close_when_inactive", false);
	} else if (id == "ffmpeg_source") {
		obs_data_set_bool(st, "is_local_file", false);
		obs_data_set_string(st, "input", url->text().trimmed().toUtf8().constData());
		obs_data_set_bool(st, "restart_on_activate", false);
		obs_data_set_bool(st, "close_when_inactive", false);
		obs_data_set_int(st, "buffering_mb", 0);
		obs_data_set_int(st, "reconnect_delay_sec", 2);
		openProps = false;
	}
	obs_source_t *src = obs_source_create(id.c_str(), srcName.c_str(), st, nullptr);
	obs_data_release(st);
	if (!src) {
		QMessageBox::warning(parent, "ReplayDeck", tl("Não foi possível criar a câmera."));
		return QString();
	}
	if (mkScene->isChecked()) {
		const std::string scName = freeName((tl("Cena ").toStdString() + srcName).c_str());
		obs_scene_t *scene = obs_scene_create(scName.c_str());
		if (scene) {
			addFullScreen(scene, src);
			obs_scene_release(scene);
		}
	} else {
		/* sem cena a fonte não fica salva no OBS: põe na cena atual */
		obs_source_t *cur = obs_frontend_get_current_scene();
		if (cur) {
			addFullScreen(obs_scene_from_source(cur), src);
			obs_source_release(cur);
		}
	}
	rlog(LOG_INFO, "câmera \"%s\" (%s) criada", srcName.c_str(), id.c_str());
	if (openProps)
		obs_frontend_open_source_properties(src);
	obs_source_release(src);
	return QString::fromUtf8(srcName.c_str());
}

bool assignToFreeSlot(const QString &source)
{
	if (source.isEmpty() || engine().recording())
		return false;
	ReplayConfig cfg = engine().config();
	const std::string n = source.toUtf8().constData();
	for (int i = 0; i < MAX_CAMERAS; i++)
		if (cfg.cameras[i] == n)
			return true;
	for (int i = 0; i < MAX_CAMERAS; i++)
		if (cfg.cameras[i].empty()) {
			cfg.cameras[i] = n;
			engine().setConfig(cfg);
			return true;
		}
	return false;
}

/* ---------------- assistente ---------------- */


bool needsSetup()
{
	if (QSettings(ui::settingsFile(), QSettings::IniFormat).value("setupDone", false).toBool())
		return false;
	const ReplayConfig cfg = engine().config();
	for (int i = 0; i < MAX_CAMERAS; i++)
		if (!cfg.cameras[i].empty())
			return false;
	return true;
}

static bool looksLikeCamera(obs_source_t *s)
{
	const char *id = obs_source_get_id(s);
	static const char *ids[] = {"dshow_input",   "decklink-input",      "ndi_source", "v4l2_input",
				    "av_capture_input", "av_capture_input_v2", "aja_source", "teradek_source"};
	for (const char *c : ids)
		if (strcmp(id, c) == 0)
			return true;
	if (strcmp(id, "ffmpeg_source") == 0) { /* câmera IP / SRT / RTMP (não arquivo) */
		obs_data_t *st = obs_source_get_settings(s);
		const bool live = !obs_data_get_bool(st, "is_local_file");
		obs_data_release(st);
		return live;
	}
	return false;
}

struct Cand {
	QString name;
	bool camera;
	bool scene;
};

static bool collectSource(void *param, obs_source_t *s)
{
	auto *v = static_cast<std::vector<Cand> *>(param);
	if (!(obs_source_get_output_flags(s) & OBS_SOURCE_VIDEO))
		return true;
	const char *id = obs_source_get_id(s);
	if (strcmp(id, REPLAY_ID) == 0 || strncmp(id, "instant_replay", 14) == 0 || strncmp(id, "gt_", 3) == 0)
		return true;
	v->push_back({QString::fromUtf8(obs_source_get_name(s)), looksLikeCamera(s), false});
	return true;
}

static bool collectScene(void *param, obs_source_t *s)
{
	static_cast<std::vector<Cand> *>(param)->push_back({QString::fromUtf8(obs_source_get_name(s)), false, true});
	return true;
}

static QString freeText(const QString &dir)
{
	QString d = dir;
	while (!d.isEmpty() && !QDir(d).exists())
		d = QFileInfo(d).absolutePath() == d ? QString() : QFileInfo(d).absolutePath();
	if (d.isEmpty())
		return QString();
	const QStorageInfo si(d);
	if (!si.isValid())
		return QString();
	return tl("%1 GB livres neste disco").arg(si.bytesAvailable() / 1e9, 0, 'f', 0);
}

static bool g_wizardOpen = false;

static Result showWizardOnce(QWidget *parent, bool &reopen)
{
	reopen = false;
	if (g_wizardOpen)
		return Cancelled;
	if (engine().recording()) {
		QMessageBox::information(parent, "ReplayDeck", tl("Pare a gravação (●) antes de reconfigurar as câmeras."));
		return Cancelled;
	}
	ReplayConfig cfg = engine().config();
	g_wizardOpen = true;
	struct Reset {
		~Reset() { g_wizardOpen = false; }
	} reset;

	QDialog dlg(parent);
	dlg.setWindowTitle(tl("ReplayDeck — configuração rápida"));
	dlg.setMinimumSize(560, 600);
	ui::applyDialog(&dlg);
	auto *v = new QVBoxLayout(&dlg);
	v->setContentsMargins(18, 16, 18, 14);
	v->setSpacing(8);

	auto *title = new QLabel(tl("<span style='font-size:17px;font-weight:700'>Vamos deixar o replay pronto</span>"),
				 &dlg);
	{
		/* idioma logo no primeiro contato: troca e reabre a janela no idioma escolhido */
		auto *head = new QHBoxLayout();
		head->addWidget(title, 1);
		auto *langBox = new QComboBox(&dlg);
		langBox->setToolTip(tl("Idioma do plugin"));
		langBox->addItem(QString::fromUtf8("🌐  ") + tl("Automático (idioma do OBS)"), "auto");
		for (const i18n::Lang &lg : i18n::languages())
			langBox->addItem(QString::fromUtf8("🌐  ") + QString::fromUtf8(lg.name), QString::fromLatin1(lg.code));
		const int li = langBox->findData(i18n::setting());
		langBox->setCurrentIndex(li < 0 ? 0 : li);
		head->addWidget(langBox, 0, Qt::AlignTop);
		v->addLayout(head);
		QObject::connect(langBox, &QComboBox::currentIndexChanged, &dlg, [&, langBox](int) {
			i18n::setLanguage(langBox->currentData().toString());
			reopen = true;
			dlg.reject();
		});
	}
	auto *intro = new QLabel(tl("Três escolhas e pronto. Dá para mudar tudo depois em ⚙ Configurar."), &dlg);
	intro->setProperty("role", "muted");
	v->addWidget(intro);

	auto step = [&](const QString &n, const QString &t, const QString &hint) {
		auto *l = new QLabel(tl("<b>%1&nbsp;&nbsp;%2</b>").arg(n, t), &dlg);
		l->setContentsMargins(0, 10, 0, 0);
		v->addWidget(l);
		if (!hint.isEmpty()) {
			auto *h = new QLabel(hint, &dlg);
			h->setProperty("role", "muted");
			h->setWordWrap(true);
			v->addWidget(h);
		}
	};

	/* ---- 1. câmeras ---- */
	step("1", tl("Quais câmeras gravar?"),
	     tl("Marque as fontes que o replay deve gravar (até %1). "
			       "Arraste para mudar a ordem: a primeira marcada vira a câmera 1.")
		     .arg(MAX_CAMERAS));
	std::vector<Cand> cands;
	obs_enum_sources(collectSource, &cands);
	obs_enum_scenes(collectScene, &cands);
	bool anyConfigured = false;
	for (int i = 0; i < MAX_CAMERAS; i++)
		anyConfigured |= !cfg.cameras[i].empty();

	auto *list = new QListWidget(&dlg);
	list->setDragDropMode(QAbstractItemView::InternalMove);
	list->setDefaultDropAction(Qt::MoveAction);
	list->setSelectionMode(QAbstractItemView::SingleSelection);
	list->setMinimumHeight(200);
	list->setStyleSheet("QListWidget::item { padding: 5px 4px; }");
	auto addItem = [&](const Cand &c, bool checked) {
		auto *it = new QListWidgetItem(list);
		it->setData(Qt::UserRole, c.name);
		it->setData(Qt::UserRole + 1, c.scene ? tl("  (cena)") : c.camera ? tl("  · câmera") : QString());
		it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled);
		it->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
	};
	/* primeiro as já configuradas (na ordem), depois câmeras, fontes e cenas */
	QStringList used;
	for (int i = 0; i < MAX_CAMERAS; i++) {
		const QString n = QString::fromUtf8(cfg.cameras[i].c_str());
		if (n.isEmpty() || used.contains(n))
			continue;
		for (const Cand &c : cands)
			if (c.name == n) {
				addItem(c, true);
				used << n;
			}
	}
	int autoChecked = 0;
	for (int pass = 0; pass < 3; pass++)
		for (const Cand &c : cands) {
			if (used.contains(c.name))
				continue;
			const bool inPass = pass == 0 ? c.camera : pass == 1 ? (!c.camera && !c.scene) : c.scene;
			if (!inPass)
				continue;
			const bool check = !anyConfigured && c.camera && autoChecked < MAX_CAMERAS;
			autoChecked += check;
			addItem(c, check);
			used << c.name;
		}
	v->addWidget(list, 1);
	auto *camInfo = new QLabel(&dlg);
	camInfo->setProperty("role", "muted");
	auto *addCam = new QPushButton(tl("+ Adicionar câmera"), &dlg);
	addCam->setToolTip(tl("A câmera ainda não está no OBS? Crie aqui a fonte e a cena dela"));
	auto *camRow = new QHBoxLayout();
	camRow->addWidget(camInfo, 1);
	camRow->addWidget(addCam);
	v->addLayout(camRow);
	if (cands.empty()) {
		auto *none = new QListWidgetItem(tl("Nenhuma fonte de vídeo ainda. Clique em \"+ Adicionar câmera\"."),
						 list);
		none->setFlags(Qt::NoItemFlags);
	}

	bool relabeling = false;
	auto relabel = [&] {
		if (relabeling)
			return;
		relabeling = true;
		int n = 0;
		for (int r = 0; r < list->count(); r++) {
			QListWidgetItem *it = list->item(r);
			const QString name = it->data(Qt::UserRole).toString();
			if (name.isEmpty())
				continue;
			if (it->checkState() == Qt::Checked && n >= MAX_CAMERAS)
				it->setCheckState(Qt::Unchecked);
			const bool on = it->checkState() == Qt::Checked;
			if (on)
				n++;
			it->setText((on ? tl("Câmera %1  —  ").arg(n) : QString()) + name +
				    it->data(Qt::UserRole + 1).toString());
		}
		camInfo->setText(n == 0 ? tl("Nenhuma câmera marcada.")
					: tl("%1 câmera(s) marcada(s).").arg(n));
		relabeling = false;
	};
	relabel();
	QObject::connect(addCam, &QPushButton::clicked, &dlg, [&] {
		const QString n = addCameraDialog(&dlg);
		if (n.isEmpty())
			return;
		/* tira o aviso de "nenhuma fonte" e põe a câmera nova marcada, logo após as marcadas */
		for (int r = list->count() - 1; r >= 0; r--)
			if (list->item(r)->data(Qt::UserRole).toString().isEmpty())
				delete list->takeItem(r);
		int pos = 0;
		while (pos < list->count() && list->item(pos)->checkState() == Qt::Checked)
			pos++;
		auto *it = new QListWidgetItem();
		it->setData(Qt::UserRole, n);
		it->setData(Qt::UserRole + 1, tl("  · câmera nova"));
		it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled);
		relabeling = true;
		list->insertItem(pos, it);
		it->setCheckState(Qt::Checked);
		relabeling = false;
		relabel();
		list->scrollToItem(it);
	});
	QObject::connect(list, &QListWidget::itemChanged, &dlg, [&](QListWidgetItem *) { relabel(); });
	QObject::connect(list->model(), &QAbstractItemModel::rowsMoved, &dlg, [&] { relabel(); });

	/* ---- 2. pasta ---- */
	step("2", tl("Onde salvar as gravações?"),
	     tl("Use o disco mais rápido e com mais espaço (de preferência SSD)."));
	QString dir = QString::fromUtf8(cfg.outputDir.c_str());
	if (dir.isEmpty())
		dir = QDir(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)).filePath("ReplayDeck");
	auto *dirEdit = new QLineEdit(QDir::toNativeSeparators(dir), &dlg);
	auto *browse = new QPushButton(tl("Escolher…"), &dlg);
	auto *dirRow = new QHBoxLayout();
	dirRow->addWidget(dirEdit, 1);
	dirRow->addWidget(browse);
	v->addLayout(dirRow);
	auto *freeLbl = new QLabel(&dlg);
	freeLbl->setProperty("role", "muted");
	v->addWidget(freeLbl);
	auto updFree = [&] { freeLbl->setText(freeText(QDir::fromNativeSeparators(dirEdit->text().trimmed()))); };
	updFree();
	QObject::connect(dirEdit, &QLineEdit::textChanged, &dlg, [&] { updFree(); });
	QObject::connect(browse, &QPushButton::clicked, &dlg, [&] {
		const QString d = QFileDialog::getExistingDirectory(&dlg, tl("Pasta das gravações"), dirEdit->text());
		if (!d.isEmpty())
			dirEdit->setText(QDir::toNativeSeparators(d));
	});

	/* ---- 3. qualidade ---- */
	step("3", tl("Qualidade"), QString());
	auto *quality = new QComboBox(&dlg);
	quality->addItem(tl("Alta  ★ recomendada"), 18);
	quality->addItem(tl("Máxima  (usa mais disco)"), 14);
	quality->addItem(tl("Leve  (economiza disco, PC mais simples)"), 22);
	if (cfg.rec.cq > 0 && quality->findData(cfg.rec.cq) >= 0)
		quality->setCurrentIndex(quality->findData(cfg.rec.cq));
	v->addWidget(quality);

	/* ---- finalização ---- */
	auto *mkScene = new QCheckBox(tl("Criar a cena \"Replay\" com o replay já posicionado (canal A)"), &dlg);
	mkScene->setChecked(true);
	if (anySceneHasChannelA()) {
		mkScene->setChecked(false);
		mkScene->setEnabled(false);
		mkScene->setText(tl("Cena com o replay: já existe ✓"));
	}
	auto *startRec = new QCheckBox(tl("Começar a gravar agora"), &dlg);
	startRec->setChecked(true);
	auto *opts = new QVBoxLayout();
	opts->setContentsMargins(0, 12, 0, 4);
	opts->addWidget(mkScene);
	opts->addWidget(startRec);
	v->addLayout(opts);

	auto *row = new QHBoxLayout();
	auto *later = new QPushButton(tl("Agora não"), &dlg);
	auto *done = new QPushButton(tl("Concluir ✓"), &dlg);
	ui::setKind(done, "primary");
	done->setDefault(true);
	row->addWidget(later);
	row->addStretch(1);
	row->addWidget(done);
	v->addLayout(row);
	QObject::connect(later, &QPushButton::clicked, &dlg, &QDialog::reject);
	QObject::connect(done, &QPushButton::clicked, &dlg, [&] {
		int n = 0;
		for (int r = 0; r < list->count(); r++)
			if (list->item(r)->checkState() == Qt::Checked)
				n++;
		if (n == 0) {
			QMessageBox::information(&dlg, "ReplayDeck", tl("Marque pelo menos uma câmera."));
			return;
		}
		const QString d = QDir::fromNativeSeparators(dirEdit->text().trimmed());
		if (d.isEmpty() || !QDir().mkpath(d)) {
			QMessageBox::warning(&dlg, "ReplayDeck",
					     tl("Não foi possível usar essa pasta. Escolha outra."));
			return;
		}
		dlg.accept();
	});

	/* não modal: deixa usar a janela de propriedades do OBS que abre ao criar câmera */
	dlg.setWindowModality(Qt::NonModal);
	dlg.show();
	{
		QEventLoop loop;
		QObject::connect(&dlg, &QDialog::finished, &loop, &QEventLoop::quit);
		loop.exec();
	}
	const int r = dlg.result();
	if (reopen)
		return Cancelled;
	QSettings(ui::settingsFile(), QSettings::IniFormat).setValue("setupDone", true); /* não pergunta de novo sozinho */
	if (r != QDialog::Accepted)
		return Cancelled;

	int n = 0;
	for (int i = 0; i < MAX_CAMERAS; i++)
		cfg.cameras[i].clear();
	for (int row_ = 0; row_ < list->count() && n < MAX_CAMERAS; row_++) {
		QListWidgetItem *it = list->item(row_);
		if (it->checkState() == Qt::Checked)
			cfg.cameras[n++] = it->data(Qt::UserRole).toString().toUtf8().constData();
	}
	cfg.outputDir = QDir::fromNativeSeparators(dirEdit->text().trimmed()).toUtf8().constData();
	cfg.rec.cq = quality->currentData().toInt();
	cfg.rec.bitrateMbps = cfg.rec.cq <= 14 ? 220 : cfg.rec.cq <= 18 ? 144 : 80;
	cfg.rec.mjpegQuality = cfg.rec.cq <= 14 ? 2 : cfg.rec.cq <= 18 ? 3 : 4;
	engine().setConfig(cfg);

	if (mkScene->isChecked())
		ensureReplayScene();
	return startRec->isChecked() ? DoneStartRecording : Done;
}

Result showWizard(QWidget *parent)
{
	bool reopen = true;
	Result r = Cancelled;
	for (int guard = 0; reopen && guard < 20; guard++)
		r = showWizardOnce(parent, reopen);
	return r;
}

} // namespace setup
