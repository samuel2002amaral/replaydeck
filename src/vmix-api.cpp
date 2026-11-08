#include "vmix-api.hpp"
#include "i18n.hpp"
#include "air-control.hpp"
#include "common.hpp"
#include "engine.hpp"
#include "overlay-channels.hpp"
#include "replay-dock.hpp"
#include "gt/gt-doc.hpp"
#include "gt/gt-source.hpp"
#include <obs-frontend-api.h>
#include <QColor>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <cmath>
#include <cstring>
#include <vector>

/* funções do plugin-main (lista de atalhos) */
size_t pluginHotkeyCount();
const char *pluginHotkeyDesc(size_t i);
void pluginRunHotkey(size_t i);

static VmixApi *g_api = nullptr;

/* ======================= inputs ======================= */

enum class Kind { Scene, Gt, Text, Image, Media, Source, Replay };

struct VInput {
	int number = 0;
	QString key, type, title;
	Kind kind = Kind::Source;
	obs_source_t *src = nullptr; /* ref (nullptr no Replay) */
};

struct Inputs {
	std::vector<VInput> list;
	~Inputs()
	{
		for (auto &i : list)
			obs_source_release(i.src);
	}
	VInput *byTitle(const QString &t)
	{
		for (auto &i : list)
			if (i.title == t)
				return &i;
		return nullptr;
	}
	/* como no vMix: chave, número ou nome */
	VInput *find(const QString &ref)
	{
		const QString r = ref.trimmed();
		if (r.isEmpty())
			return nullptr;
		for (auto &i : list)
			if (i.key.compare(r, Qt::CaseInsensitive) == 0)
				return &i;
		bool ok = false;
		const int n = r.toInt(&ok);
		if (ok && n >= 1 && n <= (int)list.size())
			return &list[(size_t)n - 1];
		for (auto &i : list)
			if (i.title == r)
				return &i;
		for (auto &i : list)
			if (i.title.compare(r, Qt::CaseInsensitive) == 0)
				return &i;
		/* tolerante: "GOL CASA" acha "GOLCASA", "AMARELO.gtzip" acha "AMARELO" */
		const QString nr = norm(r);
		VInput *hit = nullptr;
		for (auto &i : list)
			if (!nr.isEmpty() && norm(i.title) == nr) {
				if (hit)
					return nullptr; /* ambíguo */
				hit = &i;
			}
		return hit;
	}
	static QString norm(const QString &s)
	{
		QString t = s.normalized(QString::NormalizationForm_D);
		t.remove(QRegularExpression("[\\x{0300}-\\x{036f}]"));
		t.remove(QRegularExpression("\\.(gtzip|gtxml|xaml|png|jpe?g|gif|mp4|mov|webm)$",
					    QRegularExpression::CaseInsensitiveOption));
		t = t.toUpper();
		t.remove(QRegularExpression("[^A-Z0-9]"));
		return t;
	}
};

#define REPLAY_KEY "instant-replay-replay"

static QString typeOf(obs_source_t *s, Kind &k)
{
	const char *id = obs_source_get_unversioned_id(s);
	const QString t = id ? QString::fromUtf8(id) : QString();
	if (gtIsSource(s)) {
		k = Kind::Gt;
		return "GT";
	}
	if (t.startsWith("text_")) {
		k = Kind::Text;
		return "Xaml";
	}
	if (t == "image_source") {
		k = Kind::Image;
		return "Image";
	}
	if (t == "slideshow") {
		k = Kind::Media;
		return "Photos";
	}
	if (t == "ffmpeg_source" || t == "vlc_source") {
		k = Kind::Media;
		return "Video";
	}
	k = Kind::Source;
	if (t == "browser_source")
		return "Browser";
	if (t.startsWith("color_source"))
		return "Colour";
	if (t == "dshow_input" || t == "av_capture_input" || t == "v4l2_input" || t == "decklink-input")
		return "Capture";
	if (t.contains("capture"))
		return "Desktop";
	if (t == "ndi_source")
		return "NDI";
	return "Video";
}

static bool enumSource(void *param, obs_source_t *s)
{
	auto *out = static_cast<std::vector<obs_source_t *> *>(param);
	if (obs_source_get_type(s) != OBS_SOURCE_TYPE_INPUT)
		return true;
	if (!(obs_source_get_output_flags(s) & OBS_SOURCE_VIDEO))
		return true;
	const char *id = obs_source_get_id(s);
	if (id && strcmp(id, "instant_replay_output") == 0)
		return true;
	if (obs_source_removed(s))
		return true;
	out->push_back(obs_source_get_ref(s));
	return true;
}

static Inputs buildInputs()
{
	Inputs in;
	auto add = [&](obs_source_t *s, Kind k, const QString &type) {
		VInput v;
		v.src = s;
		v.kind = k;
		v.type = type;
		v.title = QString::fromUtf8(obs_source_get_name(s));
		const char *uuid = obs_source_get_uuid(s);
		v.key = uuid ? QString::fromUtf8(uuid) : v.title;
		in.list.push_back(v);
	};

	/* 1) cenas, na ordem do OBS */
	struct obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++)
		add(obs_source_get_ref(scenes.sources.array[i]), Kind::Scene, "Scene");
	obs_frontend_source_list_free(&scenes);

	/* 2) fontes com vídeo (títulos GT das cenas, imagens, vídeos, textos...) */
	std::vector<obs_source_t *> srcs;
	obs_enum_sources(enumSource, &srcs);
	for (obs_source_t *s : srcs) {
		Kind k;
		const QString type = typeOf(s, k);
		add(s, k, type);
	}

	/* 3) biblioteca de títulos GT (sem cena) */
	for (obs_source_t *s : gtLibraryList())
		add(s, Kind::Gt, "GT");

	/* 4) o replay */
	VInput r;
	r.kind = Kind::Replay;
	r.type = "Replay";
	r.title = "Replay";
	r.key = REPLAY_KEY;
	in.list.push_back(r);

	for (size_t i = 0; i < in.list.size(); i++)
		in.list[i].number = (int)i + 1;
	return in;
}

