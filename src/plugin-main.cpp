#include "engine.hpp"
#include "i18n.hpp"
#include "replay-dock.hpp"
#include "air-control.hpp"
#include "overlay-channels.hpp"
#include "overlay-dock.hpp"
#include "gt/gt-dock.hpp"
#include "gt/gt-source.hpp"
#include "vmix-api.hpp"
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("replaydeck", "en-US")

MODULE_EXPORT const char *obs_module_name(void)
{
	return "ReplayDeck";
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Multi-camera instant replay controller with dual playback channels, slow motion, and event lists";
}

void registerReplaySource();

struct Hotkey {
	std::string name;
	std::string desc;
	std::function<void()> fn;
	obs_hotkey_id id = OBS_INVALID_HOTKEY_ID;
	std::function<void(bool)> hold;
};

static std::vector<Hotkey> hotkeys;

size_t pluginHotkeyCount()
{
	return hotkeys.size();
}

const char *pluginHotkeyDesc(size_t i)
{
	return i < hotkeys.size() ? tlc(hotkeys[i].desc.c_str()) : "";
}

void pluginRunHotkey(size_t i)
{
	if (i < hotkeys.size())
		hotkeys[i].fn();
}

static void hotkeyCb(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	const Hotkey &h = hotkeys[reinterpret_cast<size_t>(data)];
	if (h.hold) {
		h.hold(pressed);
		return;
	}
	if (pressed && h.fn)
		h.fn();
}

static void playLastEvent()
{
	if (AirControl *a = AirControl::instance())
		QMetaObject::invokeMethod(a, "toggleLast", Qt::QueuedConnection);
}

static void playEventsHotkey()
{
	if (AirControl *a = AirControl::instance())
		QMetaObject::invokeMethod(a, [a] { a->toggleEvents(0); }, Qt::QueuedConnection);
}

