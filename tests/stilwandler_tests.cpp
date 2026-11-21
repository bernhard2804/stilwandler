#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "styles.h"
#include "theme.h"
#include "translator.h"

class StilwandlerTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Keeps the test's settings away from the user's own.
        QStandardPaths::setTestModeEnabled(true);
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
    }

    void bundledStylesAreValid()
    {
        QFile file(":/stile.json");
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString error;
        QStringList skipped;
        const auto styles = Styles::parse(file.readAll(), &error, &skipped);
        QVERIFY2(skipped.isEmpty(), qPrintable(skipped.join(",")));
        QCOMPARE(styles.size(), 6);
        for (const Style &s : styles)
            QVERIFY2(!s.examples.isEmpty(), qPrintable(s.key));
    }

    void parseSkipsBadEntries()
    {
        QString error;
        QStringList skipped;
        const auto styles = Styles::parse(R"([
            {"key": "ok", "name": "Ok", "description": "d"},
            {"key": "Bad Key", "name": "X", "description": "d"},
            {"key": "ok", "name": "Duplicate", "description": "d"},
            {"key": "empty", "name": "", "description": "d"}
        ])", &error, &skipped);
        QCOMPARE(styles.size(), 1);
        QCOMPARE(skipped.size(), 3);
        QVERIFY(Styles::parse("{oops", &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void requestCarriesStyleAndText()
    {
        Style style{"guru", "Guru", "🧘", "", "Sprich weise.", {{"a", "b"}}};
        const QJsonObject body = QJsonDocument::fromJson(
            Styles::requestBody(style, "Hallo", "claude-opus-5-5", "low")).object();
        QCOMPARE(body.value("model").toString(), QString("claude-opus-5-5"));
        QCOMPARE(body.value("fallbacks").toString(), QString("default"));
        QVERIFY(body.value("stream").toBool());
        QVERIFY(body.value("system").toString().contains("Sprich weise."));
        QVERIFY(body.value("system").toString().contains("<original>a</original>"));
        QVERIFY(body.value("messages").toArray().at(0).toObject().value("content").toString().contains("Hallo"));
        const QJsonObject haiku = QJsonDocument::fromJson(
            Styles::requestBody(style, "x", "claude-haiku-5-5", "low")).object();
        QVERIFY(!haiku.contains("fallbacks"));
    }

    void recipientShapesThePrompt()
    {
        Style style{"wikinger", "Wikinger", "", "", "Saga.", {{"Hallo", "Heil dir, Held", "mann"}}};
        const QString toWoman = Styles::systemPrompt(style, "frau");
        QVERIFY(toWoman.contains("geht an eine Frau"));
        QVERIFY(toWoman.contains("<an>einen Mann</an>"));
        QVERIFY(Styles::systemPrompt(style, "mann").contains("geht an einen Mann"));
        QVERIFY(Styles::systemPrompt(style, "offen").contains("ohne Geschlecht"));
        const QJsonObject body = QJsonDocument::fromJson(
            Styles::requestBody(style, "x", "claude-opus-5-5", "low", "frau")).object();
        QVERIFY(body.value("system").toString().contains("geht an eine Frau"));
    }

    void recipientIsRememberedAndChecked()
    {
        {
            Backend backend({}, "test-key");
            backend.setRecipient("frau");
            QCOMPARE(backend.recipient(), QString("frau"));
            backend.setRecipient("quatsch");
            QCOMPARE(backend.recipient(), QString("offen"));
            backend.setRecipient("mann");
        }
        Backend again({}, "test-key");
        QCOMPARE(again.recipient(), QString("mann"));
    }

    void bundledStylesAddressBothSides()
    {
        QFile file(":/stile.json");
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString error;
        for (const Style &s : Styles::parse(file.readAll(), &error)) {
            if (s.key == "emoji")
                continue;
            QVERIFY2(s.description.contains("Frau") && s.description.contains("Mann"), qPrintable(s.key));
        }
    }

    void streamEventsSplitAcrossReads()
    {
        QByteArray buffer = "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"delta\":{\"type\":\"text_delta\",\"text\":\"Hö\"}}\n\nevent: ping\ndata: {\"ty";
        auto events = Styles::takeEvents(buffer);
        QCOMPARE(events.size(), 1);
        QCOMPARE(events[0].value("delta").toObject().value("text").toString(), QString("Hö"));
        buffer += "pe\":\"ping\"}\r\n\r\n";
        events = Styles::takeEvents(buffer);
        QCOMPARE(events.size(), 1);
        QCOMPARE(events[0].value("type").toString(), QString("ping"));
        QVERIFY(buffer.isEmpty());
    }

    void whitespaceSurvivesBetweenReads()
    {
        auto delta = [](const char *text) {
            return QByteArray("data: {\"type\":\"content_block_delta\",\"delta\":{\"type\":\"text_delta\",\"text\":\"")
                   + text + "\"}}\n\n";
        };
        Styles::StreamState state;
        QByteArray buffer = delta("steht fest.\\n\\n");
        Styles::consume(buffer, state);
        buffer = delta("Bis bald");
        Styles::consume(buffer, state);
        QCOMPARE(state.text, QString("steht fest.\n\nBis bald"));
        QVERIFY(!state.complete);
    }

    void streamEndAndRefusalAreSeen()
    {
        Styles::StreamState state;
        QByteArray buffer = "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"refusal\"}}\n\n"
                            "data: {\"type\":\"message_stop\"}\n\n"
                            "data: {\"type\":\"error\",\"error\":{\"message\":\"Overloaded\"}}\n\n";
        Styles::consume(buffer, state);
        QCOMPARE(state.stopReason, QString("refusal"));
        QVERIFY(state.complete);
        QCOMPARE(state.error, QString("Overloaded"));
    }

    void tidyReplacesLongDashes()
    {
        QCOMPARE(Styles::tidy(QString::fromUtf8("a — b – c")), QString("a - b - c"));
    }

    void ownStyleFileWins()
    {
        QTemporaryDir dir;
        write(dir.path() + "/stile.json", R"([{"key": "pirat", "name": "Pirat", "description": "Arr."}])");
        Backend backend(dir.path() + "/stile.json", "test-key");
        QCOMPARE(backend.styles().size(), 1);
        QCOMPARE(backend.selected(), QString("pirat"));
        QVERIFY(backend.ready());
    }

    void brokenStyleFileFallsBackAndSays()
    {
        QTemporaryDir dir;
        write(dir.path() + "/stile.json", "nope");
        Backend backend(dir.path() + "/stile.json", "test-key");
        QCOMPARE(backend.styles().size(), 6);
        QVERIFY(!backend.notice().isEmpty());
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
    }

    void translatorDefaultsToGermanWithoutOmarchyLanguage()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", ":/i18n", nullptr, "de");
        QCOMPARE(translator.language(), QString("de"));
        QCOMPARE(translator.translate("Main", "Convert"), QString("Umwandeln"));
        write(dir.path() + "/language", "en\n");
        Translator english(dir.path() + "/language", ":/i18n", nullptr, "de");
        QVERIFY(english.translate("Main", "Convert").isNull());
    }

    void everyVisibleStringIsTranslated()
    {
        QFile qml(QFINDTESTDATA("../src/Main.qml"));
        QFile cpp(QFINDTESTDATA("../src/backend.cpp"));
        QFile catalog(":/i18n/de.json");
        QVERIFY(qml.open(QIODevice::ReadOnly) && cpp.open(QIODevice::ReadOnly) && catalog.open(QIODevice::ReadOnly));
        const QJsonObject de = QJsonDocument::fromJson(catalog.readAll()).object();
        const QString source = QString::fromUtf8(qml.readAll() + cpp.readAll());
        // Joins C++ string literals split across lines before matching.
        QString joined = source;
        joined.replace(QRegularExpression("\"\\s*\\n\\s*\""), "");
        // Built from plain literals: a raw string here trips up moc.
        QRegularExpression call(QStringLiteral("(?:qsTr|tr)\\(\"((?:[^\"\\\\]|\\\\.)*)\""));
        auto it = call.globalMatch(joined);
        int count = 0;
        while (it.hasNext()) {
            QString key = it.next().captured(1);
            key.replace("\\n", "\n");
            QVERIFY2(de.contains(key), qPrintable("missing: " + key));
            ++count;
        }
        QVERIFY(count > 20);
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }

    QTemporaryDir m_config;
};

QTEST_MAIN(StilwandlerTests)
#include "stilwandler_tests.moc"