/* ======================= programa / preview ======================= */

static QString g_previewName; /* fora do Modo Estúdio, o "preview" do vMix fica aqui */

static QString programName()
{
	obs_source_t *p = obs_frontend_get_current_scene();
	const QString n = p ? QString::fromUtf8(obs_source_get_name(p)) : QString();
	obs_source_release(p);
	return n;
}

static QString previewName()
{
	if (obs_frontend_preview_program_mode_active()) {
		obs_source_t *p = obs_frontend_get_current_preview_scene();
		const QString n = p ? QString::fromUtf8(obs_source_get_name(p)) : QString();
		obs_source_release(p);
		return n;
	}
	return g_previewName.isEmpty() ? programName() : g_previewName;
}

static int numberOfTitle(Inputs &in, const QString &title)
{
	if (title.isEmpty())
		return 0;
	for (auto &i : in.list)
		if (i.kind == Kind::Scene && i.title == title)
			return i.number;
	VInput *v = in.byTitle(title);
	return v ? v->number : 0;
}

static int overlayInput(Inputs &in, int ch)
{
	if (!overlaysAlive() || !overlays().isOn(ch))
		return 0;
	return numberOfTitle(in, QString::fromStdString(overlays().slot(ch).source));
}

/* ======================= fade to black ======================= */

#define FTB_CHANNEL 61

static obs_source_t *g_ftbTrans = nullptr;
static obs_source_t *g_ftbBlack = nullptr;
static obs_scene_t *g_ftbEmpty = nullptr;
static bool g_ftbOn = false;

static void ftbRelease()
{
	if (g_ftbTrans) {
		obs_source_t *cur = obs_get_output_source(FTB_CHANNEL);
		if (cur == g_ftbTrans)
			obs_set_output_source(FTB_CHANNEL, nullptr);
		obs_source_release(cur);
		obs_transition_clear(g_ftbTrans);
	}
	obs_source_release(g_ftbTrans);
	obs_source_release(g_ftbBlack);
	obs_scene_release(g_ftbEmpty);
	g_ftbTrans = g_ftbBlack = nullptr;
	g_ftbEmpty = nullptr;
	g_ftbOn = false;
}

static void ftbSet(bool on)
{
	if (!g_ftbTrans) {
		struct obs_video_info ovi = {};
		obs_get_video_info(&ovi);
		obs_data_t *st = obs_data_create();
		obs_data_set_int(st, "color", 0xFF000000);
		obs_data_set_int(st, "width", ovi.base_width ? ovi.base_width : 1920);
		obs_data_set_int(st, "height", ovi.base_height ? ovi.base_height : 1080);
		g_ftbBlack = obs_source_create_private("color_source", "instant-replay-ftb-black", st);
		obs_data_release(st);
		g_ftbEmpty = obs_scene_create_private("instant-replay-ftb-empty");
		g_ftbTrans = obs_source_create_private("fade_transition", "instant-replay-ftb", nullptr);
		if (!g_ftbTrans || !g_ftbBlack) {
			ftbRelease();
			return;
		}
	}
	obs_source_t *empty = obs_scene_get_source(g_ftbEmpty);
	obs_transition_set(g_ftbTrans, on ? empty : g_ftbBlack);
	obs_set_output_source(FTB_CHANNEL, g_ftbTrans);
	obs_transition_start(g_ftbTrans, OBS_TRANSITION_MODE_AUTO, 1000, on ? g_ftbBlack : empty);
	g_ftbOn = on;
}

/* ======================= XML ======================= */

static QString esc(const QString &s)
{
	QString o = s;
	o.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;").replace('"', "&quot;");
	return o;
}

static const char *B(bool b)
{
	return b ? "True" : "False";
}

static QString stripFile(const QString &v)
{
	return v.startsWith("file:") ? v.mid(5) : v;
}

static QString settingString(obs_source_t *s, const char *name)
{
	obs_data_t *st = obs_source_get_settings(s);
	const QString v = QString::fromUtf8(obs_data_get_string(st, name));
	obs_data_release(st);
	return v;
}

static QString inputXml(VInput &i)
{
	QString state = "Paused";
	double position = 0, duration = 0;
	bool muted = false;
	QString inner = esc(i.title);
	if (i.src) {
		muted = obs_source_muted(i.src);
		if (i.kind == Kind::Scene)
			state = "Running";
		else if (i.kind == Kind::Gt)
			state = gtState(i.src) == 1 || gtState(i.src) == 2 ? "Running" : "Paused";
		else if (obs_source_get_output_flags(i.src) & OBS_SOURCE_CONTROLLABLE_MEDIA) {
			state = obs_source_media_get_state(i.src) == OBS_MEDIA_STATE_PLAYING ? "Running" : "Paused";
			position = (double)obs_source_media_get_time(i.src);
			duration = (double)obs_source_media_get_duration(i.src);
		} else
			state = obs_source_active(i.src) ? "Running" : "Paused";
	} else if (i.kind == Kind::Replay && engineAlive()) {
		const ChannelState st = engine().channel(0).state();
		state = st.playing ? "Running" : "Paused";
	}

	if (i.kind == Kind::Gt) {
		auto title = gtTitleOf(i.src);
		const auto vals = gtValues(i.src);
		int ti = 0, ii = 0;
		if (title) {
			for (const GtField &f : title->fields()) {
				const QString v = vals.contains(f.id) ? vals.value(f.id) : f.value;
				if (f.kind == GtField::Text)
					inner += QString("<text index=\"%1\" name=\"%2\">%3</text>").arg(ti++).arg(esc(f.id)).arg(esc(v));
				else if (f.kind == GtField::Image)
					inner += QString("<image index=\"%1\" name=\"%2\">%3</image>")
							 .arg(ii++)
							 .arg(esc(f.id))
							 .arg(esc(stripFile(v)));
			}
		}
	} else if (i.kind == Kind::Text) {
		inner += QString("<text index=\"0\" name=\"Text.Text\">%1</text>").arg(esc(settingString(i.src, "text")));
	} else if (i.kind == Kind::Image) {
		inner += QString("<image index=\"0\" name=\"Image.Source\">%1</image>").arg(esc(settingString(i.src, "file")));
	}

	return QString("<input key=\"%1\" number=\"%2\" type=\"%3\" title=\"%4\" shortTitle=\"%4\" state=\"%5\" "
		       "position=\"%6\" duration=\"%7\" loop=\"False\" muted=\"%8\">%9</input>")
		.arg(esc(i.key))
		.arg(i.number)
		.arg(esc(i.type))
		.arg(esc(i.title))
		.arg(state)
		.arg((qint64)position)
		.arg((qint64)duration)
		.arg(B(muted))
		.arg(inner);
}

