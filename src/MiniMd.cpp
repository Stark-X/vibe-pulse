#include "MiniMd.h"

#include <QStringList>

static QString inlineMdToHtml(const QString &src)
{
    QString out;
    int pos = 0;
    while (pos < src.size()) {
        const int open = src.indexOf(QLatin1Char('`'), pos);
        if (open < 0) {
            out += src.mid(pos).toHtmlEscaped();
            break;
        }
        out += src.mid(pos, open - pos).toHtmlEscaped();
        const int close = src.indexOf(QLatin1Char('`'), open + 1);
        if (close < 0) {
            out += src.mid(open).toHtmlEscaped();
            break;
        }
        const QString code = src.mid(open + 1, close - open - 1).toHtmlEscaped();
        out += QStringLiteral("<code style=\"padding:1px 4px;border-radius:4px;"
                              "background:rgba(255,255,255,0.08);\">");
        out += code;
        out += QStringLiteral("</code>");
        pos = close + 1;
    }
    return out;
}

QString miniMdToHtml(const QString &src)
{
    QString out;
    bool inList = false;

    const auto closeList = [&]() {
        if (!inList) return;
        out += QStringLiteral("</ul>\n");
        inList = false;
    };

    for (const QString &rawLine : src.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            closeList();
            continue;
        }
        if (line.startsWith(QStringLiteral("# "))) {
            closeList();
            out += QStringLiteral("<h4>") + inlineMdToHtml(line.mid(2).trimmed()) + QStringLiteral("</h4>\n");
            continue;
        }
        if (line.startsWith(QStringLiteral("## "))) {
            closeList();
            out += QStringLiteral("<h4>") + inlineMdToHtml(line.mid(3).trimmed()) + QStringLiteral("</h4>\n");
            continue;
        }
        if (line.startsWith(QStringLiteral("- "))) {
            if (!inList) { out += QStringLiteral("<ul>\n"); inList = true; }
            out += QStringLiteral("<li>") + inlineMdToHtml(line.mid(2).trimmed()) + QStringLiteral("</li>\n");
            continue;
        }
        closeList();
        out += QStringLiteral("<p>") + inlineMdToHtml(line) + QStringLiteral("</p>\n");
    }

    closeList();
    return out;
}
