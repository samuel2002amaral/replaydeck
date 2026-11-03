#include "gt-title.hpp"
#include "../third_party/miniz/miniz.h"

#include <QFile>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <cmath>

namespace gt {

/* ---------------- XML mínimo em árvore ---------------- */

struct XNode {
	QString tag;
	std::map<QString, QString> attrs;
	std::vector<XNode> kids;
	QString text;

	QString attr(const QString &k, const QString &def = QString()) const
	{
		auto it = attrs.find(k);
		return it == attrs.end() ? def : it->second;
	}
	const XNode *child(const QString &t) const
	{
		for (auto &k : kids)
			if (k.tag == t)
				return &k;
		return nullptr;
	}
	/* caminho "A/B/C" */
	const XNode *path(const QString &p) const
	{
		const XNode *n = this;
		for (const QString &part : p.split('/')) {
			n = n->child(part);
			if (!n)
				return nullptr;
		}
		return n;
	}
};

static bool parseXml(const QByteArray &data, XNode &root)
{
	QXmlStreamReader r(data);
	std::vector<XNode *> stack;
	bool gotRoot = false;
	while (!r.atEnd()) {
		r.readNext();
		if (r.isStartElement()) {
			XNode *n;
			if (!gotRoot) {
				n = &root;
				gotRoot = true;
			} else {
				stack.back()->kids.emplace_back();
				n = &stack.back()->kids.back();
			}
			n->tag = r.name().toString();
			for (const auto &a : r.attributes())
				n->attrs[a.name().toString()] = a.value().toString();
			stack.push_back(n);
		} else if (r.isEndElement()) {
			if (!stack.empty())
				stack.pop_back();
		} else if (r.isCharacters() && !stack.empty()) {
			stack.back()->text += r.text().toString();
		}
	}
	return gotRoot && !r.hasError();
}

/* ---------------- leitura ---------------- */

static void vec2(const QString &s, double &a, double &b)
{
	const QStringList p = s.split(',');
	a = p.size() > 0 ? p[0].trimmed().toDouble() : 0;
	b = p.size() > 1 ? p[1].trimmed().toDouble() : 0;
}

static QColor parseColor(const QString &s)
{
	QString h = s.trimmed();
	if (h.startsWith('#'))
		h = h.mid(1);
	bool ok = false;
	const uint v = h.toUInt(&ok, 16);
	if (!ok)
		return QColor(0, 0, 0, 0);
	if (h.size() == 8)
		return QColor((v >> 16) & 255, (v >> 8) & 255, v & 255, (v >> 24) & 255);
	return QColor((v >> 16) & 255, (v >> 8) & 255, v & 255, 255);
}

static Brush parseBrush(const XNode *holder)
{
	Brush b;
	if (!holder)
		return b;
	const XNode *br = holder->child("Brush");
	if (!br)
		return b;
	b.color = parseColor(br->attr("Color", "#00000000"));
	if (br->attr("Type") == "Bitmap") {
		const XNode *bm = br->path("Brush.Bitmap/Bitmap");
		if (bm && !bm->attr("Source").isEmpty()) {
			b.bitmap = true;
			b.source = bm->attr("Source");
		}
	}
	return b;
}

static void parseCrop(const XNode &el, const QString &tag, Element &e)
{
	const XNode *c = el.path(tag + ".Crop/Crop");
	if (!c)
		return;
	const QStringList r = c->attr("Range").split(',');
	if (r.size() == 4) {
		e.hasCrop = true;
		e.cropL = r[0].toDouble();
		e.cropT = r[1].toDouble();
		e.cropR = r[2].toDouble();
		e.cropB = r[3].toDouble();
	}
	const QStringList f = c->attr("Feather").split(',');
	if (f.size() == 4) {
		e.featherL = f[0].toDouble();
		e.featherT = f[1].toDouble();
		e.featherR = f[2].toDouble();
		e.featherB = f[3].toDouble();
		if (e.featherL || e.featherT || e.featherR || e.featherB)
			e.hasCrop = true;
	}
}

bool Title::load(const std::string &path, std::string &err)
{
	mz_zip_archive zip;
	memset(&zip, 0, sizeof(zip));
	/* caminho em UTF-8 (miniz usa fopen; no Windows abre pelo nome curto) */
	QFile qf(QString::fromUtf8(path.c_str()));
	if (!qf.open(QIODevice::ReadOnly)) {
		err = "não foi possível abrir " + path;
		return false;
	}
	const QByteArray raw = qf.readAll();
	if (!mz_zip_reader_init_mem(&zip, raw.constData(), (size_t)raw.size(), 0)) {
		err = "arquivo .gtzip inválido";
		return false;
	}
	const mz_uint n = mz_zip_reader_get_num_files(&zip);
	for (mz_uint i = 0; i < n; i++) {
		mz_zip_archive_file_stat st;
		if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory)
			continue;
		size_t sz = 0;
		void *p = mz_zip_reader_extract_to_heap(&zip, i, &sz, 0);
		if (!p)
			continue;
		files[QString::fromUtf8(st.m_filename)] = QByteArray((const char *)p, (int)sz);
		mz_free(p);
	}
	mz_zip_reader_end(&zip);

