#include "overlay-channels.hpp"
#include "common.hpp"
#include "gt/gt-source.hpp"
#include <chrono>
#include <thread>
#include <util/platform.h>
#include <cstdlib>
#include <cstring>

/* canais 50..57: acima do programa (0) e abaixo do stinger do replay (62) */
#define OVERLAY_FIRST_CHANNEL 50
#define FADE_KEY "*fade"

static OverlayChannels *g_ov = nullptr;

OverlayChannels &overlays()
{
	return *g_ov;
}
void overlaysCreate()
{
	if (!g_ov)
		g_ov = new OverlayChannels();
}
void overlaysDestroy()
{
	delete g_ov;
	g_ov = nullptr;
}
bool overlaysAlive()
{
	return g_ov != nullptr;
}

OverlayChannels::OverlayChannels()
{
	load();
}

OverlayChannels::~OverlayChannels()
{
	releaseAll();
}

static std::string configFile()
{
	std::string dir;
	if (const char *over = getenv("REPLAY_CONFIG_DIR")) {
		dir = over;
	} else {
		if (!obs_current_module())
			return std::string();
		char *p = obs_module_config_path("");
		dir = p ? p : "";
		bfree(p);
	}
	if (dir.empty())
		return dir;
	os_mkdirs(dir.c_str());
	if (dir.back() != '/' && dir.back() != '\\')
		dir += "/";
	return dir + "overlays.json";
}

void OverlayChannels::load()
{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	const std::string file = configFile();
	if (file.empty())
		return;
	obs_data_t *d = obs_data_create_from_json_file_safe(file.c_str(), "bak");
	if (!d)
		return;
	obs_data_array_t *arr = obs_data_get_array(d, "overlays");
	const size_t n = arr ? obs_data_array_count(arr) : 0;
	for (size_t i = 0; i < n && i < OVERLAY_COUNT; i++) {
		obs_data_t *o = obs_data_array_item(arr, i);
		obs_data_set_default_int(o, "duration_ms", 300);
		cfgSlots[i].source = obs_data_get_string(o, "source");
		cfgSlots[i].transition = obs_data_get_string(o, "transition");
		cfgSlots[i].durationMs = (int)obs_data_get_int(o, "duration_ms");
		obs_data_release(o);
	}
	obs_data_array_release(arr);
	obs_data_release(d);
}

void OverlayChannels::save()
{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	const std::string file = configFile();
	if (file.empty())
		return;
	obs_data_t *d = obs_data_create();
	obs_data_array_t *arr = obs_data_array_create();
	for (auto &s : cfgSlots) {
		obs_data_t *o = obs_data_create();
		obs_data_set_string(o, "source", s.source.c_str());
		obs_data_set_string(o, "transition", s.transition.c_str());
		obs_data_set_int(o, "duration_ms", s.durationMs);
		obs_data_array_push_back(arr, o);
		obs_data_release(o);
	}
	obs_data_set_array(d, "overlays", arr);
	obs_data_array_release(arr);
	obs_data_save_json_safe(d, file.c_str(), "tmp", "bak");
	obs_data_release(d);
}

OverlaySlot OverlayChannels::slot(int i)
{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	return (i >= 0 && i < OVERLAY_COUNT) ? cfgSlots[i] : OverlaySlot();
}

void OverlayChannels::setSlot(int i, const OverlaySlot &s)
{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	if (i < 0 || i >= OVERLAY_COUNT)
		return;
	const bool wasOn = rt[i].on;
	const bool changed = s.source != cfgSlots[i].source || s.transition != cfgSlots[i].transition;
	if (wasOn && changed)
		setOn(i, false);
	cfgSlots[i] = s;
	if (changed)
		freeRuntime(i);
	save();
}

bool OverlayChannels::isOn(int i)
{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	return i >= 0 && i < OVERLAY_COUNT && rt[i].on;
}

void OverlayChannels::toggle(int i)
{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	if (i >= 0 && i < OVERLAY_COUNT)
		setOn(i, !rt[i].on);
}

static obs_source_t *findFrontendTransition(const std::string &name)
{
	/* procura entre as transições do OBS pelo nome (são fontes privadas) */
	struct Ctx {
		const std::string *name;
		obs_source_t *found;
	} ctx{&name, nullptr};
	obs_enum_all_sources(
		[](void *p, obs_source_t *src) {
			auto *c = static_cast<Ctx *>(p);
			if (obs_source_get_type(src) == OBS_SOURCE_TYPE_TRANSITION && obs_source_get_name(src) &&
			    *c->name == obs_source_get_name(src) &&
			    strncmp(obs_source_get_name(src), "instant-replay", 14) != 0) {
				c->found = obs_source_get_ref(src);
				return false;
			}
			return true;
		},
		&ctx);
	return ctx.found;
}

void OverlayChannels::ensureTransition(int i)
{
	/* chamado com mtx travado */
	Runtime &r = rt[i];
	const std::string key = cfgSlots[i].transition;
	if (r.trans && r.transKey == key)
		return;
	freeRuntime(i);
	if (key.empty())
		return; /* corte: sem transição */

	char name[48];
	snprintf(name, sizeof(name), "instant-replay-overlay-%d", i + 1);
	if (key == FADE_KEY) {
		r.trans = obs_source_create_private("fade_transition", name, nullptr);
	} else {
		obs_source_t *templ = findFrontendTransition(key);
		if (templ) {
			obs_data_t *st = obs_source_get_settings(templ);
			r.trans = obs_source_create_private(obs_source_get_id(templ), name, st);
			obs_data_release(st);
			obs_source_release(templ);
		} else {
			rlog(LOG_WARNING, "overlay %d: transição '%s' não encontrada, usando fade", i + 1, key.c_str());
			r.trans = obs_source_create_private("fade_transition", name, nullptr);
		}
	}
	if (!r.trans)
		return;
	snprintf(name, sizeof(name), "instant-replay-overlay-empty-%d", i + 1);
	r.empty = obs_scene_create_private(name);
	obs_transition_set(r.trans, obs_scene_get_source(r.empty));
	r.transKey = key;
}