QByteArray VmixApi::xml()
{
	Inputs in = buildInputs();
	QString x = "<vmix><version>27.0.0.49</version><edition>4K</edition>";
	char *coll = obs_frontend_get_current_scene_collection();
	x += "<preset>" + esc(QString::fromUtf8(coll ? coll : "")) + "</preset>";
	bfree(coll);
	x += "<inputs>";
	for (auto &i : in.list)
		x += inputXml(i);
	x += "</inputs><overlays>";
	for (int c = 0; c < 8; c++) {
		const int n = overlayInput(in, c);
		x += n ? QString("<overlay number=\"%1\">%2</overlay>").arg(c + 1).arg(n)
		       : QString("<overlay number=\"%1\" />").arg(c + 1);
	}
	x += "</overlays>";
	x += QString("<preview>%1</preview><active>%2</active>")
		     .arg(numberOfTitle(in, previewName()))
		     .arg(numberOfTitle(in, programName()));
	x += QString("<fadeToBlack>%1</fadeToBlack>").arg(B(g_ftbOn));
	x += "<transitions>";
	{
		struct obs_frontend_source_list trl = {};
		obs_frontend_get_transitions(&trl);
		const int dur = obs_frontend_get_transition_duration();
		for (size_t i = 0; i < trl.sources.num && i < 4; i++)
			x += QString("<transition number=\"%1\" effect=\"%2\" duration=\"%3\" />")
				     .arg(i + 1)
				     .arg(esc(QString::fromUtf8(obs_source_get_name(trl.sources.array[i]))))
				     .arg(dur);
		obs_frontend_source_list_free(&trl);
	}
	x += "</transitions>";
	x += QString("<recording>%1</recording><external>%2</external><streaming>%3</streaming>")
		     .arg(B(obs_frontend_recording_active()))
		     .arg(B(obs_frontend_virtualcam_active()))
		     .arg(B(obs_frontend_streaming_active()));
	x += QString("<playList>False</playList><multiCorder>%1</multiCorder><fullscreen>False</fullscreen>")
		     .arg(B(engineAlive() && engine().recording()));
	x += "<audio><master volume=\"100\" muted=\"False\" meterF1=\"0\" meterF2=\"0\" headphonesVolume=\"100\" /></audio>";
	x += "</vmix>";
	return x.toUtf8();
}

/* ======================= funções ======================= */

static bool isStingerT(obs_source_t *t)
{
	const char *id = obs_source_get_id(t);
	return id && (strstr(id, "stinger") || strstr(id, "vplus_gt") || obs_transition_fixed(t));
}

/* nome da transição do OBS: por tipo ("fade_transition"), pela posição (n >= 1) ou
 * a n-ésima stinger (stinger = true) */
static std::string transitionName(const char *idPart, int nth = 0, bool stinger = false)
{
	struct obs_frontend_source_list tl = {};
	obs_frontend_get_transitions(&tl);
	std::string name;
	int seen = 0;
	for (size_t i = 0; i < tl.sources.num && name.empty(); i++) {
		obs_source_t *t = tl.sources.array[i];
		const char *id = obs_source_get_id(t);
		bool hit = false;
		if (stinger)
			hit = isStingerT(t) && ++seen == nth;
		else if (nth > 0)
			hit = (int)i + 1 == nth;
		else
			hit = id && strstr(id, idPart);
		if (hit)
			name = obs_source_get_name(t);
	}
	obs_frontend_source_list_free(&tl);
	return name;
}

static QString toProgram(VInput *to, const std::string &transition, int ms, bool needFound = true)
{
	if (!to)
		return "Input não encontrado";
	if (to->kind != Kind::Scene)
		return QString("'%1' não é uma cena do OBS (use os overlays para títulos e fontes)").arg(to->title);
	if (needFound && transition.empty())
		return "transição não encontrada em \"Transições de cena\" do OBS";
	AirControl *a = AirControl::instance();
	if (!a)
		return "plugin ainda carregando";
	const QString before = programName();
	a->switchProgram(to->src, transition, ms);
	/* como no vMix: o que estava no programa vai para o preview */
	if (!obs_frontend_preview_program_mode_active() && before != to->title)
		g_previewName = before;
	return QString();
}

static QString colorArg(const QString &v)
{
	QColor c(v.trimmed());
	if (!c.isValid())
		return QString();
	return c.name(QColor::HexArgb);
}

static void gtSet(obs_source_t *src, const QString &key, const QString &value)
{
	auto vals = gtValues(src);
	vals[key] = value;
	gtSetValues(src, vals);
}

static bool gtHasField(obs_source_t *src, const QString &id)
{
	auto t = gtTitleOf(src);
	if (!t)
		return false;
	for (const GtField &f : t->fields())
		if (f.id == id)
			return true;
	return false;
}

/* SelectedName ou SelectedIndex → nome do campo */
static QString fieldName(VInput *in, const QString &selName, const QString &selIndex, GtField::Kind kind)
{
	if (!selName.isEmpty())
		return selName;
	bool ok = false;
	const int idx = selIndex.toInt(&ok);
	if (!ok || !in || in->kind != Kind::Gt)
		return QString();
	auto t = gtTitleOf(in->src);
	if (!t)
		return QString();
	int n = 0;
	for (const GtField &f : t->fields())
		if (f.kind == kind && n++ == idx)
			return f.id;
	return QString();
}

static QString baseOf(const QString &field)
{
	const int dot = field.indexOf('.');
	return dot < 0 ? field : field.left(dot);
}