	auto doc = files.find("document.xml");
	if (doc == files.end()) {
		err = "document.xml não encontrado no .gtzip";
		return false;
	}
	XNode root;
	if (!parseXml(doc->second, root)) {
		err = "document.xml ilegível";
		return false;
	}

	auto res = files.find("resources.xml");
	if (res != files.end()) {
		XNode rr;
		if (parseXml(res->second, rr))
			for (auto &resource : rr.kids)
				for (auto &src : resource.kids)
					resourceMap[src.text.trimmed()] = src.attr("guid");
	}

	width = root.attr("Width", "1920").toInt();
	height = root.attr("Height", "1080").toInt();

	for (auto &ln : root.kids) {
		if (ln.tag == "Layer") {
			Layer L;
			L.name = ln.attr("Name");
			vec2(ln.attr("Location"), L.x, L.y);
			vec2(ln.attr("Dimensions"), L.w, L.h);
			L.visible = ln.attr("Visible", "True") != "False";
			const XNode *comp = ln.path("Layer.Composition/Composition");
			if (comp) {
				for (auto &el : comp->kids) {
					Element e;
					const QString t = el.tag;
					e.type = t == "TextBlock"   ? ElemType::Text
						 : t == "Image"     ? ElemType::Image
						 : t == "Rectangle" ? ElemType::Rectangle
						 : t == "Ellipse"   ? ElemType::Ellipse
								    : ElemType::Unknown;
					if (e.type == ElemType::Unknown)
						continue;
					e.name = el.attr("Name");
					vec2(el.attr("Location"), e.x, e.y);
					vec2(el.attr("Dimensions"), e.w, e.h);
					e.opacity = el.attr("Opacity", "1").toDouble();
					e.visible = el.attr("Visible", "True") != "False";
					e.strokeThickness = el.attr("StrokeThickness", "0").toDouble();
					e.fill = parseBrush(el.child(t + ".Fill"));
					e.stroke = parseBrush(el.child(t + ".Stroke"));
					if (e.type == ElemType::Image) {
						const XNode *bm = el.path("Image.Bitmap/Bitmap");
						if (bm) {
							e.fill.bitmap = true;
							e.fill.source = bm->attr("Source");
						}
					}
					if (e.type == ElemType::Text) {
						e.text = el.attr("Text");
						e.fontFamily = el.attr("FontFamily", "Arial");
						e.fontSize = el.attr("FontSize", "20").toDouble();
						e.fontWeight = el.attr("FontWeight", "Regular");
						e.fontStyle = el.attr("FontStyle");
						e.textAlign = el.attr("TextAlign", "Left");
						e.verticalAlign = el.attr("VerticalAlign", "Top");
						e.lineSpacing = el.attr("LineSpacing", "1").toDouble();
						if (e.lineSpacing <= 0)
							e.lineSpacing = 1;
					}
					parseCrop(el, t, e);
					L.elements.push_back(e);
				}
			}
			layers.push_back(L);
		} else if (ln.tag == "Storyboard") {
			Storyboard sb;
			sb.type = ln.attr("Type", "TransitionIn");
			const XNode *an = ln.child("Storyboard.Animations");
			if (an) {
				for (auto &a : an->kids) {
					Animation A;
					A.kind = a.tag;
					A.object = a.attr("Object");
					A.delay = a.attr("Delay", "0").toDouble();
					A.duration = a.attr("Duration", "0.5").toDouble();
					A.interpolation = a.attr("Interpolation", "Linear");
					A.direction = a.attr("Direction");
					sb.anims.push_back(A);
				}
			}
			storyboards.push_back(sb);
		}
	}
	return true;
}

