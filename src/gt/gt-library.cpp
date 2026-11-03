/* Biblioteca de títulos GT: cada .gtzip importado vira uma fonte interna
 * (privada), sem precisar de cena. Vai ao ar pelos canais de overlay. */
#include "gt-source.hpp"
#include "../common.hpp"
#include <obs-frontend-api.h>
#include <QFileInfo>
#include <cstring>
#include <mutex>

static std::mutex g_libMtx;
static std::vector<obs_source_t *> g_lib;

static bool nameTaken(const QString &n)
{
	const std::string s = n.toStdString();
	obs_source_t *pub = obs_get_source_by_name(s.c_str());
	if (pub) {
		obs_source_release(pub);
		return true;
	}
	for (auto *x : g_lib)
		if (s == obs_source_get_name(x))
			return true;
	return false;
}

static void clearLibrary()
{
	std::lock_guard<std::mutex> lk(g_libMtx);
	for (auto *s : g_lib)
		obs_source_release(s);
	g_lib.clear();
}

static void saveCb(obs_data_t *save, bool saving, void *)
{
	if (saving) {
		obs_data_array_t *arr = obs_data_array_create();
		{
			std::lock_guard<std::mutex> lk(g_libMtx);
			for (auto *s : g_lib) {
				obs_data_t *o = obs_data_create();
				obs_data_set_string(o, "name", obs_source_get_name(s));
				obs_data_t *st = obs_source_get_settings(s);
				obs_data_set_obj(o, "settings", st);
				obs_data_release(st);
				obs_data_array_push_back(arr, o);
				obs_data_release(o);
			}
		}
		obs_data_set_array(save, "instant_replay_gt_library", arr);
		obs_data_array_release(arr);
		return;
	}
	clearLibrary();
	obs_data_array_t *arr = obs_data_get_array(save, "instant_replay_gt_library");
	const size_t n = arr ? obs_data_array_count(arr) : 0;
	std::lock_guard<std::mutex> lk(g_libMtx);
	for (size_t i = 0; i < n; i++) {
		obs_data_t *o = obs_data_array_item(arr, i);
		obs_data_t *st = obs_data_get_obj(o, "settings");
		obs_source_t *s = obs_source_create_private(GT_SOURCE_ID, obs_data_get_string(o, "name"), st);
		if (s)
			g_lib.push_back(s);
		obs_data_release(st);
		obs_data_release(o);
	}
	obs_data_array_release(arr);
	if (n)
		rlog(LOG_INFO, "GT: biblioteca com %zu título(s)", n);
}

static void eventCb(enum obs_frontend_event e, void *)
{
	if (e == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP || e == OBS_FRONTEND_EVENT_EXIT)
		clearLibrary();
}

void gtLibraryInit()
{
	obs_frontend_add_save_callback(saveCb, nullptr);
	obs_frontend_add_event_callback(eventCb, nullptr);
}

void gtLibraryShutdown()
{
	obs_frontend_remove_save_callback(saveCb, nullptr);
	obs_frontend_remove_event_callback(eventCb, nullptr);
	clearLibrary();
}

obs_source_t *gtLibraryFind(const char *name)
{
	if (!name)
		return nullptr;
	std::lock_guard<std::mutex> lk(g_libMtx);
	for (auto *s : g_lib)
		if (strcmp(obs_source_get_name(s), name) == 0)
			return obs_source_get_ref(s);
	return nullptr;
}

obs_source_t *gtLibraryAdd(const QString &file)
{
	std::lock_guard<std::mutex> lk(g_libMtx);
	const QString base = QFileInfo(file).completeBaseName();
	QString name = base;
	for (int i = 2; nameTaken(name); i++)
		name = QString("%1 %2").arg(base).arg(i);
	obs_data_t *st = obs_data_create();
	obs_data_set_string(st, "file", file.toUtf8().constData());
	obs_data_set_int(st, "overlay", 1);
	obs_source_t *s = obs_source_create_private(GT_SOURCE_ID, name.toUtf8().constData(), st);
	obs_data_release(st);
	if (!s)
		return nullptr;
	g_lib.push_back(s);
	return obs_source_get_ref(s);
}

obs_source_t *gtLibraryDuplicate(obs_source_t *src, const QString &wanted)
{
	std::lock_guard<std::mutex> lk(g_libMtx);
	QString base = wanted.trimmed().isEmpty() ? QString::fromUtf8(obs_source_get_name(src)) : wanted.trimmed();
	QString name = base;
	for (int i = 2; nameTaken(name); i++)
		name = QString("%1 %2").arg(base).arg(i);
	obs_data_t *orig = obs_source_get_settings(src);
	obs_data_t *st = obs_data_create_from_json(obs_data_get_json(orig));
	obs_data_release(orig);
	obs_source_t *s = obs_source_create_private(GT_SOURCE_ID, name.toUtf8().constData(), st);
	obs_data_release(st);
	if (!s)
		return nullptr;
	g_lib.push_back(s);
	return obs_source_get_ref(s);
}

bool gtRename(obs_source_t *src, const QString &wanted)
{
	std::lock_guard<std::mutex> lk(g_libMtx);
	const QString name = wanted.trimmed();
	if (name.isEmpty())
		return false;
	if (name == QString::fromUtf8(obs_source_get_name(src)))
		return true;
	if (nameTaken(name))
		return false;
	obs_source_set_name(src, name.toUtf8().constData());
	return true;
}

void gtLibraryRemove(obs_source_t *src)
{
	std::lock_guard<std::mutex> lk(g_libMtx);
	for (size_t i = 0; i < g_lib.size(); i++) {
		if (g_lib[i] == src) {
			obs_source_release(g_lib[i]);
			g_lib.erase(g_lib.begin() + (ptrdiff_t)i);
			return;
		}
	}
}

std::vector<obs_source_t *> gtLibraryList()
{
	std::lock_guard<std::mutex> lk(g_libMtx);
	std::vector<obs_source_t *> out;
	for (auto *s : g_lib)
		out.push_back(obs_source_get_ref(s));
	return out;
}
