// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "valgrindtestrunnertest.h"

#include "valgrindprocess.h"
#include "xmlprotocol/error.h"
#include "xmlprotocol/frame.h"
#include "xmlprotocol/stack.h"

#include <utils/algorithm.h>
#include <utils/processinterface.h>

#include <QDebug>
#include <QDir>
#include <QStringList>
#include <QTest>

#define HEADER_LENGTH 3

using namespace Valgrind::XmlProtocol;
using namespace Utils;

namespace Valgrind::Internal {

static const QString appSrcDir(TESTRUNNER_SRC_DIR);
static const QString appBinDir(TESTRUNNER_APP_DIR);

static bool on64bit()
{
    return sizeof(char*) == 8;
}

static QString srcDirForApp(const QString &app)
{
    return QDir::cleanPath(appSrcDir + '/' + app);
}

class ValgrindTestRunnerTest : public QObject
{
    Q_OBJECT

public:
    explicit ValgrindTestRunnerTest(QObject *parent = nullptr);

private slots:
    void init();
    void cleanup();

    void testLeak1();
    void testLeak2();
    void testLeak3();
    void testLeak4();

    void testUninit1();
    void testUninit2();
    void testUninit3();

    void testFree1();
    void testFree2();

    void testInvalidjump();
    void testSyscall();
    void testOverlap();

    void logMessageReceived(const QByteArray &message);
    void internalError(const QString &error);
    void error(const Valgrind::XmlProtocol::Error &error);

private:
    QString runTestBinary(const QString &binary, const QStringList &vArgs = QStringList());

    ValgrindProcess *m_runner = nullptr;
    QList<QByteArray> m_logMessages;
    QList<XmlProtocol::Error> m_errors;
    bool m_expectCrash = false;
};

ValgrindTestRunnerTest::ValgrindTestRunnerTest(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<Error>();
}

QString ValgrindTestRunnerTest::runTestBinary(const QString &binary, const QStringList &vArgs)
{
    const QFileInfo binPathFileInfo(appBinDir, binary);
    if (!binPathFileInfo.isExecutable())
        return QString();

    ProcessRunData debuggee;
    const QString &binPath = binPathFileInfo.canonicalFilePath();
    debuggee.command.setExecutable(Utils::FilePath::fromString(binPath));
    debuggee.environment = Utils::Environment::systemEnvironment();

    m_runner->setLocalServerAddress(QHostAddress::LocalHost);
    m_runner->setValgrindCommand({"valgrind", {"--num-callers=50", "--track-origins=yes", vArgs}});
    m_runner->setDebuggee(debuggee);
    m_runner->runBlocking();
    return binPath;
}

void ValgrindTestRunnerTest::logMessageReceived(const QByteArray &message)
{
    qDebug() << "log message received:" << message;
    m_logMessages << message;
}

void ValgrindTestRunnerTest::internalError(const QString &errorString)
{
    if (!m_expectCrash)
        QFAIL(qPrintable(errorString));
    else
        qDebug() << "expected crash:" << errorString;
}

void ValgrindTestRunnerTest::error(const Error &error)
{
    m_errors << error;
}

void ValgrindTestRunnerTest::cleanup()
{
    Q_ASSERT(m_runner);
    delete m_runner;
    m_runner = nullptr;

    m_logMessages.clear();
    m_errors.clear();
    m_expectCrash = false;
}

void ValgrindTestRunnerTest::init()
{
    const Utils::Environment &sysEnv = Utils::Environment::systemEnvironment();
    auto fileName = sysEnv.searchInPath("valgrind");
    if (fileName.isEmpty())
        QSKIP("This test needs valgrind in PATH");
    Q_ASSERT(m_logMessages.isEmpty());

    Q_ASSERT(!m_runner);
    m_runner = new ValgrindProcess;
    m_runner->setProcessChannelMode(QProcess::ForwardedChannels);
    connect(m_runner, &ValgrindProcess::logMessageReceived,
            this, &ValgrindTestRunnerTest::logMessageReceived);
    connect(m_runner, &ValgrindProcess::processErrorReceived,
            this, &ValgrindTestRunnerTest::internalError);
    connect(m_runner, &ValgrindProcess::internalError, this, &ValgrindTestRunnerTest::internalError);
    connect(m_runner, &ValgrindProcess::error, this, &ValgrindTestRunnerTest::error);
}

//BEGIN: Actual test cases

void ValgrindTestRunnerTest::testLeak1()
{
    const QString binary = runTestBinary("leak1/leak1");
    if (binary.isEmpty())
        QSKIP("You need to pass BUILD_TESTS when building Qt Creator or build valgrind testapps "
              "manually before executing this test.");

    QVERIFY(m_logMessages.isEmpty());

    QCOMPARE(m_errors.count(), 1);
    const Error error = m_errors.first();
    QCOMPARE(error.kind(), int(Leak_DefinitelyLost));
    QCOMPARE(error.leakedBlocks(), qint64(1));
    QCOMPARE(error.leakedBytes(), quint64(8));
    QCOMPARE(error.stacks().count(), 1);
    const Stack stack = error.stacks().constFirst();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 2);
    {
        const Frame frame = stack.frames().at(0);
        if (on64bit())
            QCOMPARE(frame.functionName(), QString("operator new(unsigned long)"));
        else
            QCOMPARE(frame.functionName(), QString("operator new(unsigned int)"));
    }
    {
        const Frame frame = stack.frames().at(1);
        QCOMPARE(frame.functionName(), QString("main"));
        QCOMPARE(frame.line(), 5 + HEADER_LENGTH);

        QCOMPARE(frame.object(), binary);
        QCOMPARE(frame.fileName(), QString("main.cpp"));
        QCOMPARE(QDir::cleanPath(frame.directory()), srcDirForApp("leak1"));
    }
}

