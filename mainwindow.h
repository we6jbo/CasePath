#pragma once

#include <QMainWindow>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QPlainTextEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    static constexpr const char *kVersion = "0.2.5";
    static constexpr const char *kTimeHelper = "/opt/casepath_time/casepath_time.py";
    static constexpr const char *kAppDataDir = "/home/we6jbo/.local/share/CasePath";
    static constexpr const char *kKeyDir = "/home/we6jbo/.local/share/CasePath/keys";
    static constexpr const char *kPrivateKey = "/home/we6jbo/.local/share/CasePath/keys/casepath_private_key.pem";
    static constexpr const char *kCertificate = "/home/we6jbo/.local/share/CasePath/keys/casepath_certificate.pem";
    static constexpr const char *kChatUrlFile = "/home/we6jbo/.local/share/CasePath/casepath_chat_url.txt";
    static constexpr const char *kShareReport = "/home/we6jbo/.local/share/CasePath/share-to-chatgpt-1.txt";

    const QStringList tgCodes {
        "TG315902", "TG708346", "TG264819", "TG150984",
        "TG594126", "TG918273", "TG540918"
    };

    QString encodeDecodeTime(const QString &day, const QString &value) const;
    bool ensurePrivateStorage();
    bool ensureCertificate(QString *details = nullptr) const;
    QString certificateDiagnosticPrompt() const;
    QString readTextFile(const QString &path) const;
    bool writeTextFile(const QString &path, const QString &text) const;
    QString currentWorkDirectory() const;
    QString tab1To9Summary() const;
    void buildUi();
    void openUrlFromFile(const QString &path);
    void runMistralWorkflow(QPlainTextEdit *statusBox);
    QString collectMistralReports() const;
};