static void updateSetting(obs_source_t *s, const char *name, const QString &v)
{
	obs_data_t *st = obs_data_create();
	obs_data_set_string(st, name, v.toUtf8().constData());
	obs_source_update(s, st);
	obs_data_release(st);
}

static void applyChannel(const QString &ch)
{
	const QString c = ch.trimmed().toUpper();
	if (c == "A")
		engine().setSelection(SEL_A);
	else if (c == "B")
		engine().setSelection(SEL_B);
	else if (c == "AB" || c == "A+B")
		engine().setSelection(SEL_AB);
}

static bool channelLive()
{
	const int sel = engine().getSelection();
	return engine().channel((sel & SEL_A) ? 0 : 1).state().liveMode;
}

QString VmixApi::run(const QString &fnIn, const QMap<QString, QString> &P)
{
	auto prm = [&](const char *k) {
		for (auto it = P.begin(); it != P.end(); ++it)
			if (it.key().compare(QLatin1String(k), Qt::CaseInsensitive) == 0)
				return it.value();
		return QString();
	};
	const QString fn = fnIn.trimmed();
	const QString f = fn.toLower();
	const QString value = prm("Value");
	if (f.isEmpty())
		return "Function vazia";
	if (!engineAlive())
		return "plugin ainda carregando";

	Inputs in = buildInputs();
	VInput *input = in.find(prm("Input"));
	if (!prm("Input").isEmpty() && !input)
		return QString("Input '%1' não encontrado").arg(prm("Input"));
	auto previewInput = [&]() -> VInput * {
		for (auto &i : in.list)
			if (i.kind == Kind::Scene && i.title == previewName())
				return &i;
		return nullptr;
	};
	const int durationMs = prm("Duration").toInt();
	AirControl *air = AirControl::instance();
	const int list = engine().activeList();
	QRegularExpressionMatch m;

	/* ---------- cenas e transições ---------- */
	if (f == "cut" || f == "activeinput")
		return toProgram(input ? input : previewInput(), transitionName("cut_transition"), 0);
	if (f == "cutdirect")
		return input ? toProgram(input, transitionName("cut_transition"), 0) : "CutDirect precisa de Input";
	if (f == "fade" || f == "merge" || f == "fadetoblack0")
		return toProgram(input ? input : previewInput(), transitionName("fade_transition"), durationMs);
	if (f == "wipe" || f == "slide" || f == "zoom" || f == "cubezoom" || f == "fly")
		return toProgram(input ? input : previewInput(),
				 transitionName(f == "wipe" ? "wipe_transition" : "slide_transition"), durationMs);
	if (f == "quickplay")
		return toProgram(input ? input : previewInput(), std::string(), durationMs, false);
	if ((m = QRegularExpression("^transition([1-4])$").match(f)).hasMatch())
		return toProgram(input ? input : previewInput(), transitionName("", m.captured(1).toInt()), 0);
	if ((m = QRegularExpression("^stinger([1-8])$").match(f)).hasMatch())
		return toProgram(input ? input : previewInput(), transitionName("", m.captured(1).toInt(), true), 0);
	if ((m = QRegularExpression("^settransitionduration([1-4])$").match(f)).hasMatch()) {
		if (value.toInt() > 0)
			obs_frontend_set_transition_duration(value.toInt());
		return QString();
	}
	if (f == "previewinput") {
		if (!input || input->kind != Kind::Scene)
			return "PreviewInput: escolha uma cena do OBS";
		if (obs_frontend_preview_program_mode_active())
			obs_frontend_set_current_preview_scene(input->src);
		else
			g_previewName = input->title;
		return QString();
	}
	if (f == "fadetoblack") {
		ftbSet(!g_ftbOn);
		return QString();
	}

	/* ---------- overlays 1–8 ---------- */
	if (f == "overlayinputalloff") {
		for (int i = 0; i < OVERLAY_COUNT; i++)
			overlays().setOn(i, false);
		return QString();
	}
	if ((m = QRegularExpression("^overlayinput([1-8])(in|out|off|last|zoom|preview)?$").match(f)).hasMatch()) {
		const int ch = m.captured(1).toInt() - 1;
		const QString act = m.captured(2);
		const bool on = overlays().isOn(ch);
		const bool same = input && on && QString::fromStdString(overlays().slot(ch).source) == input->title;
		if (input && input->kind == Kind::Replay)
			return "use a cena do replay como input do overlay";
		if (act == "out" || act == "off") {
			overlays().setOn(ch, false);
		} else if (act == "in") {
			if (input && !same)
				overlays().assignOn(ch, input->title.toStdString());
			else if (!input)
				overlays().setOn(ch, true);
		} else if (act.isEmpty()) {
			if (input && !same)
				overlays().assignOn(ch, input->title.toStdString());
			else
				overlays().setOn(ch, !on);
		}
		return QString();
	}

	/* ---------- títulos (GT) e textos ---------- */
	if (f == "settext" || f == "settextcolour" || f == "settextcolor" || f == "setcolor" || f == "setcolour" ||
	    f.startsWith("settextvisible") || f == "setimage" || f.startsWith("setimagevisible")) {
		if (!input)
			return fn + " precisa de Input";
		const bool image = f.startsWith("setimage");
		QString field = fieldName(input, prm("SelectedName"), prm("SelectedIndex"),
					  image ? GtField::Image : GtField::Text);
		if (input->kind == Kind::Gt) {
			if (field.isEmpty())
				return "informe SelectedName";
			if (f == "settext") {
				gtSet(input->src, field.contains('.') ? field : field + ".Text", value);
			} else if (f == "setimage") {
				QString key = field;
				if (!key.contains('.'))
					key = gtHasField(input->src, field + ".Fill.Bitmap") ? field + ".Fill.Bitmap"
											      : field + ".Source";
				gtSet(input->src, key, value);
			} else if (f.startsWith("settextcolo") || f.startsWith("setcolo")) {
				const QString c = colorArg(value);
				if (c.isEmpty())
					return QString("cor inválida: %1").arg(value);
				const QString key = (f.startsWith("setcolo") && field.contains('.')) ? field
												  : baseOf(field) + ".Fill.Color";
				gtSet(input->src, key, c);
			} else {
				/* SetTextVisibleOn/Off, SetImageVisibleOn/Off ou alternar */
				const QString key = baseOf(field) + ".Visible";
				bool vis;
				if (f.endsWith("on"))
					vis = true;
				else if (f.endsWith("off"))
					vis = false;
				else
					vis = gtValues(input->src).value(key, "True") == "False";
				gtSet(input->src, key, vis ? "True" : "False");
			}
			return QString();
		}
		if (input->kind == Kind::Text && f == "settext") {
			updateSetting(input->src, "text", value);
			return QString();
		}
		if (input->kind == Kind::Text && f.startsWith("settextcolo")) {
			QColor c(value.trimmed());
			if (!c.isValid())
				return QString("cor inválida: %1").arg(value);
			/* OBS guarda a cor como 0xAABBGGRR */
			const long long abgr = ((long long)c.alpha() << 24) | (c.blue() << 16) | (c.green() << 8) | c.red();
			obs_data_t *st = obs_data_create();
			obs_data_set_int(st, "color", abgr);
			obs_data_set_int(st, "color1", abgr);
			obs_data_set_int(st, "color2", abgr);
			obs_source_update(input->src, st);
			obs_data_release(st);
			return QString();
		}
		if (input->kind == Kind::Image && f == "setimage") {
			updateSetting(input->src, "file", value);
			return QString();
		}
		return QString("%1 não vale para '%2' (%3)").arg(fn, input->title, input->type);
	}
	if (f == "titlebeginanimation" || f == "titleplayin" || f == "titleplayout" || f == "titletransitionin" ||
	    f == "titletransitionout") {
		if (!input || input->kind != Kind::Gt)
			return "escolha um título GT";
		const QString v = f == "titleplayin" || f == "titletransitionin"   ? "TransitionIn"
				  : f == "titleplayout" || f == "titletransitionout" ? "TransitionOut"
										      : value;
		if (v.compare("TransitionIn", Qt::CaseInsensitive) == 0)
			gtPlayIn(input->src);
		else if (v.compare("TransitionOut", Qt::CaseInsensitive) == 0)
			gtPlayOut(input->src);
		else
			gtPlayPage(input->src, v);
		return QString();
	}

	/* ---------- mídia e áudio ---------- */
	if (f == "play" || f == "pause" || f == "playpause" || f == "restart" || f == "nextitem" || f == "previousitem") {
		if (!input || !input->src)
			return fn + " precisa de Input";
		if (input->kind == Kind::Gt) {
			if (f == "play" || f == "restart")
				gtPlayIn(input->src);
			else if (f == "pause")
				gtPlayOut(input->src);
			else
				gtToggle(input->src);
			return QString();
		}
		const bool playing = obs_source_media_get_state(input->src) == OBS_MEDIA_STATE_PLAYING;
		if (f == "play")
			obs_source_media_play_pause(input->src, false);
		else if (f == "pause")
			obs_source_media_play_pause(input->src, true);
		else if (f == "playpause")
			obs_source_media_play_pause(input->src, playing);
		else if (f == "restart")
			obs_source_media_restart(input->src);
		else if (f == "nextitem")
			obs_source_media_next(input->src);
		else
			obs_source_media_previous(input->src);
		return QString();
	}
	if (f == "setvolume" || f == "audioon" || f == "audiooff" || f == "audio") {
		if (!input || !input->src)
			return fn + " precisa de Input";
		if (f == "setvolume") {
			const double v = std::max(0.0, std::min(100.0, value.toDouble())) / 100.0;
			obs_source_set_volume(input->src, (float)(v * v)); /* curva parecida com a do vMix */
		} else
			obs_source_set_muted(input->src,
					     f == "audiooff" || (f == "audio" && !obs_source_muted(input->src)));
		return QString();
	}

	/* ---------- saídas ---------- */
	if (f == "startstopstreaming" || f == "startstreaming" || f == "stopstreaming") {
		const bool on = obs_frontend_streaming_active();
		if (f == "startstreaming" ? !on : f == "stopstreaming" ? on : true)
			on ? obs_frontend_streaming_stop() : obs_frontend_streaming_start();
		return QString();
	}
	if (f == "startstoprecording" || f == "startrecording" || f == "stoprecording") {
		const bool on = obs_frontend_recording_active();
		if (f == "startrecording" ? !on : f == "stoprecording" ? on : true)
			on ? obs_frontend_recording_stop() : obs_frontend_recording_start();
		return QString();
	}
	if (f == "startstopexternal" || f == "startexternal" || f == "stopexternal") {
		const bool on = obs_frontend_virtualcam_active();
		if (f == "startexternal" ? !on : f == "stopexternal" ? on : true)
			on ? obs_frontend_stop_virtualcam() : obs_frontend_start_virtualcam();
		return QString();
	}
	if (f == "startstopmulticorder" || f == "startmulticorder" || f == "stopmulticorder" ||
	    f == "replaystartstoprecording" || f == "replaystartrecording" || f == "replaystoprecording") {
		const bool on = engine().recording();
		const bool start = f.contains("startstop") ? !on : f.startsWith("start") || f == "replaystartrecording";
		if (start && !on) {
			std::string err;
			if (!engine().startRecording(err))
				return QString::fromStdString(err);
		} else if (!start && on)
			engine().stopRecording();
		return QString();
	}

	/* ---------- replay ---------- */
	if (f.startsWith("replay")) {
		applyChannel(prm("Channel"));
		if (f == "replayplay") {
			engine().play();
		} else if (f == "replaypause") {
			engine().pause();
		} else if (f == "replayplaypause") {
			engine().togglePlay();
		} else if (f == "replaysetspeed") {
			const double v = value.toDouble();
			engine().setReverse(v < 0);
			if (std::fabs(v) < 0.005)
				engine().pause();
			else
				engine().setSpeedPercent((int)std::lround(std::fabs(v) * 100));
		} else if (f == "replaychangespeed") {
			engine().setSpeedPercent(engine().speedPercent() + (int)std::lround(value.toDouble() * 100));
		} else if (f == "replayjumpframes") {
			engine().step(value.toInt());
		} else if (f == "replayjump") {
			engine().jumpSeconds(value.toDouble() / 1000.0);
		} else if (f == "replayjumptonow" || f == "replaylive") {
			engine().goLive();
		} else if (f == "replayrecorded") {
			if (channelLive())
				engine().pause();
		} else if (f == "replaylivetoggle") {
			if (channelLive())
				engine().pause();
			else
				engine().goLive();
		} else if (f == "replayfastforward") {
			engine().setReverse(false);
			engine().fastForward();
		} else if (f == "replayfastbackward") {
			engine().setReverse(true);
			engine().fastForward();
		} else if (f == "replaymarkin" || f == "replaymarkinlive" || f == "replaymarkinrecorded") {
			engine().markIn();
		} else if (f == "replaymarkout" || f == "replaymarkoutlive" || f == "replaymarkoutrecorded") {
			engine().markOut();
		} else if (f == "replaymarkcancel") {
			engine().markCancel();
		} else if (f == "replaymarkinout" || f == "replaymarkinoutlive" || f == "replaymarkinoutrecorded") {
			const int s = value.toInt() > 0 ? value.toInt() : 10;
			engine().autoEvent(s);
		} else if (f == "replayplaylastevent") {
			engine().playLastEvent();
		} else if (f == "replayplaylasteventtooutput") {
			if (!air)
				return "plugin ainda carregando";
			if (engine().playLastEvent())
				air->takeIn(1);
		} else if (f == "replayplayselectedevent" || f == "replayplayselectedeventtooutput") {
			const int64_t id = replayDockSelectedEvent();
			if (!id)
				return "nenhum evento selecionado";
			if (f.endsWith("tooutput")) {
				engine().playEventAllAngles(list, id);
				if (air)
					air->takeIn(3);
			} else
				engine().playEvent(list, id);
		} else if (f == "replayplayallevents" || f == "replayplayalleventstooutput") {
			engine().playEvents(list, 0);
			if (f.endsWith("tooutput") && air)
				air->takeIn(2);
		} else if (f == "replayplayevents" || f == "replayplayeventstooutput") {
			engine().playEventsConfigured(list, replayDockSelectedEvent());
			if (f.endsWith("tooutput") && air)
				air->takeIn(2);
		} else if (f == "replaystopevents") {
			if (air && air->runKind())
				air->abortRun();
			else
				engine().pause();
		} else if (f == "replayselectnextevent") {
			replayDockSelect(1);
		} else if (f == "replayselectpreviousevent") {
			replayDockSelect(-1);
		} else if (f == "replayselectfirstevent") {
			replayDockSelect(-2);
		} else if (f == "replayselectlastevent") {
			replayDockSelect(2);
		} else if (f == "replaydeleteselectedevent") {
			if (const int64_t id = replayDockSelectedEvent())
				engine().deleteEvent(list, id);
		} else if (f == "replayselectchannela" || f == "replayselectchannelb" || f == "replayselectchannelab") {
			engine().setSelection(f.endsWith("ab") ? SEL_AB : f.endsWith("a") ? SEL_A : SEL_B);
		} else if ((m = QRegularExpression("^replay(a|b)?camera([1-8])$").match(f)).hasMatch()) {
			const int prev = engine().getSelection();
			if (m.captured(1) == "a")
				engine().setSelection(SEL_A);
			else if (m.captured(1) == "b")
				engine().setSelection(SEL_B);
			engine().setAngle(m.captured(2).toInt() - 1);
			if (!m.captured(1).isEmpty())
				engine().setSelection(prev);
		} else if (f == "replayzoomin" || f == "replayzoomout") {
			/* extra do plugin (o vMix não tem): Value = fator, ex. 1.25 */
			float k = value.toFloat() > 1.0f ? value.toFloat() : 1.25f;
			engine().zoomStep(f.endsWith("in") ? k : 1.0f / k);
		} else if (f == "replayzoomleft" || f == "replayzoomright" || f == "replayzoomup" || f == "replayzoomdown") {
			const float st = value.toFloat() > 0 ? value.toFloat() : 0.1f;
			engine().zoomStep(1.0f, f.endsWith("left") ? -st : f.endsWith("right") ? st : 0.0f,
					  f.endsWith("up") ? -st : f.endsWith("down") ? st : 0.0f);
		} else if (f == "replayzoomreset") {
			engine().zoomReset();
		} else if (f == "replayzoomset") {
			/* extra do plugin: Value = "zoom,x,y" (x e y de 0 a 1), ex. 2,0.25,0.5 */
			const QStringList v = value.split(',');
			if (v.size() >= 1) {
				const PlaybackChannel::Zoom zm = engine().channel((engine().getSelection() & SEL_A) ? 0 : 1).zoom();
				engine().zoomSet(v[0].toFloat(), v.size() > 1 ? v[1].toFloat() : zm.x, v.size() > 2 ? v[2].toFloat() : zm.y);
			}
		} else if (f == "replayloop" || f == "replaytoggleloop") {
			engine().setLoop(!engine().loopOn());
		} else if (f == "replayshowhide" || QRegularExpression("^replayselectevents\\d+$").match(f).hasMatch()) {
			/* sem equivalente no OBS (uma lista só, sem janela) */
		} else {
			return QString("Function %1 não existe no plugin do OBS").arg(fn);
		}
		return QString();
	}

	return QString("Function %1 não existe no plugin do OBS").arg(fn);
}

