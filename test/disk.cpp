/* Gravações no disco: listar, apagar e limite automático (sem câmeras). */
#include "../src/engine.hpp"
#include <obs-module.h>
#include <util/platform.h>
#include <cstdio>
#include <fstream>
#include <filesystem>

OBS_DECLARE_MODULE()
void registerReplaySource();

static void fake(const std::string &dir, int mb)
{
	std::filesystem::create_directories(dir);
	std::ofstream(dir + "/session.json") << "{\"fps_num\":30,\"fps_den\":1}";
	std::ofstream f(dir + "/cam1.h264", std::ios::binary);
	std::string block(1024 * 1024, 'x');
	for (int i = 0; i < mb; i++)
		f << block;
}

static void show(const char *label)
{
	printf("%s\n", label);
	for (const SessionInfo &s : engine().listSessions())
		printf("   %s  %4llu MB  eventos=%d\n", s.name.c_str(), (unsigned long long)(s.bytes >> 20), s.events);
	fflush(stdout);
}

int main()
{
	obs_startup("en-US", nullptr, nullptr);
	struct obs_video_info ovi = {};
	ovi.graphics_module = "libobs-opengl";
	ovi.fps_num = 30;
	ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = 640;
	ovi.base_height = ovi.output_height = 360;
	ovi.output_format = VIDEO_FORMAT_NV12;
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS)
		return 1;
	registerReplaySource();
	registerReplayCaptureOutput();
	engineCreate();
	const std::string base = "/tmp/disktest";
	std::filesystem::remove_all(base);
	fake(base + "/Replay_2026-11-17_20-00-00", 30);
	fake(base + "/Replay_2026-11-18_20-00-00", 40);
	fake(base + "/Replay_2026-11-19_20-00-00", 50);
	ReplayConfig c = engine().config();
	c.outputDir = base;
	c.cacheLimitGB = 0;
	c.minFreeGB = 0;
	engine().setConfig(c);
	show("antes:");
	std::string err;
	printf("apagar a do dia 17: %s\n", engine().deleteSession(base + "/Replay_2026-11-17_20-00-00", err) ? "ok" : err.c_str());
	show("depois de apagar:");
	/* limite minúsculo (1 GB) não apaga nada (90 MB < 1 GB) */
	c.cacheLimitGB = 1;
	engine().setConfig(c);
	printf("limite 1 GB: '%s'\n", engine().enforceDiskLimits().c_str());
	/* espaço livre mínimo absurdo: apaga todas as antigas (não está gravando: todas) */
	c.minFreeGB = 100000;
	engine().setConfig(c);
	printf("mínimo livre enorme: '%s'\n", engine().enforceDiskLimits().c_str());
	show("final:");
	fake(base + "/Replay_2026-11-20_10-00-00", 20);
	fake(base + "/Replay_2026-11-21_10-00-00", 25);
	c.minFreeGB = 0;
	engine().setConfig(c);
	std::string e2;
	const uint64_t freed = engine().clearAllCache(e2);
	printf("limpar tudo: %llu MB liberados, erros='%s'\n", (unsigned long long)(freed >> 20), e2.c_str());
	show("depois de limpar tudo:");
	fflush(stdout);
	_Exit(0);
}
