// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "../utils/googletest.h"

#include <sqlitedatabase.h>
#include <sqliteglobal.h>
#include <sqlitelibraryinitializer.h>

#include <utils/processreaper.h>
#include <utils/temporarydirectory.h>

#include <QGuiApplication>
#include <QLoggingCategory>

#ifdef WITH_BENCHMARKS
#include <benchmark/benchmark.h>
#endif

class Environment : public testing::Environment
{
public:
    void SetUp() override
    {
        const QString temporayDirectoryPath = QDir::tempPath() + "/QtCreator-UnitTests-XXXXXX";
        Utils::TemporaryDirectory::setMasterTemporaryDirectory(temporayDirectoryPath);
        qputenv("TMPDIR", Utils::TemporaryDirectory::masterDirectoryPath().toUtf8());
        qputenv("TEMP", Utils::TemporaryDirectory::masterDirectoryPath().toUtf8());
    }

    void TearDown() override {}
};

int main(int argc, char *argv[])
{
    Sqlite::LibraryInitializer::initialize();
    Sqlite::Database::activateLogging();

    QGuiApplication application(argc, argv);
    testing::InitGoogleTest(&argc, argv);
#ifdef WITH_BENCHMARKS
    benchmark::Initialize(&argc, argv);
#endif

    auto environment = std::make_unique<Environment>();
    testing::AddGlobalTestEnvironment(environment.release());

    int testsHaveErrors = RUN_ALL_TESTS();

    Utils::ProcessReaper::deleteAll();
#ifdef WITH_BENCHMARKS
    if (testsHaveErrors == 0  && application.arguments().contains(QStringLiteral("--with-benchmarks")))
        benchmark::RunSpecifiedBenchmarks();
#endif

    return testsHaveErrors;
}
