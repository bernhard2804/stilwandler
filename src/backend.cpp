#include "backend.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>

namespace {
const char *ApiUrl = "https://api.anthropic.com/v1/messages";
const char *FallbackBeta = "server-side-fallback-2026-07-01";
}

Backend::Backend(const QString &stylesFile, const QString &apiKey, QObject *parent)
    : QObject(parent), m_settings("stilwandler", "stilwandler") {
    // Model and effort can be changed in ~/.config/stilwandler/stilwandler.conf.
    m_model = m_settings.value("model", "claude-opus-5-5").toString();
    m_effort = m_settings.value("effort", "low").toString();

    loadStyles(stylesFile.isEmpty()
                   ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/stilwandler/stile.json"
                   : stylesFile);
    const QString last = m_settings.value("style").toString();
    m_selected = style(last) ? last : m_styles.value(0).key;
    const QString to = m_settings.value("recipient").toString();
    m_recipient = to == "frau" || to == "mann" ? to : QStringLiteral("offen");

    m_apiKey = !apiKey.isEmpty() ? apiKey : qEnvironmentVariable("ANTHROPIC_API_KEY").trimmed();
    if (m_apiKey.isEmpty())
        lookUpKey();
}

void Backend::loadStyles(const QString &path) {
    QString error;
    QStringList skipped;
    QFile own(path);
    if (own.exists()) {
        if (own.open(QIODevice::ReadOnly))
            m_styles = Styles::parse(own.readAll(), &error, &skipped);
        else
            error = own.errorString();
        if (m_styles.isEmpty())
            m_notice = (tr("Your style file %1 could not be read (%2). Using the built-in styles.").arg(path, error));
        else if (!skipped.isEmpty())
            m_notice = (tr("Skipped invalid styles in %1: %2").arg(path, skipped.join(", ")));
    }
    if (m_styles.isEmpty()) {
        QFile bundled(":/stile.json");
        if (bundled.open(QIODevice::ReadOnly))
            m_styles = Styles::parse(bundled.readAll(), &error);
    }
}

// The key lives in the desktop keyring, not in a file; the lookup may have
// to wait for the keyring to be unlocked, so it runs without blocking.
void Backend::lookUpKey() {
    const QString tool = QStandardPaths::findExecutable("secret-tool");
    if (tool.isEmpty()) {
        setError(tr("secret-tool is missing, so the API key cannot be read from the keyring."));
        return;
    }
    connect(&m_secretTool, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            setError(tr("secret-tool could not be started: %1").arg(m_secretTool.errorString()));
    });
    connect(&m_secretTool, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const QString key = QString::fromUtf8(m_secretTool.readAllStandardOutput()).trimmed();
        if (status != QProcess::NormalExit || code != 0 || key.isEmpty()) {
            // secret-tool gives the same exit code for "not stored" and "keyring
            // locked or unavailable", so the message names both.
            setError(tr("No API key found, or the keyring is locked. Store the key in a terminal with:\n"
                        "secret-tool store --label='Stilwandler' service stilwandler key anthropic"));
            return;
        }
        m_apiKey = key;
        emit readyChanged();
    });
    m_secretTool.start(tool, {"lookup", "service", "stilwandler", "key", "anthropic"});
}

QVariantList Backend::styles() const {
    QVariantList list;
    for (const Style &s : m_styles)
        list.append(QVariantMap{{"key", s.key}, {"name", s.name}, {"emoji", s.emoji}, {"short", s.shortText}});
    return list;
}

const Style *Backend::style(const QString &key) const {
    for (const Style &s : m_styles)
        if (s.key == key)
            return &s;
    return nullptr;
}

void Backend::setSelected(const QString &key) {
    if (key == m_selected || !style(key))
        return;
    m_selected = key;
    m_settings.setValue("style", key);
    emit selectedChanged();
}

void Backend::setRecipient(const QString &recipient) {
    const QString value = recipient == "frau" || recipient == "mann" ? recipient : QStringLiteral("offen");
    if (value == m_recipient)
        return;
    m_recipient = value;
    m_settings.setValue("recipient", value);
    emit recipientChanged();
}

void Backend::setError(const QString &message) {
    if (message == m_error)
        return;
    m_error = message;
    emit errorChanged();
}

