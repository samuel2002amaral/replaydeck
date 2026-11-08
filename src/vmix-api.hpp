#pragma once
#include <QObject>
#include <QString>
#include <QMap>
#include <QByteArray>

class QTcpServer;
class QTcpSocket;
class QTimer;

/* API web compatível com a do vMix, para apps de controle (como o Painel vMix):
 *  - HTTP (8088): GET /api → XML de estado no formato do vMix;
 *                 GET /api/?Function=Nome&Input=...&Value=... → executa;
 *                 GET / → página de atalhos; /?shortcut=ID → executa o atalho.
 *  - TCP (8099): FUNCTION, XML, TALLY, SUBSCRIBE TALLY/ACTS.
 * No OBS, os "inputs" do vMix são: cenas, títulos GT, fontes e o Replay. */
class VmixApi : public QObject {
	Q_OBJECT
public:
	static void create(QObject *parent);
	static void destroy();
	static VmixApi *instance();

	/* relê a configuração (portas, ligado/desligado) e reinicia os servidores */
	void restart();
	/* texto para a tela de configurações */
	QString status() const { return statusText; }

	/* executa uma função do vMix; devolve "" se deu certo, ou a mensagem de erro */
	QString run(const QString &fn, const QMap<QString, QString> &params);
	QByteArray xml();

private:
	explicit VmixApi(QObject *parent);
	~VmixApi() override;

	void stop();
	void onHttpConnection();
	void onTcpConnection();
	void handleHttp(QTcpSocket *s);
	void handleTcpLine(QTcpSocket *s, const QByteArray &line);
	void tick();
	void broadcast(const QByteArray &line, const char *subscription);

	QTcpServer *http = nullptr;
	QTcpServer *tcp = nullptr;
	QTimer *timer = nullptr;
	QString statusText;
	QByteArray lastTally;
	QByteArray lastActs;
};
