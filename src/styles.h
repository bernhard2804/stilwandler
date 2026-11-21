#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

// A writing style: what the model is told, plus a few before/after pairs,
// which steer the tone far better than any description alone.
struct Example {
    QString original, result;
    QString to; // "frau", "mann" or empty: whom the example message addresses
};

struct Style {
    QString key, name, emoji, shortText, description;
    QList<Example> examples;
};

namespace Styles {

// Parses a style list. Entries with a bad key or missing fields are skipped
// and named in *warnings, so a typo in the user's file never loses the rest.
QList<Style> parse(const QByteArray &json, QString *error, QStringList *warnings = nullptr);

// recipient: "frau", "mann" or anything else for "open". It decides the
// gendered forms of address (Schildmaid instead of Held, and so on).
QString systemPrompt(const Style &style, const QString &recipient = {});

// The Messages API request body, streaming, with a server-side fallback model
// in case the main model declines.
QByteArray requestBody(const Style &style, const QString &text, const QString &model, const QString &effort,
                       const QString &recipient = {});

// Takes every complete Server-Sent Event out of buffer and returns its JSON
// data; an incomplete trailing event stays in the buffer for the next read.
QList<QJsonObject> takeEvents(QByteArray &buffer);

// What a streamed answer has delivered so far. text keeps all whitespace;
// trimming happens only for display, or words across reads would glue.
struct StreamState {
    QString text, stopReason, error;
    bool complete = false;
};

// Takes the complete events out of buffer and applies them to state.
void consume(QByteArray &buffer, StreamState &state);

// Long dashes are replaced with a hyphen; the user does not want them in text.
QString tidy(QString text);

}