void Backend::setOutput(const QString &text) {
    if (text == m_output)
        return;
    m_output = text;
    emit outputChanged();
}

void Backend::convert(const QString &input) {
    const QString text = input.trimmed();
    const Style *s = style(m_selected);
    if (text.isEmpty() || !s)
        return;
    if (m_apiKey.isEmpty()) {
        setError(tr("No API key yet. Store it in a terminal with:\n"
                    "secret-tool store --label='Stilwandler' service stilwandler key anthropic"));
        return;
    }
    cancel();
    setError({});
    setOutput({});
    m_buffer.clear();
    m_stream = {};

    QNetworkRequest request{QUrl(ApiUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("x-api-key", m_apiKey.toUtf8());
    request.setRawHeader("anthropic-version", "2026-10-21");
    if (!m_model.startsWith("claude-haiku"))
        request.setRawHeader("anthropic-beta", FallbackBeta);
    // Aborts only when nothing arrives for this long, not on a long answer.
    request.setTransferTimeout(90000);
    // Never follow a redirect: it would resend the API key to another host.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);

    m_reply = m_network.post(request, Styles::requestBody(*s, text, m_model, m_effort, m_recipient));
    connect(m_reply, &QNetworkReply::readyRead, this, &Backend::readStream);
    connect(m_reply, &QNetworkReply::finished, this, &Backend::finishReply);
    emit busyChanged();
}

void Backend::cancel() {
    if (!m_reply)
        return;
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();
    emit busyChanged();
}

void Backend::readStream() {
    if (!m_reply)
        return;
    // An HTTP error comes back as one JSON object, not as a stream; leave it
    // for finishReply.
    if (m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() >= 400)
        return;
    m_buffer += m_reply->readAll();
    Styles::consume(m_buffer, m_stream);
    if (!m_stream.error.isEmpty())
        setError(tr("The service reported an error: %1").arg(m_stream.error));
    setOutput(Styles::tidy(m_stream.text).trimmed());
}

void Backend::finishReply() {
    if (!m_reply)
        return;
    QNetworkReply *reply = m_reply;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 400) {
        const QJsonObject body = QJsonDocument::fromJson(reply->readAll()).object();
        QString message = body.value("error").toObject().value("message").toString();
        if (message.isEmpty())
            message = tr("no details");
        if (status == 401)
            setError(tr("The API key was rejected. Store the right one with secret-tool."));
        else if (status == 429)
            setError(tr("Too many requests right now. Try again in a moment."));
        else if (status == 529 || status == 503)
            setError(tr("The service is overloaded right now. Try again in a moment."));
        else
            setError(tr("The service answered with error %1: %2").arg(status).arg(message));
    } else if (reply->error() != QNetworkReply::NoError) {
        setError(tr("No connection to the service: %1").arg(reply->errorString()));
    } else {
        readStream();
        if (m_stream.stopReason == "refusal") {
            setOutput({});
            setError(tr("The model declined this text. Try a different wording or style."));
        } else if (m_stream.stopReason == "max_tokens") {
            setError(tr("The answer was cut off because it got too long."));
        } else if (m_output.isEmpty() && m_error.isEmpty()) {
            setError(tr("The answer was empty. Please try again."));
        } else if (!m_stream.complete && m_error.isEmpty()) {
            // A stream that closes without message_stop was cut off; say so
            // rather than offer half a message for pasting.
            setError(tr("The answer is incomplete because the connection ended early. Please try again."));
        }
    }
    m_reply = nullptr;
    reply->deleteLater();
    emit busyChanged();
}

// On Wayland, a clipboard owned by this window vanishes when it closes;
// wl-copy keeps serving the text after the app has quit.
bool Backend::copy(const QString &text) {
    if (text.isEmpty())
        return false;
    const QString wlCopy = QStandardPaths::findExecutable("wl-copy");
    if (!wlCopy.isEmpty()) {
        QProcess process;
        process.start(wlCopy, {"--type", "text/plain;charset=utf-8"});
        if (process.waitForStarted(2000)) {
            process.write(text.toUtf8());
            process.closeWriteChannel();
            if (process.waitForFinished(3000) && process.exitCode() == 0)
                return true;
        }
    }
    QGuiApplication::clipboard()->setText(text);
    setError(tr("wl-copy did not work; the text stays in the clipboard only while this window is open."));
    return false;
}
