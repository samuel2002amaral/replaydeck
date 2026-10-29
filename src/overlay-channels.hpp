#pragma once
#include <obs.h>
#include <mutex>
#include <string>

#define OVERLAY_COUNT 8

/* Canais de overlay (como no vMix): até 8 fontes/cenas que, ligadas, ficam
 * sempre por cima do programa, seja qual for a cena no ar. Cada canal usa um
 * canal de saída alto do OBS, desenhado acima da cena do programa. */
struct OverlaySlot {
	std::string source;     /* fonte ou cena */
	std::string transition; /* "" = corte, "*fade" = fade, ou nome de uma transição do OBS */
	int durationMs = 300;
};

class OverlayChannels {
public:
	OverlayChannels();
	~OverlayChannels();

	void load();
	void save();
	OverlaySlot slot(int i);
	void setSlot(int i, const OverlaySlot &s);

	void toggle(int i);
	/* coloca uma fonte no canal (substitui a anterior) e liga */
	void assignOn(int i, const std::string &source);
	/* canal onde a fonte está ligada (-1 se nenhum) */
	int channelOf(const std::string &source);
	void setOn(int i, bool on);
	bool isOn(int i);
	/* desliga tudo e solta as fontes (troca de coleção de cenas / saída) */
	void releaseAll();

private:
	struct Runtime {
		obs_source_t *trans = nullptr;
		obs_scene_t *empty = nullptr;
		std::string transKey;
		bool on = false;
		uint64_t gen = 0;
	};
	void ensureTransition(int i);
	void freeRuntime(int i);

	std::recursive_mutex mtx;
	OverlaySlot cfgSlots[OVERLAY_COUNT];
	Runtime rt[OVERLAY_COUNT];
};

OverlayChannels &overlays();
void overlaysCreate();
void overlaysDestroy();
bool overlaysAlive();
