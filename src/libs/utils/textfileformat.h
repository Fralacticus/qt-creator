// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "result.h"
#include "utils_global.h"

#include <QStringList>

#include <utility>

QT_BEGIN_NAMESPACE
class QTextCodec;
class QByteArray;
QT_END_NAMESPACE

namespace Utils {

class FilePath;

class QTCREATOR_UTILS_EXPORT TextFileFormat
{
public:
    enum LineTerminationMode
    {
        LFLineTerminator,
        CRLFLineTerminator,
        NativeLineTerminator =
#if defined (Q_OS_WIN)
        CRLFLineTerminator,
#else
        LFLineTerminator
#endif
    };

    enum ReadResult
    {
        ReadSuccess,
        ReadEncodingError,
        ReadMemoryAllocationError,
        ReadIOError
    };

    TextFileFormat();

    static TextFileFormat detect(const QByteArray &data);

    bool decode(const QByteArray &data, QString *target) const;
    bool decode(const QByteArray &data, QStringList *target) const;

    static ReadResult readFile(const FilePath &filePath, const QTextCodec *defaultCodec,
                               QStringList *plainText, TextFileFormat *format, QString *errorString,
                               QByteArray *decodingErrorSample = nullptr);
    static ReadResult readFile(const FilePath &filePath, const QTextCodec *defaultCodec,
                               QString *plainText, TextFileFormat *format, QString *errorString,
                               QByteArray *decodingErrorSample = nullptr);
    static ReadResult readFileUTF8(const FilePath &filePath, const QTextCodec *defaultCodec,
                                   QByteArray *plainText, QString *errorString);
    static tl::expected<QString, std::pair<ReadResult, QString>>
    readFile(const FilePath &filePath, const QTextCodec *defaultCodec);

    Utils::Result<> writeFile(const FilePath &filePath, QString plainText) const;

    static QByteArray decodingErrorSample(const QByteArray &data);

    LineTerminationMode lineTerminationMode = NativeLineTerminator;
    bool hasUtf8Bom = false;

    QByteArray codecName() const;
    void setCodecName(const QByteArray &codec);

    // FIXME: Avoid.
    const QTextCodec *codec() const;
    void setCodec(const QTextCodec *codec);

private:
    const QTextCodec *m_codec = nullptr;
};

} // namespace Utils
