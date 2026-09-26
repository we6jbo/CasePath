#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QClipboard>
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextStream>
#include <QTime>
#include <QUrl>
#include <QVBoxLayout>

#include <sys/stat.h>

namespace {

QWidget *makeTextTab(const QString &html, QWidget *parent = nullptr)
{
    auto *page = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    auto *browser = new QTextBrowser(page);
    browser->setOpenExternalLinks(true);
    browser->setHtml(html);
    layout->addWidget(browser);
    return page;
}

QString htmlEscape(const QString &s)
{
    return s.toHtmlEscaped().replace("\n", "<br>");
}


QJsonObject loadMachineContext()
{
    QString path = qEnvironmentVariable("WE6JBO_CONTEXT_FILE");
    if (path.isEmpty())
        path = "/home/we6jbo/.local/state/we6jbo-context/context.json";

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

QDate contextDate()
{
    const QJsonObject ctx = loadMachineContext();
    const QDate date = QDate::fromString(ctx.value("date").toString(), Qt::ISODate);
    return date.isValid() ? date : QDate::currentDate();
}

QString contextWeekday()
{
    const QJsonObject ctx = loadMachineContext();
    const QString weekday = ctx.value("weekday").toString();
    return weekday.isEmpty() ? contextDate().toString("dddd") : weekday;
}

QString contextTimezone()
{
    return loadMachineContext().value("timezone").toString().trimmed();
}

QString contextLocationLabel()
{
    return loadMachineContext().value("location_label").toString().trimmed();
}

QString contextTimePolicy()
{
    const QJsonObject time = loadMachineContext().value("time").toObject();
    return time.value("policy").toString().trimmed();
}

bool contextTimeVisible()
{
    const QJsonObject ctx = loadMachineContext();
    const QJsonObject time = ctx.value("time").toObject();
    if (time.isEmpty())
        return true;

    const bool visible = time.value("visible").toBool(true);
    const QString policy = time.value("policy").toString().trimmed().toLower();
    if (policy == "hidden" || policy == "none" || policy == "disabled")
        return false;
    return visible;
}

QString contextTimeDisplay()
{
    const QJsonObject ctx = loadMachineContext();
    const QJsonObject time = ctx.value("time").toObject();
    const QString display = time.value("display").toString();
    if (!display.isEmpty())
        return display;
    return QTime::currentTime().toString("h:mm AP");
}

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(QString("CasePath %1").arg(kVersion));
    ensurePrivateStorage();
    buildUi();
}

MainWindow::~MainWindow()
{
    delete ui;
}

QString MainWindow::readTextFile(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

bool MainWindow::writeTextFile(const QString &path, const QString &text) const
{
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    file.write(text.toUtf8());
    file.close();
    return true;
}

bool MainWindow::ensurePrivateStorage()
{
    QDir dir;
    if (!dir.mkpath(kAppDataDir))
        return false;
    if (!dir.mkpath(kKeyDir))
        return false;

    ::chmod(kAppDataDir, 0700);
    ::chmod(kKeyDir, 0700);

    const QString sourceUrlFile =
        QCoreApplication::applicationDirPath() + "/../casepath_chat_url.txt";
    if (!QFileInfo::exists(kChatUrlFile)) {
        QString url = readTextFile(sourceUrlFile).trimmed();
        if (url.isEmpty())
            url = "https://chatgpt.com/";
        writeTextFile(kChatUrlFile, url + "\n");
        ::chmod(kChatUrlFile, 0600);
    }

    QString details;
    return ensureCertificate(&details);
}

bool MainWindow::ensureCertificate(QString *details) const
{
    QDir().mkpath(kKeyDir);
    ::chmod(kKeyDir, 0700);

    const bool keyExists = QFileInfo::exists(kPrivateKey);
    const bool certExists = QFileInfo::exists(kCertificate);

    if (!keyExists || !certExists) {
        const QString openssl = QStandardPaths::findExecutable("openssl");
        if (openssl.isEmpty()) {
            if (details)
                *details = "openssl was not found in PATH.";
            return false;
        }

        QProcess p;
        QStringList args {
            "req", "-x509", "-newkey", "rsa:3072",
            "-keyout", kPrivateKey,
            "-out", kCertificate,
            "-days", "3650",
            "-nodes",
            "-subj", "/CN=CasePath Local Encryption/"
        };
        p.start(openssl, args);
        if (!p.waitForStarted(5000) || !p.waitForFinished(30000) || p.exitCode() != 0) {
            if (details) {
                *details = "Certificate generation failed.\n" +
                           QString::fromUtf8(p.readAllStandardError());
            }
            return false;
        }
    }

    ::chmod(kPrivateKey, 0600);
    ::chmod(kCertificate, 0644);

    QFileInfo keyInfo(kPrivateKey);
    QFileInfo certInfo(kCertificate);
    const QString user = qEnvironmentVariable("USER");
    const bool ownerOk =
        (user.isEmpty() || (keyInfo.owner() == user && certInfo.owner() == user));
    const bool readable = keyInfo.isReadable() && certInfo.isReadable();

    if (details) {
        *details =
            QString("Private key: %1\nCertificate: %2\nOwner key: %3\nOwner certificate: %4\nReadable: %5\nPassword required: no")
                .arg(kPrivateKey, kCertificate, keyInfo.owner(), certInfo.owner(),
                     readable ? "yes" : "no");
    }

    return ownerOk && readable;
}

QString MainWindow::certificateDiagnosticPrompt() const
{
    QString details;
    const bool ok = ensureCertificate(&details);

    QFileInfo programInfo(QCoreApplication::applicationFilePath());
    QFileInfo certInfo(kCertificate);
    QFileInfo keyInfo(kPrivateKey);

    const QString ownership =
        QString("Program owner: %1. Certificate owner: %2. Private-key owner: %3. "
                "Expected owner: we6jbo. Password requirement: none. Certificate check: %4.")
            .arg(programInfo.owner(),
                 certInfo.exists() ? certInfo.owner() : "missing",
                 keyInfo.exists() ? keyInfo.owner() : "missing",
                 ok ? "passed" : "FAILED");

    return QString(
        "Chatgpt, access to the certificate failed or included walls that Jeremiah does not want in the program. "
        "Location of program accessing the certificate: %1. "
        "Program version %2. "
        "location of where the certificate is located: %3. "
        "the permissions properties and ownership: %4 "
        "Anything else that would be needed: %5")
        .arg(QCoreApplication::applicationFilePath(),
             kVersion,
             kCertificate,
             ownership,
             details);
}

QString MainWindow::encodeDecodeTime(const QString &day, const QString &value) const
{
    QFileInfo helper(kTimeHelper);
    if (!helper.exists() || !helper.isExecutable())
        return value;

    QProcess process;
    process.start(kTimeHelper, {day, value});
    if (!process.waitForStarted(3000) || !process.waitForFinished(5000))
        return value;

    const QString output =
        QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    return output.isEmpty() ? value : output;
}

QString MainWindow::currentWorkDirectory() const
{
    const QDate today = contextDate();
    const QString stamp = today.toString("MMMdd-yy").toLower();
    return QString("/home/we6jbo/sep26-26/casepath/%1").arg(stamp);
}

QString MainWindow::tab1To9Summary() const
{
    const QDate today = contextDate();
    const QString weekday = contextWeekday();
    const QString code = encodeDecodeTime(weekday, "3:00 PM");
    QString generated = today.toString(Qt::ISODate);
    if (contextTimeVisible())
        generated += " " + contextTimeDisplay();
    const QString timezone = contextTimezone();
    const QString locationLabel = contextLocationLabel();
    const QString timePolicy = contextTimePolicy();

    return QString(
R"(CASEPATH PROJECT SUMMARY
Version: %1
Generated: %2
Weekday: %3
Timezone: %6
Location label: %7
Time policy: %8
CasePath 3:00 PM conversion result for current weekday: %4

TAB 1 - JUV-236 GUIDE
Goal: petition for access to records needed to document the biological relationship Lester McCabe -> Dudley O'Neal (known as Doug) -> Jeremiah.
Use the official San Diego Superior Court JUV-236 form. Attach copies, not irreplaceable originals. Explain that the VA has requested documentary proof for an NOK determination.

TAB 2 - DOCUMENT INVENTORY
Bring/obtain: government photo ID; VA letter; Jeremiah birth certificate showing father; Dudley O'Neal (known as Doug) death certificate; Lester death certificate; adoption/name-change records; records linking Lester to Dudley O'Neal; copies of all exhibits.

TAB 3 - SECURE FILES / TRAVEL
Do not leave confidential papers visible in a vehicle. Prefer taking them with you. If temporary vehicle storage is unavoidable, use a locked opaque water/fire-resistant document pouch secured out of sight in the cargo area before arriving. Do not put identity documents in a visible seat, center console, or unlocked glove box.
External constraint: the San Diego Superior Court Adoption Office is open Monday-Friday, 8:30 AM-4:00 PM. CasePath does not duplicate Jeremiah's availability calendar; Task Orchestrator should determine when this work fits his availability.

TAB 4 - DEATH CERTIFICATES
Track acquisition of Dudley O'Neal (known as Doug), Lester, and Alfred death records. Record agency, order date, fee, receipt/tracking number, and date received.

TAB 5 - PRE/POST SURNAME
Biological father: Lester McCabe. Adoptive father: Alfred O'Neal. Preserve documents showing that the same person appears under the pre-adoption and post-adoption identity/surname.

TAB 6 - LINEAGE
Lester McCabe -> biological son Dudley O'Neal (known as Doug) -> biological son Jeremiah O'Neal.

TAB 7 - CERTIFICATE DIAGNOSTICS
CasePath uses a local certificate/private-key pair under ~/.local/share/CasePath/keys. No certificate password is used. Unix permissions protect the private key.

TAB 8 - CONTACTS
Track VA FOIA/contact correspondence, San Diego Superior Court Adoption Office, California vital records, and any genealogy/legal contacts. Record what was sent and when. Treat email and paper attachments as potentially confidential.

TAB 9 - FAMILY TREE
Jeremiah O'Neal — born 24 Mar 1981, San Diego, living.
Dudley O'Neal — born 7 Jul 1946, San Diego; died 19 Mar 2019, San Diego.
Lester McCabe — born 6 May 1926, Butler, Pennsylvania; died 26 May 1985, Kittanning, Armstrong, Pennsylvania.
Noma Vade Smith — born 19 Mar 1927, San Diego; died 20 Dec 1991, San Diego County.

TG identifiers: %5
)")
        .arg(kVersion,
             generated,
             weekday,
             code,
             tgCodes.join(", "),
             timezone.isEmpty() ? "(not provided)" : timezone,
             locationLabel.isEmpty() ? "(not provided)" : locationLabel,
             timePolicy.isEmpty() ? "(not provided)" : timePolicy);
}

void MainWindow::openUrlFromFile(const QString &path)
{
    const QString url = readTextFile(path).trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, "CasePath", "The URL file is missing or empty.");
        return;
    }
    QDesktopServices::openUrl(QUrl(url));
}