double Storyboard::length() const
{
	double m = 0;
	for (auto &a : anims)
		m = std::max(m, a.delay + a.duration);
	return m;
}

const Storyboard *Title::storyboard(const QString &type) const
{
	for (auto &s : storyboards)
		if (s.type == type)
			return &s;
	return nullptr;
}

/* ---------------- campos ---------------- */

std::vector<Field> Title::fields() const
{
	std::vector<Field> out;
	for (int li = 0; li < (int)layers.size(); li++) {
		for (int ei = 0; ei < (int)layers[li].elements.size(); ei++) {
			const Element &e = layers[li].elements[ei];
			Field f;
			f.layer = li;
			f.element = ei;
			if (e.type == ElemType::Text) {
				f.kind = Field::Text;
				f.key = e.name + ".Text";
				out.push_back(f);
				f.kind = Field::Color;
				f.key = e.name + ".Fill.Color";
				out.push_back(f);
			} else if (e.type == ElemType::Image) {
				f.kind = Field::Image;
				f.key = e.name + ".Source";
				out.push_back(f);
			} else if (e.fill.bitmap) {
				f.kind = Field::Image;
				f.key = e.name + ".Fill.Bitmap";
				out.push_back(f);
			} else {
				f.kind = Field::Color;
				f.key = e.name + ".Fill.Color";
				out.push_back(f);
			}
		}
	}
	for (auto &f : out)
		f.label = f.key;
	return out;
}

QString Title::originalValue(const QString &key) const
{
	for (auto &L : layers)
		for (auto &e : L.elements) {
			if (key == e.name + ".Text")
				return e.text;
			if (key == e.name + ".Fill.Color")
				return e.fill.color.name(QColor::HexArgb);
			if (key == e.name + ".Source" || key == e.name + ".Fill.Bitmap")
				return e.fill.source;
		}
	return QString();
}

/* ---------------- imagens ---------------- */

QImage Title::zipImage(const QString &source) const
{
	auto c = imgCache.find(source);
	if (c != imgCache.end())
		return c->second;
	QImage img;
	auto m = resourceMap.find(source);
	const QString inZip = m != resourceMap.end() ? m->second : source;
	auto f = files.find(inZip);
	if (f != files.end())
		img.loadFromData(f->second);
	if (img.isNull()) {
		/* caminho no disco (imagem escolhida no editor) */
		img.load(source);
	}
	if (!img.isNull())
		img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
	imgCache[source] = img;
	return img;
}

/* ---------------- desenho ---------------- */

static int fontWeight(const QString &w)
{
	const QString s = w.toLower();
	if (s == "thin")
		return 100;
	if (s == "extralight" || s == "ultralight")
		return 200;
	if (s == "light")
		return 300;
	if (s == "medium")
		return 500;
	if (s == "semibold" || s == "demibold")
		return 600;
	if (s == "bold")
		return 700;
	if (s == "extrabold" || s == "ultrabold")
		return 800;
	if (s == "black" || s == "heavy")
		return 900;
	return 400;
}

static void applyCrop(QImage &img, const Element &e)
{
	if (!e.hasCrop)
		return;
	const int W = img.width(), H = img.height();
	const double x0 = e.cropL * W, x1 = e.cropR * W, y0 = e.cropT * H, y1 = e.cropB * H;
	auto ramp = [](double d, double f) { return f <= 0 ? (d >= 0 ? 1.0 : 0.0) : std::clamp(d / f, 0.0, 1.0); };
	for (int y = 0; y < H; y++) {
		QRgb *row = reinterpret_cast<QRgb *>(img.scanLine(y));
		const double py = y + 0.5;
		const double fy = std::min(ramp(py - y0, e.featherT), ramp(y1 - py, e.featherB));
		for (int x = 0; x < W; x++) {
			const double px = x + 0.5;
			const double f = std::min(fy, std::min(ramp(px - x0, e.featherL), ramp(x1 - px, e.featherR)));
			if (f >= 1.0)
				continue;
			const QRgb c = row[x];
			row[x] = qRgba((int)(qRed(c) * f), (int)(qGreen(c) * f), (int)(qBlue(c) * f), (int)(qAlpha(c) * f));
		}
	}
}

