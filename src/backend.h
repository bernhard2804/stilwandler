#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QSettings>
#include <QVariantList>

#include "styles.h"

class QNetworkReply;

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList styles READ styles CONSTANT)
    Q_PROPERTY(QString selected READ selected WRITE setSelected NOTIFY selectedChanged)
    Q_PROPERTY(QString recipient READ recipient WRITE setRecipient NOTIFY recipientChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QString output READ output NOTIFY outputChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString notice READ notice CONSTANT)
    Q_PROPERTY(QString model READ model CONSTANT)

public:
    // stylesFile: the user's own style list, used instead of the bundled one
    // when it exists. apiKey: skips the keyring lookup (tests, environment).
    explicit Backend(const QString &stylesFile = {}, const QString &apiKey = {}, QObject *parent = nullptr);

    QVariantList styles() const;
    QString selected() const { return m_selected; }
    void setSelected(const QString &key);
    // "frau", "mann" or "offen"; remembered like the style.
    QString recipient() const { return m_recipient; }
    void setRecipient(const QString &recipient);
    bool busy() const { return m_reply != nullptr; }
    bool ready() const { return !m_apiKey.isEmpty(); }
    QString output() const { return m_output; }
    QString error() const { return m_error; }
    // Problems with the user's style file; kept apart so later errors never hide them.
    QString notice() const { return m_notice; }
    QString model() const { return m_model; }

    Q_INVOKABLE void convert(const QString &text);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool copy(const QString &text);

signals:
    void selectedChanged();
    void recipientChanged();
    void busyChanged();
    void readyChanged();
    void outputChanged();
    void errorChanged();

private:
    void loadStyles(const QString &path);
    void lookUpKey();
    void setError(const QString &message);
    void setOutput(const QString &text);
    void readStream();
    void finishReply();
    const Style *style(const QString &key) const;

    QList<Style> m_styles;
    QSettings m_settings;
    QString m_selected, m_recipient, m_model, m_effort, m_apiKey, m_output, m_error, m_notice;
    Styles::StreamState m_stream;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QByteArray m_buffer;
    QProcess m_secretTool;
};