void ValgrindTestRunnerTest::testLeak2()
{
    const QString binary = runTestBinary("leak2/leak2");
    if (binary.isEmpty())
        QSKIP("You need to pass BUILD_TESTS when building Qt Creator or build valgrind testapps "
              "manually before executing this test.");

    QVERIFY(m_logMessages.isEmpty());
    QCOMPARE(m_errors.count(), 1);
    const Error error = m_errors.first();
    QCOMPARE(error.kind(), int(Leak_PossiblyLost));
    QCOMPARE(error.leakedBlocks(), qint64(1));
    QCOMPARE(error.leakedBytes(), quint64(5));
    QCOMPARE(error.stacks().count(), 1);
    const Stack stack = error.stacks().constFirst();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 3);
    {
        const Frame frame = stack.frames().at(0);
        QCOMPARE(frame.functionName(), QString("malloc"));
    }
    {
        const Frame frame = stack.frames().at(1);
        QCOMPARE(frame.functionName(), QString("strdup"));
    }
    {
        const Frame frame = stack.frames().at(2);
        if (on64bit()) {
            QCOMPARE(frame.functionName(), QString("main"));
            QCOMPARE(frame.line(), 7 + HEADER_LENGTH);

            QCOMPARE(frame.object(), binary);
            QCOMPARE(frame.fileName(), QString("main.cpp"));
            QCOMPARE(QDir::cleanPath(frame.directory()), srcDirForApp("leak2"));
        } else {
           QCOMPARE(frame.functionName(), QString("(below main)"));
        }
    }
}