/* ======================= servidores ======================= */

void VmixApi::create(QObject *parent)
{
	if (!g_api) {
		g_api = new VmixApi(parent);
		/* depois que o OBS terminar de abrir (cenas carregadas) */
		QTimer::singleShot(0, g_api, [] {
			if (g_api)
				g_api->restart();
		});
	}
}

void VmixApi::destroy()
{
	delete g_api;
	g_api = nullptr;
}

VmixApi *VmixApi::instance()
{
	return g_api;
}

VmixApi::VmixApi(QObject *parent) : QObject(parent)
{
	timer = new QTimer(this);
	timer->setInterval(150);
	connect(timer, &QTimer::timeout, this, &VmixApi::tick);
}

VmixApi::~VmixApi()
{
	stop();
	ftbRelease();
	if (g_api == this)
		g_api = nullptr;
}

void VmixApi::stop()
{
	timer->stop();
	for (QTcpServer *s : {http, tcp}) {
		if (!s)
			continue;
		for (QTcpSocket *c : s->findChildren<QTcpSocket *>())
			c->abort();
		s->close();
		s->deleteLater();
	}
	http = tcp = nullptr;
}

static QStringList localIps()
{
	QStringList ips;
	for (const QHostAddress &a : QNetworkInterface::allAddresses())
		if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback())
			ips << a.toString();
	return ips;
}

