#pragma once
#include <QWidget>

class QPushButton;

/* Painel com os botões 1..8 dos canais de overlay. */
class OverlayDock : public QWidget {
	Q_OBJECT
public:
	explicit OverlayDock(QWidget *parent = nullptr);

private slots:
	void refresh();
	void openConfig(int focus);

private:
	QPushButton *buttons[8];
};