void ValgrindTestRunnerTest::testLeak3()
{
    const QString binary = runTestBinary("leak3/leak3", QStringList{"--show-reachable=yes"});
    if (binary.isEmpty())
        QSKIP("You need to pass BUILD_TESTS when building Qt Creator or build valgrind testapps "
              "manually before executing this test.");
    QVERIFY(m_logMessages.isEmpty());

    QCOMPARE(m_errors.count(), 1);
    const Error error = m_errors.first();
    QCOMPARE(error.kind(), int(Leak_StillReachable));
    QCOMPARE(error.leakedBlocks(), qint64(1));
    QCOMPARE(error.leakedBytes(), quint64(5));
    QCOMPARE(error.stacks().count(), 1);
    const Stack stack = error.stacks().constFirst();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 3);
    {
        const Frame frame = stack.frames().at(0);
        QCOMPARE(frame.functionName(), QString("malloc"));
    }
    {
        const Frame frame = stack.frames().at(1);
        QCOMPARE(frame.functionName(), QString("strdup"));
    }
    {
        const Frame frame = stack.frames().at(2);
        if (on64bit()) {
            QCOMPARE(frame.functionName(), QString("main"));
            QCOMPARE(frame.line(), 7 + HEADER_LENGTH);

            QCOMPARE(frame.object(), binary);
            QCOMPARE(frame.fileName(), QString("main.cpp"));
            QCOMPARE(QDir::cleanPath(frame.directory()), srcDirForApp("leak3"));
        } else {
            QCOMPARE(frame.functionName(), QString("(below main)"));
        }
    }
}

void ValgrindTestRunnerTest::testLeak4()
{
    const QString app("leak4");
    const QString binary = runTestBinary(app + '/' + app,
                                         QStringList() << "--show-reachable=yes");
    if (binary.isEmpty())
        QSKIP("You need to pass BUILD_TESTS when building Qt Creator or build valgrind testapps "
              "manually before executing this test.");
    const QString srcDir = srcDirForApp("leak4");

    QVERIFY(m_logMessages.isEmpty());

    QVERIFY(m_errors.count() >= 3);
    //BEGIN first error
    {
    // depending on the valgrind version the errors can be different - try to find the correct one
    const Error error = Utils::findOrDefault(m_errors, [](const Error &err) {
        return err.kind() == Leak_IndirectlyLost;
    });

    QCOMPARE(error.leakedBlocks(), qint64(1));
    QCOMPARE(error.leakedBytes(), quint64(8));
    QCOMPARE(error.stacks().count(), 1);
    const Stack stack = error.stacks().constFirst();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 3);
    {
        const Frame frame = stack.frames().at(0);
        if (on64bit())
            QCOMPARE(frame.functionName(), QString("operator new(unsigned long)"));
        else
            QCOMPARE(frame.functionName(), QString("operator new(unsigned int)"));
    }
    {
        const Frame frame = stack.frames().at(1);
        QCOMPARE(frame.functionName(), QString("Foo::Foo()"));
        QCOMPARE(frame.line(), 6 + HEADER_LENGTH);

        QCOMPARE(frame.object(), binary);
        QCOMPARE(frame.fileName(), QString("main.cpp"));
        QCOMPARE(QDir::cleanPath(frame.directory()), srcDir);
    }
    {
        const Frame frame = stack.frames().at(2);
        QCOMPARE(frame.functionName(), QString("main"));
        QCOMPARE(frame.line(), 14 + HEADER_LENGTH);

        QCOMPARE(frame.object(), binary);
        QCOMPARE(frame.fileName(), QString("main.cpp"));
        QCOMPARE(QDir::cleanPath(frame.directory()), srcDir);
    }
    }
    //BEGIN second error
    {
    const Error error = Utils::findOrDefault(m_errors, [](const Error &err) {
        return err.kind() == Leak_DefinitelyLost;
    });

    QCOMPARE(error.leakedBlocks(), qint64(1));
    if (on64bit())
        QCOMPARE(error.leakedBytes(), quint64(16));
    else
        QCOMPARE(error.leakedBytes(), quint64(12));
    QCOMPARE(error.stacks().count(), 1);
    const Stack stack = error.stacks().constFirst();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 2);
    {
        const Frame frame = stack.frames().at(0);
        if (on64bit())
            QCOMPARE(frame.functionName(), QString("operator new(unsigned long)"));
        else
            QCOMPARE(frame.functionName(), QString("operator new(unsigned int)"));
    }
    {
        const Frame frame = stack.frames().at(1);
        QCOMPARE(frame.functionName(), QString("main"));
        QCOMPARE(frame.line(), 14 + HEADER_LENGTH);

        QCOMPARE(frame.object(), binary);
        QCOMPARE(frame.fileName(), QString("main.cpp"));
        QCOMPARE(QDir::cleanPath(frame.directory()), srcDir);
    }
    }
}

