#include "../src/gt/gt-source.hpp"
#include <obs-module.h>
#include <util/platform.h>
#include <QGuiApplication>
#include <cstdio>
#include <unistd.h>
#include "../src/gt/gt-doc.hpp"
OBS_DECLARE_MODULE()
int main(int argc, char **argv)
{
	QGuiApplication app(argc, argv);
	obs_startup("en-US", nullptr, nullptr);
	struct obs_video_info ovi = {};
	ovi.graphics_module = "libobs-opengl";
	ovi.fps_num = 30; ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = 1920; ovi.base_height = ovi.output_height = 1080;
	ovi.output_format = VIDEO_FORMAT_NV12;
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS) return 1;
	registerGtSource();
	obs_source_t *casa = gtLibraryAdd(argv[1]);
	auto t = gtTitleOf(casa);
	QString field;
	for (const GtField &f : t->fields()) if (f.kind == GtField::Text) { field = f.id; break; }
	auto v = gtValues(casa); v[field] = "FORCA JOVEM"; gtSetValues(casa, v);
	gtRename(casa, "ESCALAÇÃO CASA");
	obs_source_t *fora = gtLibraryDuplicate(casa, "ESCALAÇÃO FORA");
	auto vf = gtValues(fora); vf[field] = "INTOCAVEIS"; gtSetValues(fora, vf);
	printf("campo %s\n", field.toUtf8().constData());
	for (obs_source_t *s : gtLibraryList()) {
		printf("'%s' = %s\n", obs_source_get_name(s), gtValues(s).value(field).toUtf8().constData());
		obs_source_release(s);
	}
	printf("renomear para nome ocupado: %s\n", gtRename(fora, "ESCALAÇÃO CASA") ? "aceitou (ERRO)" : "recusou (ok)");
	obs_source_release(casa); obs_source_release(fora);
	fflush(stdout);
	_exit(0);
}
