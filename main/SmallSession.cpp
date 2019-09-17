/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Vect
    An experimental audio player for plural recordings of a work
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "SmallSession.h"

#include "base/TempWriteFile.h"
#include "base/XmlExportable.h"
#include "base/Exceptions.h"

#include <QTextStream>
#include <QTextCodec>
#include <QXmlDefaultHandler>

void
SmallSession::save(const SmallSession &session, QString sessionFile)
{
    TempWriteFile tempFile(sessionFile);
    QString tempFilePath(tempFile.getTemporaryFilename());

    QFile f(tempFilePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        throw std::runtime_error("Failed to open temporary file for writing");
    }
    
    QTextStream out(&f);
    out.setCodec(QTextCodec::codecForName("UTF-8"));
    
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<!DOCTYPE sonic-lineup>\n"
        << "<vect>\n";

    out << (QString("  <model id=\"1\" type=\"wavefile\" "
                    "mainModel=\"true\" file=\"%1\"/>\n")
            .arg(XmlExportable::encodeEntities(session.mainFile)));

    for (int i = 0; in_range_for(session.additionalFiles, i); ++i) {

        out << (QString("  <model id=\"%1\" type=\"wavefile\" "
                        "mainModel=\"false\" file=\"%2\"/>\n")
                .arg(i + 2)
                .arg(XmlExportable::encodeEntities
                     (session.additionalFiles[i])));
    }

    out << "</vect>\n";

    f.close();

    try {
        tempFile.moveToTarget();
    } catch (const FileOperationFailed &f) {
        throw std::runtime_error("Failed to move temporary file to save target");
    }
}

class SmallSessionReadHandler : public QXmlDefaultHandler
{
public:
    SmallSessionReadHandler() :
        m_inVectContext(false) {
    }
    virtual ~SmallSessionReadHandler() {
    }
    
    bool startElement(const QString & /* namespaceURI */,
                      const QString & /* localName */,
                      const QString &qName,
                      const QXmlAttributes& attributes) override {

        QString name = qName.toLower();

        if (name == "vect") {
            if (m_inVectContext) {
                m_errorString = "Nested session contexts found";
            } else {
                m_inVectContext = true;
            }
        } else if (name == "model") {
            if (!m_inVectContext) {
                m_errorString = "Model found outside session context";
            } else {
                readModel(attributes);
            }
        } else {
            SVCERR << "WARNING: SmallSessionReadHandler: Unexpected element \""
                   << name << "\"" << endl;
        }

        return true;
    }

    void readModel(const QXmlAttributes &attributes) {
        
        QString type = attributes.value("type").trimmed();
        bool isMainModel = (attributes.value("mainModel").trimmed() == "true");

        if (type == "wavefile") {
            
            QString file = attributes.value("file");

            if (isMainModel) {
                if (m_session.mainFile != "") {
                    m_errorString = "Duplicate main model found";
                } else {
                    m_session.mainFile = file;
                }
            } else {
                m_session.additionalFiles.push_back(file);
            }

        } else {
            SVCERR << "WARNING: SmallSessionReadHandler: Unsupported model type \""
                   << type << "\"" << endl;
        }
    }
    
    bool error(const QXmlParseException &exception) override {
        m_errorString =
            QString("%1 at line %2, column %3")
            .arg(exception.message())
            .arg(exception.lineNumber())
            .arg(exception.columnNumber());
        SVCERR << "ERROR: SmallSessionReadHandler: " << m_errorString << endl;
        return QXmlDefaultHandler::error(exception);
    }
    
    bool fatalError(const QXmlParseException &exception) override {
        m_errorString =
            QString("%1 at line %2, column %3")
            .arg(exception.message())
            .arg(exception.lineNumber())
            .arg(exception.columnNumber());
        SVCERR << "ERROR: SmallSessionReadHandler: " << m_errorString << endl;
        return QXmlDefaultHandler::fatalError(exception);
    }

    bool isOK() const { return (m_errorString == ""); }
    QString getErrorString() const { return m_errorString; }
    SmallSession getSession() const { return m_session; }
    
private:
    SmallSession m_session;
    QString m_errorString;
    bool m_inVectContext;
};

SmallSession
SmallSession::load(QString path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error("Failed to open file for reading");
    }

    SmallSessionReadHandler handler;
    QXmlSimpleReader reader;
    reader.setContentHandler(&handler);
    reader.setErrorHandler(&handler);

    QXmlInputSource source(&f);
    bool ok = reader.parse(source);

    if (!handler.isOK()) {
        throw std::runtime_error
            (("Session XML load failed: " + handler.getErrorString())
             .toStdString());
    } else if (!ok) {
        throw std::runtime_error("Session XML parse failed");
    }

    return handler.getSession();
}