void ValgrindTestRunnerTest::testUninit1()
{
    const QString app("uninit1");
    const QString binary = runTestBinary(app + '/' + app);
    if (binary.isEmpty())
        QSKIP("You need to pass BUILD_TESTS when building Qt Creator or build valgrind testapps "
              "manually before executing this test.");
    const QString srcDir = srcDirForApp(app);

    QVERIFY(m_logMessages.isEmpty());

    QCOMPARE(m_errors.count(), 1);
    const Error error = m_errors.first();
    QCOMPARE(error.kind(), int(UninitCondition));
    QCOMPARE(error.stacks().count(), 2);
    //BEGIN first stack
    {
    const Stack stack = error.stacks().constFirst();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 1);

    const Frame frame = stack.frames().constFirst();
    QCOMPARE(frame.functionName(), QString("main"));
    QCOMPARE(frame.line(), 4 + HEADER_LENGTH);

    QCOMPARE(frame.object(), binary);
    QCOMPARE(frame.fileName(), QString("main.cpp"));
    QCOMPARE(QDir::cleanPath(frame.directory()), srcDir);
    }
    //BEGIN second stack
    {
    const Stack stack = error.stacks().constLast();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 1);

    const Frame frame = stack.frames().constFirst();
    QCOMPARE(frame.functionName(), QString("main"));
    QCOMPARE(frame.line(), 2 + HEADER_LENGTH);

    QCOMPARE(frame.object(), binary);
    QCOMPARE(frame.fileName(), QString("main.cpp"));
    QCOMPARE(QDir::cleanPath(frame.directory()), srcDir);
    }
}

void ValgrindTestRunnerTest::testUninit2()
{
    const QString app("uninit2");
    m_expectCrash = true;
    const QString binary = runTestBinary(app + '/' + app);
    if (binary.isEmpty())
        QSKIP("You need to pass BUILD_TESTS when building Qt Creator or build valgrind testapps "
              "manually before executing this test.");
    const QString srcDir = srcDirForApp(app);

    QVERIFY(m_logMessages.size() < 2);
    if (!m_logMessages.isEmpty()) {
        QVERIFY2(m_logMessages.first().contains("If you believe"),
                 m_logMessages.first().constData());
    }

    QCOMPARE(m_errors.count(), 2);
    //BEGIN first error
    {
    const Error error = m_errors.first();
    QCOMPARE(error.kind(), int(UninitValue));
    QCOMPARE(error.stacks().count(), 2);
    //BEGIN first stack
    {
    const Stack stack = error.stacks().constFirst();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 1);

    const Frame frame = stack.frames().constFirst();
    QCOMPARE(frame.functionName(), QString("main"));
    QCOMPARE(frame.line(), 4 + HEADER_LENGTH);

    QCOMPARE(frame.object(), binary);
    QCOMPARE(frame.fileName(), QString("main.cpp"));
    QCOMPARE(QDir::cleanPath(frame.directory()), srcDir);
    }
    //BEGIN second stack
    {
    const Stack stack = error.stacks().constLast();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 1);

    const Frame frame = stack.frames().constFirst();
    QCOMPARE(frame.functionName(), QString("main"));
    QCOMPARE(frame.line(), 2 + HEADER_LENGTH);

    QCOMPARE(frame.object(), binary);
    QCOMPARE(frame.fileName(), QString("main.cpp"));
    QCOMPARE(QDir::cleanPath(frame.directory()), srcDir);
    }
    }
    //BEGIN second error
    {
    const Error error = m_errors.last();
    QCOMPARE(error.kind(), int(InvalidWrite));
    QCOMPARE(error.stacks().count(), 1);

    const Stack stack = error.stacks().constFirst();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 1);

    const Frame frame = stack.frames().constFirst();
    QCOMPARE(frame.functionName(), QString("main"));
    QCOMPARE(frame.line(), 4 + HEADER_LENGTH);

    QCOMPARE(frame.object(), binary);
    QCOMPARE(frame.fileName(), QString("main.cpp"));
    QCOMPARE(QDir::cleanPath(frame.directory()), srcDir);
    }
}