QImage Title::renderElement(const Element &e, const std::map<QString, QString> &ov) const
{
	const int W = std::max(1, (int)std::ceil(e.w));
	const int H = std::max(1, (int)std::ceil(e.h));
	QImage img(W, H, QImage::Format_ARGB32_Premultiplied);
	img.fill(Qt::transparent);
	if (!e.visible)
		return img;

	auto get = [&](const QString &k) -> const QString * {
		auto it = ov.find(k);
		return it == ov.end() ? nullptr : &it->second;
	};

	QPainter p(&img);
	p.setRenderHint(QPainter::Antialiasing);
	p.setRenderHint(QPainter::SmoothPixmapTransform);
	p.setRenderHint(QPainter::TextAntialiasing);
	const QRectF r(0, 0, e.w, e.h);

	QColor fillColor = e.fill.color;
	if (const QString *c = get(e.name + ".Fill.Color"))
		fillColor = parseColor(*c);

	if (e.type == ElemType::Rectangle || e.type == ElemType::Ellipse) {
		QPainterPath shape;
		if (e.type == ElemType::Ellipse)
			shape.addEllipse(r);
		else
			shape.addRect(r);
		p.setClipPath(shape);
		QImage bmp;
		if (e.fill.bitmap) {
			const QString *o = get(e.name + ".Fill.Bitmap");
			bmp = zipImage(o && !o->isEmpty() ? *o : e.fill.source);
		}
		if (!bmp.isNull())
			p.drawImage(r, bmp); /* estica para o tamanho da forma, como no GT */
		else
			p.fillPath(shape, fillColor);
		p.setClipping(false);
		if (e.strokeThickness > 0) {
			QPen pen(e.stroke.color, e.strokeThickness);
			pen.setJoinStyle(Qt::MiterJoin);
			p.setPen(pen);
			p.setBrush(Qt::NoBrush);
			const double h2 = e.strokeThickness / 2;
			if (e.type == ElemType::Ellipse)
				p.drawEllipse(r.adjusted(h2, h2, -h2, -h2));
			else
				p.drawRect(r.adjusted(h2, h2, -h2, -h2));
		}
	} else if (e.type == ElemType::Image) {
		const QString *o = get(e.name + ".Source");
		const QImage src = zipImage(o && !o->isEmpty() ? *o : e.fill.source);
		if (!src.isNull()) {
			/* proporcional, centralizado */
			const double s = std::min(e.w / src.width(), e.h / src.height());
			const double sw = src.width() * s, sh = src.height() * s;
			p.drawImage(QRectF((e.w - sw) / 2, (e.h - sh) / 2, sw, sh), src);
		}
	} else if (e.type == ElemType::Text) {
		QString text = e.text;
		if (const QString *t = get(e.name + ".Text"))
			text = *t;
		QFont f(e.fontFamily);
		f.setPixelSize(std::max(1, (int)std::lround(e.fontSize)));
		f.setWeight(QFont::Weight(fontWeight(e.fontWeight)));
		f.setItalic(e.fontStyle.compare("Italic", Qt::CaseInsensitive) == 0);
		f.setHintingPreference(QFont::PreferNoHinting);
		const QFontMetricsF fm(f);

		/* \r\n, \n e \r sozinho são quebra de linha (o GT usa os três) */
		static const QRegularExpression nl("\r\n|\r|\n");
		while (text.endsWith('\r') || text.endsWith('\n'))
			text.chop(1);
		const QStringList lines = text.split(nl);
		const double lh = fm.height() * e.lineSpacing;
		const double total = lh * lines.size();
		double y = 0;
		if (e.verticalAlign == "Center")
			y = (e.h - total) / 2;
		else if (e.verticalAlign == "Bottom")
			y = e.h - total;

		QPainterPath path;
		for (const QString &ln : lines) {
			const double tw = fm.horizontalAdvance(ln);
			double x = 0;
			if (e.textAlign == "Center")
				x = (e.w - tw) / 2;
			else if (e.textAlign == "Right")
				x = e.w - tw;
			path.addText(QPointF(x, y + fm.ascent()), f, ln);
			y += lh;
		}
		if (e.strokeThickness > 0) {
			QPen pen(e.stroke.color, e.strokeThickness * 2);
			pen.setJoinStyle(Qt::RoundJoin);
			p.strokePath(path, pen);
		}
		if (e.fill.bitmap) {
			const QImage bmp = zipImage(e.fill.source);
			if (!bmp.isNull()) {
				QBrush b(bmp.scaled(W, H));
				p.fillPath(path, b);
			} else {
				p.fillPath(path, fillColor);
			}
		} else {
			p.fillPath(path, fillColor);
		}
	}
	p.end();
	applyCrop(img, e);
	return img;
}

