/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#ifndef HTMLPARSER_H
#define HTMLPARSER_H

// A parser for HTML and XHTML.
// This file is based on HTMLParser.py.

// XXX There should be a way to distinguish between PCDATA (parsed
// character data -- the normal case), RCDATA (replaceable character
// data -- only char and entity references and end tags are special)
// and CDATA (character data -- only end tags are special).

// Find tags and other markup and call handler functions.

//    Usage:
//        HTMLParser p
//        p.feed(data)
//        ...
//        p.close()

//    Start tags are handled by calling handleStartTag() or
//    handleStartEndTag(); end tags by handleEndTag().  The
//    data between tags is passed from the parser to the derived class
//    by calling handleData() with the data as argument (the data
//    may be split up in arbitrary chunks).  Entity references are
//    passed by calling handleEntityRef() with the entity
//    reference as the argument.  Numeric character references are
//    passed to handleCharRef() with the string containing the
//    reference as the argument.

#include <QHash>
#include <QRegExp>
#include <QString>

class QTextDecoder;

// Exception raised for all parse errors.
class HTMLParseError
{
public:
    HTMLParseError(const QString &msg, int lineNo, int offset);

    QString message() const;
    int lineNo() const;
    int offset() const;

private:
    QString m_msg;
    int m_lineNo;
    int m_offset;
};

class HTMLParser
{
public:
    HTMLParser();
    virtual ~HTMLParser();

    void reset();
    void feed(const QByteArray &data);
    void close();

    static QChar entityToChar(const QString &entityName);
    static QString unescape(QString str);

protected:
    // Overridable -- finish processing of start+end tag: <tag.../>
    virtual void handleStartEndTag(const QString &tag, const QHash<QString, QString> &attrs);
    // Overridable -- handle start tag
    virtual void handleStartTag(const QString &tag, const QHash<QString, QString> &attrs);
    // Overridable -- handle end tag
    virtual void handleEndTag(const QString &tag);
    // Overridable -- handle character reference
    virtual void handleCharRef(const QString &name);
    // Overridable -- handle entity reference
    virtual void handleEntityRef(const QString &name);
    // Overridable -- handle data
    virtual void handleData(const QString &data);
    // Overridable -- handle comment
    virtual void handleComment(const QString &data);
    // Overridable -- handle declaration
    virtual void handleDecl(const QString &decl);
    // Overridable -- handle processing instruction
    virtual void handlePI(const QString &data);
    virtual void handleUnknownDecl(const QString &data);

private:
    void error(const QString &message);
    void setCDATAMode(const QString &elem);
    void clearCDATAMode();
    int updatePos(int i, int j);
    void goAhead(bool end);
    int parseHTMLDeclaration(int i);
    int parseBogusComment(int i, bool report = true);
    int parsePI(int i);
    int parseStartTag(int i);
    int checkForWholeStartTag(int i);
    int parseEndTag(int i);
    int parseMarkedSection(int i, bool report = true);
    int parseComment(int i, bool report = true);
    int scanName(int i, int declstartpos, QString &name);

    const QRegExp m_rxInterestingNormal;
    const QRegExp m_rxIncomplete;
    const QRegExp m_rxEntityRef;
    const QRegExp m_rxCharRef;
    const QRegExp m_rxStarttagOpen;
    const QRegExp m_rxPiClose;
    const QRegExp m_rxCommentClose;
    const QRegExp m_rxTagFind;
    const QRegExp m_rxAttrFind;
    const QRegExp m_rxLocateStarttagEnd;
    const QRegExp m_rxEndEndtag;
    const QRegExp m_rxEndtagFind;
    const QRegExp m_rxDeclName;
    const QRegExp m_rxMarkedSectionClose;
    const QRegExp m_rxMsMarkedSectionClose;

    QString m_rawData;
    QString m_lastTag;
    QString m_cdataElem;
    QRegExp m_interesting;
    QString m_startTagText;
    int m_lineNo;
    int m_offset;
    QTextDecoder *m_decoder;
};

#endif // HTMLPARSER_H