static void buildHotkeys()
{
	auto add = [](std::string name, std::string desc, std::function<void()> fn) {
		hotkeys.push_back(Hotkey{std::move(name), std::move(desc), std::move(fn), OBS_INVALID_HOTKEY_ID, nullptr});
	};

	add("replay.rec", N_("Replay: iniciar/parar gravação"), [] {
		std::string err;
		if (engine().recording())
			engine().stopRecording();
		else
			engine().startRecording(err);
	});
	add("replay.play", N_("Replay: play"), [] { engine().play(); });
	add("replay.reverse", N_("Replay: ligar/desligar reverso"), [] { engine().playReverse(); });
	add("replay.pause", N_("Replay: pausa"), [] { engine().pause(); });
	add("replay.toggle", N_("Replay: play/pausa"), [] { engine().togglePlay(); });
	add("replay.ff", N_("Replay: fast forward (2x/4x/8x/16x)"), [] { engine().fastForward(); });
	add("replay.live", N_("Replay: voltar ao vivo"), [] { engine().goLive(); });
	add("replay.frame_back", N_("Replay: quadro anterior"), [] { engine().step(-1); });
	add("replay.frame_fwd", N_("Replay: próximo quadro"), [] { engine().step(1); });
	add("replay.back_1s", N_("Replay: voltar 1 s"), [] { engine().jumpSeconds(-1); });
	add("replay.fwd_1s", N_("Replay: avançar 1 s"), [] { engine().jumpSeconds(1); });
	add("replay.back_5s", N_("Replay: voltar 5 s"), [] { engine().jumpSeconds(-5); });
	add("replay.fwd_5s", N_("Replay: avançar 5 s"), [] { engine().jumpSeconds(5); });
	add("replay.mark_in", N_("Replay: Mark In"), [] { engine().markIn(); });
	add("replay.mark_out", N_("Replay: Mark Out"), [] { engine().markOut(); });
	add("replay.auto_5", N_("Replay: evento automático 5 s"), [] { engine().autoEvent(5); });
	add("replay.auto_10", N_("Replay: evento automático 10 s"), [] { engine().autoEvent(10); });
	add("replay.auto_20", N_("Replay: evento automático 20 s"), [] { engine().autoEvent(20); });
	add("replay.play_last", N_("Replay: tocar último evento (inicia / encerra)"), playLastEvent);
	add("replay.play_events", N_("Replay: Play Events (inicia / encerra)"), playEventsHotkey);
	add("replay.restart", N_("Replay: voltar ao início do clipe"), [] { engine().restartClip(); });
	add("replay.loop", N_("Replay: ligar/desligar Loop"), [] { engine().setLoop(!engine().loopOn()); });
	add("replay.audio", N_("Replay: ligar/desligar áudio do replay"), [] { engine().setAudio(!engine().audioOn()); });
	add("replay.export", N_("Replay: exportar eventos da lista"), [] { engine().exportEvents(engine().activeList(), 0); });
	add("replay.take_in", N_("Replay: pôr no ar (transição de entrada)"), [] {
		if (AirControl::instance())
			AirControl::instance()->requestIn();
	});
	add("replay.take_out", N_("Replay: tirar do ar (volta à cena anterior)"), [] {
		if (AirControl::instance())
			AirControl::instance()->requestOut();
	});
	add("replay.sel_a", N_("Replay: selecionar canal A"), [] { engine().setSelection(SEL_A); });
	add("replay.sel_b", N_("Replay: selecionar canal B"), [] { engine().setSelection(SEL_B); });
	add("replay.sel_ab", N_("Replay: selecionar A+B (sincronizados)"), [] { engine().setSelection(SEL_AB); });
	add("replay.sync", N_("Replay: sincronizar B com A"), [] { engine().syncBToA(); });

	auto addHold = [](std::string name, std::string desc, int axis, int dir) {
		hotkeys.push_back(Hotkey{
			std::move(name),
			std::move(desc),
			[axis, dir] {
				if (axis == 2)
					engine().zoomStep(dir > 0 ? 1.25f : 0.8f);
				else
					engine().zoomStep(1.0f, axis == 0 ? dir * 0.1f : 0.0f, axis == 1 ? dir * 0.1f : 0.0f);
			},
			OBS_INVALID_HOTKEY_ID,
			[axis, dir](bool pressed) { engine().zoomMove(axis, pressed ? dir : 0); }});
	};

	addHold("replay.zoom_in", N_("Replay: zoom — aproximar (segure)"), 2, 1);
	addHold("replay.zoom_out", N_("Replay: zoom — afastar (segure)"), 2, -1);
	addHold("replay.zoom_left", N_("Replay: zoom — mover para a esquerda (segure)"), 0, -1);
	addHold("replay.zoom_right", N_("Replay: zoom — mover para a direita (segure)"), 0, 1);
	addHold("replay.zoom_up", N_("Replay: zoom — mover para cima (segure)"), 1, -1);
	addHold("replay.zoom_down", N_("Replay: zoom — mover para baixo (segure)"), 1, 1);
	add("replay.zoom_reset", N_("Replay: zoom — voltar à imagem inteira"), [] { engine().zoomReset(); });
	add("replay.speed_down", N_("Replay: velocidade -5%"), [] { engine().setSpeedPercent(engine().speedPercent() - 5); });
	add("replay.speed_up", N_("Replay: velocidade +5%"), [] { engine().setSpeedPercent(engine().speedPercent() + 5); });

	for (int pct : {0, 10, 25, 33, 50, 75, 100}) {
		add("replay.speed_" + std::to_string(pct),
		    tl("Replay: velocidade %1%").arg(pct).toStdString(),
		    [pct] { engine().setSpeedPercent(pct); });
	}
	for (int ov = 0; ov < OVERLAY_COUNT; ov++) {
		add("replay.overlay_" + std::to_string(ov + 1),
		    tl("Overlay %1: liga/desliga").arg(ov + 1).toStdString(),
		    [ov] {
			    if (overlaysAlive())
				    overlays().toggle(ov);
		    });
	}
	for (int cam = 0; cam < MAX_CAMERAS; cam++) {
		add("replay.cam_" + std::to_string(cam + 1),
		    tl("Replay: câmera %1").arg(cam + 1).toStdString(),
		    [cam] { engine().setAngle(cam); });
	}

	for (size_t i = 0; i < hotkeys.size(); i++) {
		hotkeys[i].id = obs_hotkey_register_frontend(
			hotkeys[i].name.c_str(),
			tlc(hotkeys[i].desc.c_str()),
			hotkeyCb,
			reinterpret_cast<void *>(i));
	}
}