void ValgrindTestRunnerTest::testUninit3()
{
    const QString app("uninit3");
    m_expectCrash = true;
    const QString binary = runTestBinary(app + '/' + app);
    if (binary.isEmpty())
        QSKIP("You need to pass BUILD_TESTS when building Qt Creator or build valgrind testapps "
              "manually before executing this test.");
    const QString srcDir = srcDirForApp(app);

    QVERIFY(m_logMessages.size() < 2);
    if (!m_logMessages.isEmpty()) {
        QVERIFY2(m_logMessages.first().contains("If you believe"),
                 m_logMessages.first().constData());
    }

    QCOMPARE(m_errors.count(), 2);
    //BEGIN first error
    {
    const Error error = m_errors.first();
    QCOMPARE(error.kind(), int(UninitValue));
    QCOMPARE(error.stacks().count(), 2);
    //BEGIN first stack
    {
    const Stack stack = error.stacks().constFirst();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 1);

    const Frame frame = stack.frames().constFirst();
    QCOMPARE(frame.functionName(), QString("main"));
    QCOMPARE(frame.line(), 4 + HEADER_LENGTH);

    QCOMPARE(frame.object(), binary);
    QCOMPARE(frame.fileName(), QString("main.cpp"));
    QCOMPARE(QDir::cleanPath(frame.directory()), srcDir);
    }
    //BEGIN second stack
    {
    const Stack stack = error.stacks().constLast();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 1);

    const Frame frame = stack.frames().constFirst();
    QCOMPARE(frame.functionName(), QString("main"));
    QCOMPARE(frame.line(), 2 + HEADER_LENGTH);

    QCOMPARE(frame.object(), binary);
    QCOMPARE(frame.fileName(), QString("main.cpp"));
    QCOMPARE(QDir::cleanPath(frame.directory()), srcDir);
    }
    }
    //BEGIN second error
    {
    const Error error = m_errors.last();
    QCOMPARE(error.kind(), int(InvalidRead));
    QCOMPARE(error.stacks().count(), 1);

    const Stack stack = error.stacks().constFirst();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 1);

    const Frame frame = stack.frames().constFirst();
    QCOMPARE(frame.functionName(), QString("main"));
    QCOMPARE(frame.line(), 4 + HEADER_LENGTH);

    QCOMPARE(frame.object(), binary);
    QCOMPARE(frame.fileName(), QString("main.cpp"));
    QCOMPARE(QDir::cleanPath(frame.directory()), srcDir);
    }
}

