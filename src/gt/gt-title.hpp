#pragma once
#include <QImage>
#include <QColor>
#include <QString>
#include <map>
#include <memory>
#include <string>
#include <vector>

/* Modelo de um título .gtzip (formato do GT Designer do vMix).
 *  - document.xml: camadas, elementos e storyboards de animação
 *  - resources.xml: mapa "caminho da imagem" -> arquivo dentro do zip */

namespace gt {

enum class ElemType { Text, Image, Rectangle, Ellipse, Unknown };

struct Brush {
	bool bitmap = false;
	QColor color = QColor(0, 0, 0, 0);
	QString source; /* caminho da imagem (dentro do zip) */
};

struct Element {
	ElemType type = ElemType::Unknown;
	QString name;
	double x = 0, y = 0, w = 0, h = 0; /* relativo à camada */
	double opacity = 1;
	bool visible = true;
	Brush fill;
	Brush stroke;
	double strokeThickness = 0;
	/* texto */
	QString text;
	QString fontFamily = "Arial";
	double fontSize = 20;
	QString fontWeight = "Regular";
	QString fontStyle;
	QString textAlign = "Left";
	QString verticalAlign = "Top";
	double lineSpacing = 1;
	/* recorte: área mantida (0..1) e suavização em px */
	bool hasCrop = false;
	double cropL = 0, cropT = 0, cropR = 1, cropB = 1;
	double featherL = 0, featherT = 0, featherR = 0, featherB = 0;
};

struct Layer {
	QString name;
	double x = 0, y = 0, w = 0, h = 0;
	bool visible = true;
	std::vector<Element> elements;
};

struct Animation {
	QString kind;      /* Fly, Zoom, ZoomFade, Fade... */
	QString object;    /* nome do elemento ou da camada */
	double delay = 0;  /* s */
	double duration = 0.5;
	QString interpolation = "Linear";
	QString direction; /* Left/Right/Top/Bottom */
};

struct Storyboard {
	QString type; /* TransitionIn, TransitionOut, Page1... */
	std::vector<Animation> anims;
	double length() const;
};

/* Um campo editável, como no Title Editor do vMix */
struct Field {
	enum Kind { Text, Image, Color } kind;
	QString key;   /* "TITULO TIME.Text", "CAMPO1.Fill.Bitmap", "ESCUDO.Source", "X.Fill.Color" */
	QString label;
	int layer = -1, element = -1;
};

class Title {
public:
	bool load(const std::string &path, std::string &err);

	int width = 1920, height = 1080;
	std::vector<Layer> layers;
	std::vector<Storyboard> storyboards;

	const Storyboard *storyboard(const QString &type) const;
	std::vector<Field> fields() const;

	/* valores originais do arquivo, por chave de campo */
	QString originalValue(const QString &key) const;

	/* imagem de dentro do zip (com cache) */
	QImage zipImage(const QString &source) const;

	/* desenha um elemento no tamanho dele, já com os valores editados.
	 * overrides: chave de campo -> valor (texto, caminho de arquivo, #AARRGGBB) */
	QImage renderElement(const Element &e, const std::map<QString, QString> &overrides) const;

	/* título inteiro parado (para prévia e testes) */
	QImage renderStatic(const std::map<QString, QString> &overrides) const;

private:
	std::map<QString, QByteArray> files;       /* nome no zip -> bytes */
	std::map<QString, QString> resourceMap;     /* caminho -> nome no zip */
	mutable std::map<QString, QImage> imgCache;
};

/* curvas de aceleração do GT (CubicEasingInOut etc.) */
double ease(const QString &interpolation, double t);

} // namespace gt