void VmixApi::restart()
{
	stop();
	if (!engineAlive())
		return;
	const ReplayConfig cfg = engine().config();
	if (!cfg.apiEnabled) {
		statusText = tl("Desligado");
		rlog(LOG_INFO, "API para o app: desligada");
		return;
	}
	const QHostAddress addr = cfg.apiLocalOnly ? QHostAddress(QHostAddress::LocalHost) : QHostAddress(QHostAddress::AnyIPv4);
	http = new QTcpServer(this);
	tcp = new QTcpServer(this);
	connect(http, &QTcpServer::newConnection, this, &VmixApi::onHttpConnection);
	connect(tcp, &QTcpServer::newConnection, this, &VmixApi::onTcpConnection);
	QStringList errs;
	if (!http->listen(addr, (quint16)cfg.apiHttpPort))
		errs << tl("porta %1 ocupada (o vMix está aberto neste PC?)").arg(cfg.apiHttpPort);
	if (!tcp->listen(addr, (quint16)cfg.apiTcpPort))
		errs << tl("porta TCP %1 ocupada").arg(cfg.apiTcpPort);
	const QString where = cfg.apiLocalOnly ? QString("127.0.0.1") : localIps().join(", ");
	if (errs.isEmpty())
		statusText = tl("Ligado — endereço: %1, porta %2 (TCP %3)")
				     .arg(where.isEmpty() ? "127.0.0.1" : where)
				     .arg(cfg.apiHttpPort)
				     .arg(cfg.apiTcpPort);
	else
		statusText = tl("Erro: ") + errs.join("; ");
	rlog(errs.isEmpty() ? LOG_INFO : LOG_WARNING, "API para o app: %s", statusText.toUtf8().constData());
	timer->start();
}