void ValgrindTestRunnerTest::testSyscall()
{
    const QString app("syscall");
    const QString binary = runTestBinary(app + '/' + app);
    if (binary.isEmpty())
        QSKIP("You need to pass BUILD_TESTS when building Qt Creator or build valgrind testapps "
              "manually before executing this test.");
    const QString srcDir = srcDirForApp(app);

    QVERIFY(m_logMessages.isEmpty());

    QCOMPARE(m_errors.count(), 1);
    const Error error = m_errors.first();
    QCOMPARE(error.kind(), int(SyscallParam));
    QCOMPARE(error.stacks().count(), 2);
    //BEGIN first stack
    {
    const Stack stack = error.stacks().constFirst();
    QCOMPARE(stack.line(), qint64(-1));
    if (on64bit()) {
        QCOMPARE(stack.frames().count(), 4);

        {
            const Frame frame = stack.frames().at(0);
            QCOMPARE(frame.functionName(), QString("_Exit"));
        }
        {
            const Frame frame = stack.frames().at(1);
            QCOMPARE(frame.functionName(), QString("__run_exit_handlers"));
        }
        {
            const Frame frame = stack.frames().at(2);
            QCOMPARE(frame.functionName(), QString("exit"));
        }
        {
            const Frame frame = stack.frames().at(3);
            QCOMPARE(frame.functionName(), QString("(below main)"));
        }
    } else {
        QCOMPARE(stack.frames().count(), 1);
        {
            const Frame frame = stack.frames().at(0);
            QCOMPARE(frame.functionName(), QString("_Exit"));
        }
    }
    }
    //BEGIN second stack
    {
    const Stack stack = error.stacks().constLast();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 1);

    const Frame frame = stack.frames().constFirst();
    QCOMPARE(frame.functionName(), QString("main"));
    QCOMPARE(frame.line(), 2 + HEADER_LENGTH);

    QCOMPARE(frame.object(), binary);
    QCOMPARE(frame.fileName(), QString("main.cpp"));
    QCOMPARE(QDir::cleanPath(frame.directory()), srcDir);
    }
}

void ValgrindTestRunnerTest::testFree1()
{
    const QString app("free1");
    const QString binary = runTestBinary(app + '/' + app);
    if (binary.isEmpty())
        QSKIP("You need to pass BUILD_TESTS when building Qt Creator or build valgrind testapps "
              "manually before executing this test.");
    const QString srcDir = srcDirForApp(app);

    QVERIFY(m_logMessages.isEmpty());

    QCOMPARE(m_errors.count(), 1);
    const Error error = m_errors.first();
    QCOMPARE(error.kind(), int(InvalidFree));
    QVERIFY(error.stacks().count() >= 2);
    //BEGIN first stack
    {
    const Stack stack = error.stacks().constFirst();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 2);

    {
    const Frame frame = stack.frames().constFirst();
    QVERIFY2(frame.functionName().contains("operator delete"), qPrintable(frame.functionName()));
    }
    {
    const Frame frame = stack.frames().constLast();
    QCOMPARE(frame.functionName(), QString("main"));
    QCOMPARE(frame.line(), 7 + HEADER_LENGTH);

    QCOMPARE(frame.object(), binary);
    QCOMPARE(frame.fileName(), QString("main.cpp"));
    QCOMPARE(QDir::cleanPath(frame.directory()), srcDir);
    }
    }
    //BEGIN second stack
    {
    const Stack stack = error.stacks().at(1);
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 2);

    {
    const Frame frame = stack.frames().constFirst();
    QVERIFY2(frame.functionName().contains("operator delete"), qPrintable(frame.functionName()));
    }
    {
    const Frame frame = stack.frames().constLast();
    QCOMPARE(frame.functionName(), QString("main"));
    QCOMPARE(frame.line(), 6 + HEADER_LENGTH);

    QCOMPARE(frame.object(), binary);
    QCOMPARE(frame.fileName(), QString("main.cpp"));
    QCOMPARE(QDir::cleanPath(frame.directory()), srcDir);
    }
    }
}