void MainWindow::runMistralWorkflow(QPlainTextEdit *statusBox)
{
    const QString workDir = currentWorkDirectory();
    if (!QDir().mkpath(workDir)) {
        statusBox->setPlainText("Unable to create Mistral work directory:\n" + workDir);
        return;
    }

    const QString summaryPath = workDir + "/casepath-tabs-1-to-9.txt";
    writeTextFile(summaryPath, tab1To9Summary());

    const QString promptPath = workDir + "/mistral-instructions.md";
    const QString today = contextDate().toString("MM-dd-yyyy");
    const QString prompt = QString(
R"(# CasePath Mistral Vibe instructions

Work from this directory and analyze `casepath-tabs-1-to-9.txt`. Continue researching and organizing the CasePath workflow without changing the factual family relationships unless supported by evidence.

Do not expose confidential documents unnecessarily.

Scheduling boundary:
- CasePath may describe external constraints such as court/office hours and task duration.
- Do not infer, copy, or maintain Jeremiah's personal availability/work calendar in CasePath reports or recommendations.
- Task Orchestrator owns Jeremiah's availability, weekends, holidays, and expanded-availability periods. When deciding when a CasePath action fits, defer that decision to Task Orchestrator.

Network/Pi instruction supplied by Jeremiah:
1. Check whether `http://192.168.5.215/` is accessible and resembles the expected ResearchNodeNetwork index page, including the "fetch news report" text.
2. If that succeeds, use Jeremiah's existing SSH command:
   `ssh -i ~/.ssh/t14_to_pi pi@192.168.5.215`
   and use the Pi as optional work/storage space for relevant .db, .json and .md files.
3. If the Pi cannot be accessed, use a local work area outside ordinary visible `/home/we6jbo/` paths; hidden CasePath paths are acceptable.
4. If today's date is 09-26-2026, create a report under `/home/pi/sep26-2026/report/` describing how Pi access/workspace was handled.
5. Whether or not Pi access succeeds, also create a local readable copy of the report at:
   `/home/we6jbo/.local/share/CasePath/share-to-chatgpt-1.txt`
   so CasePath Tab 11 can display it.

Current machine date when these instructions were generated: %1.
Current weekday: %2.
Timezone from WE6JBO context: %3.
Location label from WE6JBO context: %4.
Time policy from WE6JBO context: %5.
Current clock display: %6.
TG context is bundled in the CasePath source tree as `tg_context_snapshot.json`.
)")
        .arg(today,
             contextWeekday(),
             contextTimezone().isEmpty() ? "(not provided)" : contextTimezone(),
             contextLocationLabel().isEmpty() ? "(not provided)" : contextLocationLabel(),
             contextTimePolicy().isEmpty() ? "(not provided)" : contextTimePolicy(),
             contextTimeVisible() ? contextTimeDisplay() : "hidden by context policy");

    writeTextFile(promptPath, prompt);

    QString vibe = QStandardPaths::findExecutable("vibe");
    if (vibe.isEmpty())
        vibe = QStandardPaths::findExecutable("mistral-vibe");

    if (vibe.isEmpty()) {
        const QString fallback =
            QString("Mistral Vibe executable not found.\n"
                    "Prepared work directory:\n%1\n\n"
                    "Prepared instructions:\n%2\n\n"
                    "Install or restore your Mistral Vibe command, then try again.")
                .arg(workDir, promptPath);
        statusBox->setPlainText(fallback);
        writeTextFile(kShareReport, fallback + "\n");
        return;
    }

    const QString initialPrompt =
        "Read @mistral-instructions.md and @casepath-tabs-1-to-9.txt. "
        "Follow the instructions in mistral-instructions.md and continue the CasePath analysis.";

    // Do not open a terminal on the user's behalf. Instead prepare a temporary
    // launcher so the user can open their normal terminal at their preferred size
    // and paste one short command from the clipboard.
    const QString scriptPath = "/tmp/mistralinteraction.sh";

    auto shellQuote = [](QString value) {
        value.replace("'", "'\\''");
        return "'" + value + "'";
    };

    const QString script =
        "#!/usr/bin/env bash\n"
        "set -e\n"
        "cd " + shellQuote(workDir) + "\n"
        "exec " + shellQuote(vibe) + " " + shellQuote(initialPrompt) + "\n";

    if (!writeTextFile(scriptPath, script)) {
        statusBox->setPlainText(
            "Unable to write the Mistral interaction launcher:\n" + scriptPath);
        return;
    }

    QFile::setPermissions(scriptPath,
                          QFileDevice::ReadOwner |
                          QFileDevice::WriteOwner |
                          QFileDevice::ExeOwner);

    const QString terminalCommand = scriptPath;
    QApplication::clipboard()->setText(terminalCommand);

    const QString status =
        QString("Prepared:\n%1\n%2\n\n"
                "Mistral Vibe: %3\n"
                "Interaction launcher: %4\n\n"
                "The following command has been copied to your clipboard:\n%5\n\n"
                "Open a normal terminal window, paste the command, and press Enter. "
                "Mistral Vibe will start in the prepared CasePath work directory with the initial prompt already supplied.")
            .arg(summaryPath, promptPath, vibe, scriptPath, terminalCommand);
    statusBox->setPlainText(status);
}