static QByteArray httpResponse(int code, const char *type, const QByteArray &body)
{
	const char *reason = code == 200 ? "OK" : code == 404 ? "Not Found" : code == 400 ? "Bad Request" : "Error";
	QByteArray r = "HTTP/1.1 " + QByteArray::number(code) + " " + reason + "\r\n";
	r += QByteArray("Content-Type: ") + type + "; charset=utf-8\r\n";
	r += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
	r += "Access-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n";
	return r + body;
}

static QMap<QString, QString> parseQuery(const QByteArray &q, QString *function)
{
	QMap<QString, QString> out;
	for (const QByteArray &part : q.split('&')) {
		if (part.isEmpty())
			continue;
		const int eq = part.indexOf('=');
		QByteArray k = eq < 0 ? part : part.left(eq);
		QByteArray v = eq < 0 ? QByteArray() : part.mid(eq + 1);
		k.replace('+', ' ');
		v.replace('+', ' ');
		const QString key = QUrl::fromPercentEncoding(k);
		const QString val = QUrl::fromPercentEncoding(v);
		if (function && key.compare("Function", Qt::CaseInsensitive) == 0)
			*function = val;
		else
			out[key] = val;
	}
	return out;
}

void VmixApi::onHttpConnection()
{
	while (QTcpSocket *s = http->nextPendingConnection()) {
		connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
		connect(s, &QTcpSocket::readyRead, this, [this, s] { handleHttp(s); });
	}
}

void VmixApi::handleHttp(QTcpSocket *s)
{
	QByteArray buf = s->property("buf").toByteArray() + s->readAll();
	if (buf.size() > 65536) {
		s->abort();
		return;
	}
	const int end = buf.indexOf("\r\n\r\n");
	if (end < 0) {
		s->setProperty("buf", buf);
		return;
	}
	const QList<QByteArray> parts = buf.left(buf.indexOf("\r\n")).split(' ');
	if (parts.size() < 2 || (parts[0] != "GET" && parts[0] != "POST")) {
		s->write(httpResponse(400, "text/plain", "bad request"));
		s->disconnectFromHost();
		return;
	}
	const QByteArray target = parts[1];
	const int qm = target.indexOf('?');
	const QByteArray path = (qm < 0 ? target : target.left(qm)).toLower();
	const QByteArray query = qm < 0 ? QByteArray() : target.mid(qm + 1);
	QString function;
	const auto params = parseQuery(query, &function);

	QByteArray resp;
	if (path == "/api" || path == "/api/") {
		if (function.isEmpty()) {
			resp = httpResponse(200, "text/xml", xml());
		} else {
			const QString err = run(function, params);
			resp = err.isEmpty() ? httpResponse(200, "text/plain", "Function completed successfully.")
					     : httpResponse(500, "text/plain", ("Error: " + err).toUtf8());
		}
	} else if (path == "/" || path == "/index.html") {
		const QString sc = params.value("shortcut");
		if (!sc.isEmpty()) {
			bool ok = false;
			const size_t i = (size_t)sc.toULongLong(&ok, 16);
			if (ok && i < pluginHotkeyCount())
				pluginRunHotkey(i);
			resp = httpResponse(200, "text/html", "");
		} else {
			/* mesmo formato da página de atalhos do vMix */
			QString html = "<!doctype html><html><head><meta charset=\"utf-8\"><title>ReplayDeck - "
				       "Shortcuts</title></head><body>";
			for (size_t i = 0; i < pluginHotkeyCount(); i++) {
				const QString id = QString("%1").arg((qulonglong)i, 8, 16, QChar('0'));
				const QString desc = QString::fromUtf8(pluginHotkeyDesc(i));
				const int colon = desc.indexOf(':');
				const QString grp = colon > 0 ? desc.left(colon) : QString("OBS");
				const QString lbl = colon > 0 ? desc.mid(colon + 1).trimmed() : desc;
				html += QString("<div onclick=\"javascript:$.ajax('?shortcut=%1');\" class=\"shortcutButton\">"
						"<div class=\"shortcutButtonText\"><strong>%2</strong><br />%3</div></div>\n")
						.arg(id, esc(lbl), esc(grp));
			}
			html += "</body></html>";
			resp = httpResponse(200, "text/html", html.toUtf8());
		}
	} else {
		resp = httpResponse(404, "text/plain", "not found");
	}
	s->setProperty("buf", QByteArray());
	s->write(resp);
	s->disconnectFromHost();
}