void ValgrindTestRunnerTest::testFree2()
{
    const QString app("free2");
    const QString binary = runTestBinary(app + '/' + app);
    if (binary.isEmpty())
        QSKIP("You need to pass BUILD_TESTS when building Qt Creator or build valgrind testapps "
              "manually before executing this test.");
    const QString srcDir = srcDirForApp(app);

    QVERIFY(m_logMessages.isEmpty());

    QCOMPARE(m_errors.count(), 1);
    const Error error = m_errors.first();
    QCOMPARE(error.kind(), int(MismatchedFree));
    QCOMPARE(error.stacks().count(), 2);
    //BEGIN first stack
    {
    const Stack stack = error.stacks().constFirst();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 2);

    {
    const Frame frame = stack.frames().constFirst();
    QCOMPARE(frame.functionName(), QString("free"));
    }
    {
    const Frame frame = stack.frames().constLast();
    QCOMPARE(frame.functionName(), QString("main"));
    QCOMPARE(frame.line(), 6 + HEADER_LENGTH);

    QCOMPARE(frame.object(), binary);
    QCOMPARE(frame.fileName(), QString("main.cpp"));
    QCOMPARE(QDir::cleanPath(frame.directory()), srcDir);
    }
    }
    //BEGIN second stack
    {
    const Stack stack = error.stacks().constLast();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 2);


    {
    const Frame frame = stack.frames().constFirst();
    if (on64bit())
        QCOMPARE(frame.functionName(), QString("operator new(unsigned long)"));
    else
        QCOMPARE(frame.functionName(), QString("operator new(unsigned int)"));
    }
    {
    const Frame frame = stack.frames().constLast();
    QCOMPARE(frame.functionName(), QString("main"));
    QCOMPARE(frame.line(), 5 + HEADER_LENGTH);

    QCOMPARE(frame.object(), binary);
    QCOMPARE(frame.fileName(), QString("main.cpp"));
    QCOMPARE(QDir::cleanPath(frame.directory()), srcDir);
    }
    }
}

void ValgrindTestRunnerTest::testInvalidjump()
{
    const QString app("invalidjump");
    m_expectCrash = true;
    const QString binary = runTestBinary(app + '/' + app);
    if (binary.isEmpty())
        QSKIP("You need to pass BUILD_TESTS when building Qt Creator or build valgrind testapps "
              "manually before executing this test.");

    QVERIFY(m_logMessages.isEmpty());

    QCOMPARE(m_errors.count(), 1);
    const Error error = m_errors.first();
    QCOMPARE(error.kind(), int(InvalidJump));
    QCOMPARE(error.stacks().count(), 1);
    const Stack stack = error.stacks().constFirst();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 2);
    QVERIFY(!stack.auxWhat().isEmpty());
    {
        const Frame frame = stack.frames().at(0);
        QCOMPARE(frame.instructionPointer(), quint64(0));
    }
    {
        const Frame frame = stack.frames().at(1);
        QCOMPARE(frame.functionName(), QString("(below main)"));
    }
}

void ValgrindTestRunnerTest::testOverlap()
{
    const QString app("overlap");
    m_expectCrash = true;
    const QString binary = runTestBinary(app + '/' + app);
    if (binary.isEmpty())
        QSKIP("You need to pass BUILD_TESTS when building Qt Creator or build valgrind testapps "
              "manually before executing this test.");
    const QString srcDir = srcDirForApp(app);

    QVERIFY(m_logMessages.isEmpty());

    QVERIFY(m_errors.count() <= 1);
    if (m_errors.isEmpty())
        QSKIP("Some libc implementations automatically use memmove in case of an overlap.");

    const Error error = m_errors.first();
    QCOMPARE(error.kind(), int(Overlap));
    QCOMPARE(error.stacks().count(), 1);
    const Stack stack = error.stacks().constFirst();
    QCOMPARE(stack.line(), qint64(-1));
    QCOMPARE(stack.frames().count(), 2);
    {
        const Frame frame = stack.frames().at(0);
        QVERIFY(frame.functionName().startsWith("memcpy"));
    }
    {
        const Frame frame = stack.frames().constLast();
        QCOMPARE(frame.functionName(), QLatin1String("main"));
        QCOMPARE(frame.line(), 6 + HEADER_LENGTH);

        QCOMPARE(frame.object(), binary);
        QCOMPARE(frame.fileName(), QLatin1String("main.cpp"));
        QCOMPARE(QDir::cleanPath(frame.directory()), srcDir);
    }
}

QObject *createValgrindTestRunnerTest()
{
    return new ValgrindTestRunnerTest;
}

} // namespace Valgrind::Internal

#include "valgrindtestrunnertest.moc"