QImage Title::renderStatic(const std::map<QString, QString> &ov) const
{
	QImage canvas(width, height, QImage::Format_ARGB32_Premultiplied);
	canvas.fill(Qt::transparent);
	QPainter p(&canvas);
	p.setRenderHint(QPainter::SmoothPixmapTransform);
	for (auto &L : layers) {
		if (!L.visible)
			continue;
		p.save();
		p.setClipRect(QRectF(L.x, L.y, L.w, L.h)); /* a camada recorta o que sai dela */
		for (auto &e : L.elements) {
			if (!e.visible)
				continue;
			p.setOpacity(e.opacity);
			p.drawImage(QPointF(L.x + e.x, L.y + e.y), renderElement(e, ov));
		}
		p.restore();
	}
	return canvas;
}

/* ---------------- curvas ---------------- */

static double bounceOut(double t)
{
	const double n1 = 7.5625, d1 = 2.75;
	if (t < 1 / d1)
		return n1 * t * t;
	if (t < 2 / d1) {
		t -= 1.5 / d1;
		return n1 * t * t + 0.75;
	}
	if (t < 2.5 / d1) {
		t -= 2.25 / d1;
		return n1 * t * t + 0.9375;
	}
	t -= 2.625 / d1;
	return n1 * t * t + 0.984375;
}

static double easeIn(const QString &f, double t)
{
	const double PI = 3.14159265358979323846;
	if (f == "Quadratic")
		return t * t;
	if (f == "Cubic")
		return t * t * t;
	if (f == "Quartic")
		return t * t * t * t;
	if (f == "Quintic")
		return t * t * t * t * t;
	if (f == "Sine")
		return 1 - std::cos(t * PI / 2);
	if (f == "Exponential")
		return t <= 0 ? 0 : std::pow(2, 10 * (t - 1));
	if (f == "Circular")
		return 1 - std::sqrt(std::max(0.0, 1 - t * t));
	if (f == "Back")
		return t * t * (2.70158 * t - 1.70158);
	if (f == "Elastic")
		return t <= 0 || t >= 1 ? t : -std::pow(2, 10 * t - 10) * std::sin((t * 10 - 10.75) * (2 * PI / 3));
	if (f == "Bounce")
		return 1 - bounceOut(1 - t);
	return t;
}

double ease(const QString &interp, double t)
{
	t = std::clamp(t, 0.0, 1.0);
	QString s = interp;
	s.remove("Easing");
	QString family = s, mode = "InOut";
	for (const char *m : {"InOut", "Out", "In"}) {
		if (s.endsWith(m)) {
			family = s.left(s.size() - (int)strlen(m));
			mode = m;
			break;
		}
	}
	if (family.isEmpty() || family == "Linear" || family == "None")
		return t;
	if (mode == "In")
		return easeIn(family, t);
	if (mode == "Out")
		return 1 - easeIn(family, 1 - t);
	return t < 0.5 ? easeIn(family, t * 2) / 2 : 1 - easeIn(family, (1 - t) * 2) / 2;
}

} // namespace gt
