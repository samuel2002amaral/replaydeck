#include "engine.hpp"
#include <obs-module.h>
#include <util/platform.h>
#include <cstdio>
OBS_DECLARE_MODULE()
void registerReplaySource();
size_t pluginHotkeyCount() { return 0; }
const char *pluginHotkeyDesc(size_t) { return ""; }
void pluginRunHotkey(size_t) {}
int main() {
	obs_startup("en-US", nullptr, nullptr);
	obs_video_info ovi = {}; ovi.graphics_module = "libobs-opengl"; ovi.fps_num = 30; ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = 640; ovi.base_height = ovi.output_height = 360; ovi.output_format = VIDEO_FORMAT_NV12;
	obs_reset_video(&ovi); registerReplaySource(); registerReplayCaptureOutput(); engineCreate();
	auto show = [](const char *l) { auto z = engine().channel(0).zoom(); auto b = engine().channel(1).zoom(); printf("%-16s A z=%.2f x=%.3f y=%.3f | B z=%.2f x=%.3f\n", l, z.z, z.x, z.y, b.z, b.x); };
	engine().zoomPresetByKey("goal_left"); os_sleep_ms(50); show("goal_left");
	engine().zoomPresetByKey("pan_right");
	for (int i = 0; i <= 9; i++) { char b[32]; snprintf(b, 32, "pan t=%.1fs", i * 0.5); show(b); os_sleep_ms(500); }
	engine().zoomPresetByKey("push_in"); os_sleep_ms(1000); show("push_in 1s");
	engine().zoomMove(0, 1); os_sleep_ms(300); engine().zoomMove(0, 0); show("mao interrompe"); os_sleep_ms(1500); show("depois");
	engine().zoomSet(9, 2, -1); show("set fora (clamp)");
	fflush(stdout); _Exit(0);
}