void VmixApi::onTcpConnection()
{
	while (QTcpSocket *s = tcp->nextPendingConnection()) {
		s->setSocketOption(QAbstractSocket::LowDelayOption, 1);
		connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
		connect(s, &QTcpSocket::readyRead, this, [this, s] {
			QByteArray buf = s->property("buf").toByteArray() + s->readAll();
			int nl;
			while ((nl = buf.indexOf('\n')) >= 0) {
				QByteArray line = buf.left(nl);
				buf = buf.mid(nl + 1);
				if (line.endsWith('\r'))
					line.chop(1);
				handleTcpLine(s, line.trimmed());
			}
			if (buf.size() > 65536)
				buf.clear();
			s->setProperty("buf", buf);
		});
		s->write("VERSION OK 27.0.0.49\r\n");
	}
}

static QByteArray tallyString()
{
	Inputs in = buildInputs();
	QByteArray t(in.list.size(), '0');
	const int prev = numberOfTitle(in, previewName());
	const int prog = numberOfTitle(in, programName());
	if (prev > 0)
		t[prev - 1] = '2';
	for (int c = 0; c < OVERLAY_COUNT; c++)
		if (const int n = overlayInput(in, c))
			t[n - 1] = '1';
	for (auto &i : in.list)
		if (i.kind == Kind::Gt && (gtState(i.src) == 1 || gtState(i.src) == 2) && obs_source_showing(i.src))
			t[i.number - 1] = '1';
	if (AirControl::instance() && AirControl::instance()->onAir())
		t[in.list.size() - 1] = '1';
	if (prog > 0)
		t[prog - 1] = '1';
	return t;
}

void VmixApi::handleTcpLine(QTcpSocket *s, const QByteArray &line)
{
	if (line.isEmpty())
		return;
	const int sp = line.indexOf(' ');
	const QByteArray cmd = (sp < 0 ? line : line.left(sp)).toUpper();
	const QByteArray rest = sp < 0 ? QByteArray() : line.mid(sp + 1).trimmed();

	if (cmd == "TALLY") {
		s->write("TALLY OK " + tallyString() + "\r\n");
	} else if (cmd == "SUBSCRIBE" || cmd == "UNSUBSCRIBE") {
		const QByteArray what = rest.toUpper();
		const bool on = cmd == "SUBSCRIBE";
		if (what != "TALLY" && what != "ACTS") {
			s->write(cmd + " ER " + what + " not supported\r\n");
			return;
		}
		s->setProperty(what == "TALLY" ? "subTally" : "subActs", on);
		s->write(cmd + " OK " + what + (on ? " Subscribed" : " Unsubscribed") + "\r\n");
		if (on && what == "TALLY")
			s->write("TALLY OK " + tallyString() + "\r\n");
	} else if (cmd == "FUNCTION") {
		const int sp2 = rest.indexOf(' ');
		const QString fn = QString::fromUtf8(sp2 < 0 ? rest : rest.left(sp2));
		const auto params = parseQuery(sp2 < 0 ? QByteArray() : rest.mid(sp2 + 1), nullptr);
		const QString err = run(fn, params);
		s->write(err.isEmpty() ? QByteArray("FUNCTION OK Completed\r\n") : "FUNCTION ER " + err.toUtf8() + "\r\n");
	} else if (cmd == "XML") {
		const QByteArray x = xml() + "\r\n";
		s->write("XML " + QByteArray::number(x.size()) + "\r\n" + x);
	} else if (cmd == "QUIT") {
		s->write("QUIT OK Bye\r\n");
		s->disconnectFromHost();
	} else if (cmd == "VERSION") {
		s->write("VERSION OK 27.0.0.49\r\n");
	} else {
		s->write(cmd + " ER Unknown command\r\n");
	}
}

void VmixApi::broadcast(const QByteArray &line, const char *subscription)
{
	if (!tcp)
		return;
	for (QTcpSocket *c : tcp->findChildren<QTcpSocket *>())
		if (c->state() == QAbstractSocket::ConnectedState && c->property(subscription).toBool())
			c->write(line);
}

void VmixApi::tick()
{
	if (!tcp || !engineAlive())
		return;
	bool any = false;
	for (QTcpSocket *c : tcp->findChildren<QTcpSocket *>())
		any |= c->property("subTally").toBool() || c->property("subActs").toBool();
	if (!any)
		return;
	const QByteArray t = tallyString();
	if (t != lastTally) {
		lastTally = t;
		broadcast("TALLY OK " + t + "\r\n", "subTally");
	}
	/* qualquer mudança visível (títulos, overlays, replay) avisa os apps */
	QByteArray acts = t;
	if (engineAlive()) {
		const ChannelState st = engine().channel(0).state();
		acts += st.playing ? "P" : "p";
		acts += QByteArray::number((qulonglong)engine().eventsVersion());
	}
	acts += g_ftbOn ? "F" : "f";
	if (acts != lastActs) {
		const bool first = lastActs.isEmpty();
		lastActs = acts;
		if (!first)
			broadcast("ACTS OK Input 0 1\r\n", "subActs");
	}
}