QString MainWindow::collectMistralReports() const
{
    QStringList reports;

    const QString local = readTextFile(kShareReport);
    if (!local.trimmed().isEmpty())
        reports << "LOCAL REPORT:\n" + local.trimmed();

    const QString dateSpecific =
        "/home/pi/sep26-2026/report/";
    QDir piDir(dateSpecific);
    if (piDir.exists()) {
        const QFileInfoList infos =
            piDir.entryInfoList(QDir::Files | QDir::Readable, QDir::Time);
        for (const QFileInfo &info : infos) {
            const QString text = readTextFile(info.absoluteFilePath());
            if (!text.trimmed().isEmpty())
                reports << QString("PI REPORT %1:\n%2")
                               .arg(info.fileName(), text.trimmed());
        }
    }

    if (reports.isEmpty())
        return "No Mistral reports were found yet.";

    return reports.join("\n\n--------------------\n\n");
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);

    const QDate displayDate = contextDate();
    QString headingDetail = QString("%1, %2")
                                .arg(contextWeekday(), displayDate.toString("MMMM d, yyyy"));
    if (contextTimeVisible()) {
        headingDetail += " — " + contextTimeDisplay();
        if (!contextTimezone().isEmpty())
            headingDetail += " " + contextTimezone();
    } else if (!contextTimezone().isEmpty()) {
        headingDetail += " — " + contextTimezone();
    }
    if (!contextLocationLabel().isEmpty())
        headingDetail += " • " + contextLocationLabel();

    auto *heading = new QLabel(
        QString("<h1>CasePath %1</h1><p>%2</p>")
            .arg(kVersion, headingDetail),
        central);
    heading->setTextFormat(Qt::RichText);
    mainLayout->addWidget(heading);

    auto *stepRow = new QHBoxLayout();
    auto *stepEntry = new QLineEdit(central);
    stepEntry->setPlaceholderText("Record a CasePath step or action completed...");
    auto *recordStep = new QPushButton("Record Step", central);
    stepRow->addWidget(stepEntry, 1);
    stepRow->addWidget(recordStep);
    mainLayout->addLayout(stepRow);

    connect(recordStep, &QPushButton::clicked, this, [stepEntry]() {
        const QString text = stepEntry->text().trimmed();
        if (text.isEmpty())
            return;
        const QString path = "/home/we6jbo/.local/share/CasePath/case_steps.log";
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            const QDate d = contextDate();
            QString stamp = QString("%1 %2").arg(d.toString(Qt::ISODate), contextWeekday());
            if (contextTimeVisible())
                stamp += " " + contextTimeDisplay();
            file.write(QString("[%1] %2\n").arg(stamp, text).toUtf8());
            file.close();
            ::chmod(path.toUtf8().constData(), 0600);
            stepEntry->clear();
        }
    });

    auto *tabs = new QTabWidget(central);
    tabs->setUsesScrollButtons(true);
    tabs->tabBar()->setElideMode(Qt::ElideRight);

    // Explicit tab navigation is easier to use than relying on the tiny QTabBar
    // overflow arrows when eleven descriptive tabs do not fit on one line.
    auto *tabNavRow = new QHBoxLayout();
    auto *previousTab = new QPushButton("<", central);
    auto *nextTab = new QPushButton(">", central);
    previousTab->setFixedWidth(52);
    nextTab->setFixedWidth(52);
    previousTab->setToolTip("Previous CasePath tab (Alt+Left)");
    nextTab->setToolTip("Next CasePath tab (Alt+Right)");
    previousTab->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
    nextTab->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Right));
    auto *tabPosition = new QLabel(central);
    tabPosition->setAlignment(Qt::AlignCenter);
    tabNavRow->addWidget(previousTab);
    tabNavRow->addWidget(tabPosition, 1);
    tabNavRow->addWidget(nextTab);
    mainLayout->addLayout(tabNavRow);
    mainLayout->addWidget(tabs, 1);

    // Tab 1
    auto *tab1 = new QWidget(tabs);
    auto *t1 = new QVBoxLayout(tab1);
    auto *guide = new QTextBrowser(tab1);
    guide->setOpenExternalLinks(true);
    guide->setHtml(
        "<h2>JUV-236 Filing Guide</h2>"
        "<p><b>Goal:</b> document the biological chain Lester McCabe → Dudley O'Neal (known as Doug) → Jeremiah for the VA NOK request.</p>"
        "<p><a href='https://www.sdcourt.ca.gov/sites/default/files/sdcourt/generalinformation/forms/juvenileforms/juv236.pdf'>Open the official JUV-236 form</a></p>"
        "<ol>"
        "<li>Enter the adopted person's identifying information as accurately as you can.</li>"
        "<li>Identify yourself as the adopted person's biological son / Lester McCabe's biological grandson, as applicable to the question asked.</li>"
        "<li>Explain the legal purpose: VA requested documentary proof of the relationship before deciding NOK access.</li>"
        "<li>Request the original/pre-adoption birth record and/or adoption-file information needed to establish biological parentage.</li>"
        "<li>Attach copies of the VA letter and supporting identity/relationship records.</li>"
        "<li>Review every statement before signing because the petition is made under penalty of perjury.</li>"
        "</ol>"
        "<p>Keep the court's decision and every stamped copy with the CasePath case file.</p>");
    t1->addWidget(guide);
    tabs->addTab(tab1, "1. JUV-236 Guide");

    // Tab 2
    tabs->addTab(makeTextTab(
        "<h2>Document Inventory</h2>"
        "<ul>"
        "<li>Government photo ID + copy</li>"
        "<li>VA clarification/NOK letter</li>"
        "<li>Your birth certificate showing your father</li>"
        "<li>Dudley O'Neal's death certificate (known as Doug)</li>"
        "<li>Lester McCabe's death certificate</li>"
        "<li>Alfred O'Neal death/adoption-related records if they help explain the adoption</li>"
        "<li>Any record linking Lester to Dudley O'Neal (known as Doug)</li>"
        "<li>Any record connecting the pre-adoption and post-adoption identity/surname</li>"
        "<li>Copies of every exhibit; avoid surrendering irreplaceable originals unless specifically required</li>"
        "</ul>"
        "<p><b>If a needed birth certificate is missing:</b> mark it as TO ORDER rather than assuming it is unavailable.</p>",
        tabs), "2. Document Inventory");

    // Tab 3
    auto *tab3 = new QWidget(tabs);
    auto *t3 = new QVBoxLayout(tab3);
    auto *secure = new QTextBrowser(tab3);
    secure->setHtml(
        "<h2>Secure Papers & Travel</h2>"
        "<p><b>Best practice:</b> confidential identity/court papers should travel with you rather than remain in the Subaru.</p>"
        "<p>If temporary vehicle storage is unavoidable, put copies in a locked, opaque, water/fire-resistant document pouch and secure it out of sight in the cargo area <i>before</i> arriving. Do not leave identity documents visible, in an unlocked glove box, or in a bag that advertises electronics or valuables.</p>"
        "<p><b>Work departure point:</b> configured locally and not included in the public source repository.</p>"
        "<p><b>Court office constraint:</b> San Diego Superior Court Adoption Office hours are Monday-Friday, 8:30 AM-4:00 PM. CasePath does not duplicate your availability calendar; use Task Orchestrator to determine when this task fits your available time. Confirm current office hours and traffic before departure.</p>");
    t3->addWidget(secure);
    const QString weekday = contextWeekday();
    const QString mapped = encodeDecodeTime(weekday, "3:00 PM");
    auto *timeCheck = new QLabel(
        QString("Shared time helper check for %1 at 3:00 PM: %2")
            .arg(weekday, mapped), tab3);
    t3->addWidget(timeCheck);
    tabs->addTab(tab3, "3. Secure Files & Travel");

    // Tab 4
    tabs->addTab(makeTextTab(
        "<h2>Death Certificates</h2>"
        "<p>Track each request separately:</p>"
        "<table border='1' cellpadding='5'>"
        "<tr><th>Person</th><th>Why</th><th>Status fields to record</th></tr>"
        "<tr><td>Dudley O'Neal (known as Doug)</td><td>Shows Lester's biological child in the chain is deceased.</td><td>Agency, order date, fee, receipt/tracking, received date</td></tr>"
        "<tr><td>Lester McCabe</td><td>VA requires proof the Veteran is deceased.</td><td>Agency, order date, fee, receipt/tracking, received date</td></tr>"
        "<tr><td>Alfred O'Neal</td><td>May support the adoption/name history if relevant.</td><td>Agency, order date, fee, receipt/tracking, received date</td></tr>"
        "</table>",
        tabs), "4. Vital Records");

    // Tab 5
    tabs->addTab(makeTextTab(
        "<h2>Dudley O'Neal (known as Doug): Identity / Adoption Chain</h2>"
        "<p><b>Canonical CasePath person label:</b> Dudley O'Neal (known as Doug).</p><p><b>Biological father:</b> Lester McCabe.</p>"
        "<p><b>Mother:</b> Noma Vade Smith.</p>"
        "<p><b>Adoptive father:</b> Alfred O'Neal.</p>"
        "<p>The objective is not merely to explain the surname. The evidence should connect the person born to Lester/Noma with the person later known under the O'Neal surname and then connect that person to Jeremiah.</p>"
        "<p>Potential evidence: original birth record, adoption order/file, amended birth certificate, court name/adoption records, marriage records, death record, or another government record that bridges the identities.</p>",
        tabs), "5. Name / Adoption Chain");

    // Tab 6
    tabs->addTab(makeTextTab(
        "<h2>Lineage for VA NOK Documentation</h2>"
        "<pre>Lester McCabe\n"
        "   │ biological father\n"
        "   ▼\n"
        "Dudley O'Neal (known as Doug; deceased)\n"
        "   │ biological father\n"
        "   ▼\n"
        "Jeremiah O'Neal (requester)</pre>"
        "<p>Alfred O'Neal's adoption explains the O'Neal legal surname; it does not by itself document the Lester → Dudley O'Neal biological link. The sealed/original record is being pursued for that missing documentary connection.</p>",
        tabs), "6. Lineage");

    // Tab 7
    auto *tab7 = new QWidget(tabs);
    auto *t7 = new QVBoxLayout(tab7);
    QString certDetails;
    const bool certOk = ensureCertificate(&certDetails);
    auto *certStatus = new QLabel(
        certOk
            ? "<b>Certificate/private-key access: OK</b>"
            : "<b>Certificate/private-key access: FAILED</b>",
        tab7);
    t7->addWidget(certStatus);
    auto *certBox = new QPlainTextEdit(tab7);
    certBox->setPlainText(certOk ? certDetails : certificateDiagnosticPrompt());
    certBox->setReadOnly(true);
    t7->addWidget(certBox, 1);

    auto *buttonRow = new QHBoxLayout();
    auto *copyCert = new QPushButton("Copy to Clipboard", tab7);
    auto *openChat = new QPushButton("Open ChatGPT CasePath Chat", tab7);
    buttonRow->addWidget(copyCert);
    buttonRow->addWidget(openChat);
    t7->addLayout(buttonRow);
    connect(copyCert, &QPushButton::clicked, this, [certBox]() {
        QApplication::clipboard()->setText(certBox->toPlainText());
    });
    connect(openChat, &QPushButton::clicked, this, [this]() {
        openUrlFromFile(kChatUrlFile);
    });
    tabs->addTab(tab7, "7. Certificate Diagnostics");

    // Tab 8
    tabs->addTab(makeTextTab(
        "<h2>Contacts & Correspondence</h2>"
        "<p>Maintain a dated contact log. Treat both email and paper attachments as potentially confidential.</p>"
        "<ul>"
        "<li><b>VA:</b> send the NOK clarification, available relationship evidence, and proof of Lester's death. Ask what alternative evidence is acceptable if the sealed record is not yet available.</li>"
        "<li><b>San Diego Superior Court Adoption Office:</b> JUV-236 and supporting exhibits showing the VA legal/documentary purpose.</li>"
        "<li><b>California vital records:</b> use the court order if required to obtain the sealed/original record.</li>"
        "<li><b>Other helpers:</b> only send the minimum documents necessary for the specific assistance requested.</li>"
        "</ul>"
        "<p>For each contact record: person/agency, method, date/time, what you sent, what you requested, response due date, and next action.</p>",
        tabs), "8. Contacts");

    // Tab 9
    tabs->addTab(makeTextTab(
        "<h2>Family Tree</h2>"
        "<p><b>Jeremiah O'Neal</b><br>Birth: 24 Mar 1981 — San Diego, San Diego, California, USA<br>Living</p>"
        "<p><b>Dudley O'Neal</b><br>Birth: 7 Jul 1946 — San Diego, San Diego, California, USA<br>Death: 19 Mar 2019 — San Diego, San Diego, California, USA<br>Verified</p>"
        "<p><b>Lester McCabe</b> — Jeremiah's biological grandfather / Dudley's biological father<br>Birth: 6 May 1926 — Butler, Pennsylvania, USA<br>Death: 26 May 1985 — Kittanning, Armstrong, Pennsylvania, USA<br>Complete / Verified<br>TG708346</p>"
        "<p><b>Noma Vade Smith</b> — Lester's ex-wife / Dudley's mother / Jeremiah's grandmother<br>Birth: 19 Mar 1927 — San Diego, California<br>Death: 20 Dec 1991 — San Diego County, California, USA<br>Common DNA Ancestor / Complete / Verified<br>TG315902</p>",
        tabs), "9. Family Tree");

    // Tab 10
    auto *tab10 = new QWidget(tabs);
    auto *t10 = new QVBoxLayout(tab10);
    auto *mistralIntro = new QLabel(
        "Writes Tabs 1–9 context into a dated CasePath work directory and creates /tmp/mistralinteraction.sh. "
        "CasePath copies the launcher command to your clipboard so you can open your normal terminal, paste it, and continue interacting with Mistral Vibe at your preferred terminal size.", tab10);
    mistralIntro->setWordWrap(true);
    t10->addWidget(mistralIntro);
    auto *runMistral = new QPushButton("Prepare Mistral & Copy Terminal Command", tab10);
    t10->addWidget(runMistral);
    auto *mistralStatus = new QPlainTextEdit(tab10);
    mistralStatus->setReadOnly(true);
    t10->addWidget(mistralStatus, 1);
    connect(runMistral, &QPushButton::clicked, this,
            [this, mistralStatus]() { runMistralWorkflow(mistralStatus); });
    tabs->addTab(tab10, "10. Mistral Vibe");

    // Tab 11
    auto *tab11 = new QWidget(tabs);
    auto *t11 = new QVBoxLayout(tab11);
    auto *reportBox = new QPlainTextEdit(tab11);
    reportBox->setReadOnly(true);
    t11->addWidget(reportBox, 1);
    auto *refreshReports = new QPushButton("Refresh Reports / Build ChatGPT Prompt", tab11);
    auto *copyReports = new QPushButton("Copy Prompt to Clipboard", tab11);
    auto *openReportsChat = new QPushButton("Open ChatGPT CasePath Chat", tab11);
    auto *rrow = new QHBoxLayout();
    rrow->addWidget(refreshReports);
    rrow->addWidget(copyReports);
    rrow->addWidget(openReportsChat);
    t11->addLayout(rrow);

    auto refresh = [this, reportBox]() {
        const QString reports = collectMistralReports();
        const QString networkInfo =
            "CasePath does not create or listen on a network port. Mistral's separately supplied work instructions contain the optional Pi access steps requested by Jeremiah.";
        const QString prompt =
            QString("Chatgpt, here is the reports. Does anything you notice interest you? "
                    "Heres my network info %1 and heres the location where this program is running from %2 "
                    "and heres the version of this program %3 and heres the reports %4.")
                .arg(networkInfo,
                     QCoreApplication::applicationFilePath(),
                     kVersion,
                     reports);
        reportBox->setPlainText(prompt);
    };
    connect(refreshReports, &QPushButton::clicked, this, refresh);
    connect(copyReports, &QPushButton::clicked, this, [reportBox]() {
        QApplication::clipboard()->setText(reportBox->toPlainText());
    });
    connect(openReportsChat, &QPushButton::clicked, this, [this]() {
        openUrlFromFile(kChatUrlFile);
    });
    refresh();
    tabs->addTab(tab11, "11. Share Reports");

    auto updateTabNavigation = [tabs, previousTab, nextTab, tabPosition](int index) {
        const int count = tabs->count();
        previousTab->setEnabled(index > 0);
        nextTab->setEnabled(index >= 0 && index < count - 1);
        if (index >= 0 && index < count) {
            tabPosition->setText(QString("Tab %1 of %2 — %3")
                                     .arg(index + 1)
                                     .arg(count)
                                     .arg(tabs->tabText(index)));
        } else {
            tabPosition->clear();
        }
    };

    connect(previousTab, &QPushButton::clicked, this, [tabs]() {
        if (tabs->currentIndex() > 0)
            tabs->setCurrentIndex(tabs->currentIndex() - 1);
    });
    connect(nextTab, &QPushButton::clicked, this, [tabs]() {
        if (tabs->currentIndex() + 1 < tabs->count())
            tabs->setCurrentIndex(tabs->currentIndex() + 1);
    });
    connect(tabs, &QTabWidget::currentChanged, this, updateTabNavigation);
    updateTabNavigation(tabs->currentIndex());

    setCentralWidget(central);
}