void OverlayChannels::freeRuntime(int i)
{
	/* chamado com mtx travado */
	Runtime &r = rt[i];
	if (r.trans) {
		obs_set_output_source(OVERLAY_FIRST_CHANNEL + i, nullptr);
		obs_transition_clear(r.trans);
		obs_source_release(r.trans);
		r.trans = nullptr;
	}
	if (r.empty) {
		obs_scene_release(r.empty);
		r.empty = nullptr;
	}
	r.transKey.clear();
}

/* fonte comum pelo nome, ou um título da biblioteca GT */
static obs_source_t *findOverlaySource(const std::string &name)
{
	if (name.empty())
		return nullptr;
	obs_source_t *src = obs_get_source_by_name(name.c_str());
	if (!src)
		src = gtLibraryFind(name.c_str());
	return src;
}

void OverlayChannels::setOn(int i, bool on)
{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	if (i < 0 || i >= OVERLAY_COUNT)
		return;
	Runtime &r = rt[i];
	const int ch = OVERLAY_FIRST_CHANNEL + i;
	const OverlaySlot &s = cfgSlots[i];

	if (on) {
		obs_source_t *src = findOverlaySource(s.source);
		if (!src) {
			rlog(LOG_WARNING, "overlay %d: fonte '%s' não encontrada", i + 1, s.source.c_str());
			return;
		}
		r.gen++;
		if (gtIsSource(src)) {
			/* título GT: usa as animações do próprio título */
			obs_set_output_source(ch, src);
			gtPlayIn(src);
			obs_source_release(src);
			r.on = true;
			return;
		}
		ensureTransition(i);
		if (r.trans) {
			/* parte sempre do vazio (o OBS nem sempre troca as fontes no fim) */
			obs_transition_set(r.trans, obs_scene_get_source(r.empty));
			obs_set_output_source(ch, r.trans);
			obs_transition_start(r.trans, OBS_TRANSITION_MODE_AUTO,
					     (uint32_t)(s.durationMs > 0 ? s.durationMs : 300), src);
		} else {
			obs_set_output_source(ch, src);
		}
		obs_source_release(src);
		r.on = true;
	} else {
		obs_source_t *cur = obs_get_output_source(ch);
		if (cur && gtIsSource(cur)) {
			/* título GT: anima a saída e só depois tira do canal */
			gtPlayOut(cur);
			const int ms = gtOutLengthMs(cur) + 150;
			const uint64_t gen = ++r.gen;
			obs_weak_source_t *weak = obs_source_get_weak_source(cur);
			obs_source_release(cur);
			r.on = false;
			std::thread([this, i, ch, ms, gen, weak] {
				std::this_thread::sleep_for(std::chrono::milliseconds(ms));
				if (overlaysAlive()) {
					std::lock_guard<std::recursive_mutex> lk2(mtx);
					obs_source_t *now = obs_get_output_source(ch);
					obs_source_t *mine = obs_weak_source_get_source(weak);
					if (rt[i].gen == gen && !rt[i].on && now == mine)
						obs_set_output_source(ch, nullptr);
					obs_source_release(now);
					obs_source_release(mine);
				}
				obs_weak_source_release(weak);
			}).detach();
			return;
		}
		obs_source_release(cur);
		r.gen++;
		if (r.trans && r.empty) {
			/* sai com a mesma transição; a cena vazia não desenha nada */
			obs_source_t *src = findOverlaySource(s.source);
			if (src)
				obs_transition_set(r.trans, src);
			obs_source_release(src);
			obs_transition_start(r.trans, OBS_TRANSITION_MODE_AUTO,
					     (uint32_t)(s.durationMs > 0 ? s.durationMs : 300),
					     obs_scene_get_source(r.empty));
		} else {
			obs_set_output_source(ch, nullptr);
		}
		r.on = false;
	}
}

void OverlayChannels::assignOn(int i, const std::string &source)
{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	if (i < 0 || i >= OVERLAY_COUNT)
		return;
	if (cfgSlots[i].source != source) {
		if (rt[i].on) {
			/* troca direto: o título novo entra no lugar do anterior */
			rt[i].on = false;
			obs_source_t *cur = obs_get_output_source(OVERLAY_FIRST_CHANNEL + i);
			if (cur && gtIsSource(cur))
				gtPlayOut(cur);
			obs_source_release(cur);
		}
		cfgSlots[i].source = source;
		freeRuntime(i);
		save();
	}
	setOn(i, true);
}

int OverlayChannels::channelOf(const std::string &source)
{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	for (int i = 0; i < OVERLAY_COUNT; i++)
		if (rt[i].on && cfgSlots[i].source == source)
			return i;
	return -1;
}

void OverlayChannels::releaseAll()
{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	for (int i = 0; i < OVERLAY_COUNT; i++) {
		rt[i].on = false;
		obs_set_output_source(OVERLAY_FIRST_CHANNEL + i, nullptr);
		freeRuntime(i);
	}
}