static void saveCb(obs_data_t *saveData, bool saving, void *)
{
	if (saving) {
		obs_data_t *o = obs_data_create();
		for (auto &h : hotkeys) {
			obs_data_array_t *a = obs_hotkey_save(h.id);
			obs_data_set_array(o, h.name.c_str(), a);
			obs_data_array_release(a);
		}
		obs_data_set_obj(saveData, "instant_replay_hotkeys", o);
		obs_data_release(o);
	} else {
		obs_data_t *o = obs_data_get_obj(saveData, "instant_replay_hotkeys");
		if (!o)
			return;
		for (auto &h : hotkeys) {
			obs_data_array_t *a = obs_data_get_array(o, h.name.c_str());
			obs_hotkey_load(h.id, a);
			obs_data_array_release(a);
		}
		obs_data_release(o);
	}
}

static void frontendEvent(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP || event == OBS_FRONTEND_EVENT_EXIT) {
		if (engineAlive())
			engine().stopRecording();
		if (overlaysAlive())
			overlays().releaseAll();
	}
}

/* Migrate configuration directory from legacy module name (obs-instant-replay) */
static void migrateOldConfig()
{
	char *p = obs_module_config_path("");
	if (!p)
		return;
	namespace fs = std::filesystem;
	std::error_code ec;
	fs::path cur = fs::u8path(p);
	bfree(p);
	cur = cur.lexically_normal();
	if (!cur.has_filename())
		cur = cur.parent_path();
	const fs::path old = cur.parent_path() / "obs-instant-replay";
	if (fs::exists(cur, ec) && !fs::is_empty(cur, ec))
		return;
	if (!fs::is_directory(old, ec))
		return;
	fs::create_directories(cur, ec);
	fs::copy(old, cur, fs::copy_options::recursive | fs::copy_options::skip_existing, ec);
	rlog(LOG_INFO, "Migrated legacy configuration from obs-instant-replay");
}

bool obs_module_load(void)
{
	migrateOldConfig();
	engineCreate();
	registerReplaySource();
	registerReplayCaptureOutput();
	registerGtSource();
	buildHotkeys();

	auto *mainWin = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	new AirControl(mainWin);
	overlaysCreate();
	obs_frontend_add_dock_by_id("instant-replay-overlays", tlc("ReplayDeck (overlays)"), new OverlayDock(mainWin));
	gtLibraryInit();
	obs_frontend_add_dock_by_id("instant-replay-gt", tlc("ReplayDeck GT (títulos)"), new GtDock(mainWin));
	obs_frontend_add_dock_by_id("instant-replay-dock", "ReplayDeck", new ReplayDock(mainWin));

	obs_frontend_add_save_callback(saveCb, nullptr);
	obs_frontend_add_event_callback(frontendEvent, nullptr);
	VmixApi::create(mainWin);

	rlog(LOG_INFO, "Loaded (version %s, build %s)", PLUGIN_VERSION, PLUGIN_BUILD);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_save_callback(saveCb, nullptr);
	obs_frontend_remove_event_callback(frontendEvent, nullptr);
	VmixApi::destroy();
	delete AirControl::instance();
	overlaysDestroy();
	gtLibraryShutdown();
	for (auto &h : hotkeys)
		obs_hotkey_unregister(h.id);
	hotkeys.clear();
	engineDestroy();
}
