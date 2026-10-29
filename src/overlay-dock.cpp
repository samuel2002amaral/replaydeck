#include "overlay-dock.hpp"
#include "i18n.hpp"
#include "overlay-channels.hpp"
#include "ui-theme.hpp"
#include <algorithm>
#include <obs-frontend-api.h>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#define FADE_KEY "*fade"

static bool collectSources(void *param, obs_source_t *src)
{
	auto *names = static_cast<QStringList *>(param);
	const uint32_t flags = obs_source_get_output_flags(src);
	if ((flags & OBS_SOURCE_VIDEO) && strcmp(obs_source_get_id(src), "instant_replay_output") != 0)
		names->append(QString::fromUtf8(obs_source_get_name(src)));
	return true;
}

OverlayDock::OverlayDock(QWidget *parent) : QWidget(parent)
{
	i18n::track(this);
	ui::apply(this);
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(6, 4, 6, 6);
	root->setSpacing(4);

	auto *top = new QHBoxLayout();
	top->addWidget(ui::caption(tl("Overlays"), this));
	top->addStretch(1);
	top->addWidget(ui::button(tl("Tirar todos"), tl("Tira todos os overlays do ar"), this, "ghost", [this] {
		for (int i = 0; i < OVERLAY_COUNT; i++)
			overlays().setOn(i, false);
		refresh();
	}));
	auto *cfg = ui::button(QString::fromUtf8("⚙"), tl("Escolhe a fonte e a transição de cada overlay (ou botão direito num overlay)"),
			       this, "ghost", [this] { openConfig(0); });
	top->addWidget(cfg);
	root->addLayout(top);

	auto *grid = new QGridLayout();
	grid->setSpacing(4);
	for (int i = 0; i < OVERLAY_COUNT; i++) {
		auto *b = ui::toggle(QString::number(i + 1), QString(), this);
		b->setProperty("tile", true);
		b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		b->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(b, &QPushButton::clicked, this, [this, i] {
			if (overlays().slot(i).source.empty())
				openConfig(i);
			else
				overlays().toggle(i);
			refresh();
		});
		connect(b, &QPushButton::customContextMenuRequested, this, [this, i] { openConfig(i); });
		buttons[i] = b;
		grid->addWidget(b, i / 4, i % 4);
	}
	root->addLayout(grid);
	root->addStretch(1);

	auto *timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, &OverlayDock::refresh);
	timer->start(150);
	refresh();
}

void OverlayDock::refresh()
{
	if (!overlaysAlive())
		return;
	for (int i = 0; i < OVERLAY_COUNT; i++) {
		const OverlaySlot s = overlays().slot(i);
		const QString src = QString::fromUtf8(s.source.c_str());
		const bool on = overlays().isOn(i);
		QString label = src.isEmpty() ? QString::number(i + 1)
					       : QString("%1\n%2").arg(i + 1).arg(fontMetrics().elidedText(src, Qt::ElideRight,
													     std::max(30, buttons[i]->width() - 12)));
		if (buttons[i]->text() != label)
			buttons[i]->setText(label);
		buttons[i]->setChecked(on);
		ui::setProp(buttons[i], "empty", src.isEmpty());
		buttons[i]->setToolTip(src.isEmpty()
					       ? tl("Overlay %1: vazio — clique para escolher a fonte").arg(i + 1)
					       : tl("Overlay %1: %2\nClique liga/desliga • botão direito configura")
							 .arg(i + 1)
							 .arg(src));
	}
}

void OverlayDock::openConfig(int focus)
{
	QStringList names;
	obs_enum_sources(collectSources, &names);
	obs_enum_scenes(collectSources, &names);
	names.sort(Qt::CaseInsensitive);

	QStringList trans;
	struct obs_frontend_source_list trl = {};
	obs_frontend_get_transitions(&trl);
	for (size_t i = 0; i < trl.sources.num; i++)
		trans << QString::fromUtf8(obs_source_get_name(trl.sources.array[i]));
	obs_frontend_source_list_free(&trl);

	QDialog dlg(this);
	dlg.setWindowTitle(tl("Canais de overlay"));
	ui::applyDialog(&dlg);
	auto *grid = new QGridLayout(&dlg);
	grid->addWidget(new QLabel(QString::fromUtf8("<b>Nº</b>")), 0, 0);
	grid->addWidget(new QLabel(tl("<b>Fonte ou cena</b>")), 0, 1);
	grid->addWidget(new QLabel(tl("<b>Transição</b>")), 0, 2);
	grid->addWidget(new QLabel(tl("<b>Duração</b>")), 0, 3);

	QComboBox *src[OVERLAY_COUNT], *tr[OVERLAY_COUNT];
	QSpinBox *ms[OVERLAY_COUNT];
	for (int i = 0; i < OVERLAY_COUNT; i++) {
		const OverlaySlot s = overlays().slot(i);
		grid->addWidget(new QLabel(QString("<b>%1</b>").arg(i + 1)), i + 1, 0);

		src[i] = new QComboBox(&dlg);
		src[i]->addItem(tl("Nenhuma"), QString());
		for (const QString &n : names)
			src[i]->addItem(n, n);
		const QString cur = QString::fromUtf8(s.source.c_str());
		int idx = src[i]->findData(cur);
		if (idx < 0 && !cur.isEmpty()) {
			src[i]->addItem(cur + tl(" (não encontrada)"), cur);
			idx = src[i]->count() - 1;
		}
		src[i]->setCurrentIndex(idx < 0 ? 0 : idx);
		src[i]->setMinimumWidth(220);
		grid->addWidget(src[i], i + 1, 1);

		tr[i] = new QComboBox(&dlg);
		tr[i]->addItem(tl("Corte"), QString());
		tr[i]->addItem(tl("Fade"), QString(FADE_KEY));
		for (const QString &n : trans)
			tr[i]->addItem(n, n);
		tr[i]->setCurrentIndex(qMax(0, tr[i]->findData(QString::fromUtf8(s.transition.c_str()))));
		grid->addWidget(tr[i], i + 1, 2);

		ms[i] = new QSpinBox(&dlg);
		ms[i]->setRange(0, 10000);
		ms[i]->setSingleStep(100);
		ms[i]->setSuffix(" ms");
		ms[i]->setValue(s.durationMs);
		ms[i]->setToolTip(tl("Stingers usam a duração do próprio gráfico"));
		grid->addWidget(ms[i], i + 1, 3);
	}
	auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	if (auto *okB = bb->button(QDialogButtonBox::Ok)) {
		okB->setText(tl("Salvar"));
		ui::setKind(okB, "primary");
	}
	if (auto *cB = bb->button(QDialogButtonBox::Cancel))
		cB->setText(tl("Cancelar"));
	grid->addWidget(bb, OVERLAY_COUNT + 1, 0, 1, 4);
	connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	if (focus >= 0 && focus < OVERLAY_COUNT)
		src[focus]->setFocus();

	if (dlg.exec() != QDialog::Accepted)
		return;
	for (int i = 0; i < OVERLAY_COUNT; i++) {
		OverlaySlot s;
		s.source = src[i]->currentData().toString().toUtf8().constData();
		s.transition = tr[i]->currentData().toString().toUtf8().constData();
		s.durationMs = ms[i]->value();
		overlays().setSlot(i, s);
	}
	refresh();
}
