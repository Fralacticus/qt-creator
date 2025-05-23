// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qmlprofileranimationsmodel.h"
#include "qmlprofilermodelmanager.h"
#include "qmlprofilernotesmodel.h"
#include "qmlprofilerrangemodel.h"
#include "qmlprofilerstatemanager.h"
#include "qmlprofilertool.h"
#include "qmlprofilertr.h"
#include "qmlprofilertraceview.h"

#include "quick3dmodel.h"
#include "inputeventsmodel.h"
#include "pixmapcachemodel.h"
#include "debugmessagesmodel.h"
#include "flamegraphview.h"
#include "memoryusagemodel.h"
#include "scenegraphtimelinemodel.h"

// Communication with the other views (limit events to range)
#include "qmlprofilerviewmanager.h"

#include <tracing/timelinezoomcontrol.h>
#include <tracing/timelinemodelaggregator.h>
#include <tracing/timelinerenderer.h>
#include <tracing/timelineoverviewrenderer.h>
#include <tracing/timelinetheme.h>
#include <tracing/timelineformattime.h>

#include <aggregation/aggregate.h>
// Needed for the load&save actions in the context menu
#include <debugger/analyzer/analyzerutils.h>
#include <coreplugin/findplaceholder.h>
#include <utils/styledbar.h>
#include <utils/algorithm.h>

#include <QQmlContext>
#include <QToolButton>
#include <QEvent>
#include <QVBoxLayout>
#include <QGraphicsObject>
#include <QScrollBar>
#include <QSlider>
#include <QMenu>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QApplication>
#include <QRegularExpression>
#include <QTextCursor>

namespace QmlProfiler::Internal {

class QmlProfilerTraceView::QmlProfilerTraceViewPrivate
{
public:
    QmlProfilerTraceViewPrivate(QmlProfilerTraceView *qq) : q(qq) {}
    QmlProfilerTraceView *q;
    QmlProfilerViewManager *m_viewContainer;
    QQuickWidget *m_mainView;
    QmlProfilerModelManager *m_modelManager;
    QVariantList m_suspendedModels;
    Timeline::TimelineModelAggregator *m_modelProxy;
    Timeline::TimelineZoomControl *m_zoomControl;
};

QmlProfilerTraceView::QmlProfilerTraceView(QWidget *parent, QmlProfilerViewManager *container,
                                           QmlProfilerModelManager *modelManager)
    : QWidget(parent), d(new QmlProfilerTraceViewPrivate(this))
{
    setWindowTitle(Tr::tr("Timeline"));
    setObjectName("QmlProfiler.Timeline.Dock");

    d->m_zoomControl = new Timeline::TimelineZoomControl(this);
    modelManager->registerFeatures(0, QmlProfilerModelManager::QmlEventLoader(), [this] {
        if (d->m_suspendedModels.isEmpty()) {
            // Temporarily remove the models, while we're changing them
            d->m_suspendedModels = d->m_modelProxy->models();
            d->m_modelProxy->setModels(QVariantList());
        }
        // Otherwise models are suspended already. This can happen if either acquiring was
        // aborted or we're doing a "restrict to range" which consists of a partial clearing and
        // then re-acquiring of data.
    }, [this, modelManager]() {
        const qint64 start = modelManager->traceStart();
        const qint64 end = modelManager->traceEnd();
        d->m_zoomControl->setTrace(start, end);
        d->m_zoomControl->setRange(start, start + (end - start) / 10);
        d->m_modelProxy->setModels(d->m_suspendedModels);
        d->m_suspendedModels.clear();
    }, [this] {
        d->m_zoomControl->clear();
        if (!d->m_suspendedModels.isEmpty()) {
            d->m_modelProxy->setModels(d->m_suspendedModels);
            d->m_suspendedModels.clear();
        }
    });

    auto groupLayout = new QVBoxLayout;
    groupLayout->setContentsMargins(0, 0, 0, 0);
    groupLayout->setSpacing(0);

    d->m_mainView = new QQuickWidget(this);
    d->m_mainView->setResizeMode(QQuickWidget::SizeRootObjectToView);
    d->m_mainView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusProxy(d->m_mainView);

    Aggregation::aggregate({d->m_mainView, new TraceViewFindSupport(this, modelManager)});

    groupLayout->addWidget(d->m_mainView);
    groupLayout->addWidget(new Core::FindToolBarPlaceHolder(this));
    setLayout(groupLayout);

    d->m_viewContainer = container;
    d->m_modelProxy = new Timeline::TimelineModelAggregator(this);
    d->m_modelProxy->setNotes(modelManager->notesModel());
    d->m_modelManager = modelManager;

    QVariantList models;
    models.append(QVariant::fromValue(new PixmapCacheModel(modelManager, d->m_modelProxy)));
    models.append(QVariant::fromValue(new SceneGraphTimelineModel(modelManager, d->m_modelProxy)));
    models.append(QVariant::fromValue(new MemoryUsageModel(modelManager, d->m_modelProxy)));
    models.append(QVariant::fromValue(new InputEventsModel(modelManager, d->m_modelProxy)));
    models.append(QVariant::fromValue(new DebugMessagesModel(modelManager, d->m_modelProxy)));
    models.append(QVariant::fromValue(new Quick3DModel(modelManager, d->m_modelProxy)));
    models.append(QVariant::fromValue(new QmlProfilerAnimationsModel(modelManager,
                                                                     d->m_modelProxy)));
    for (int i = 0; i < MaximumRangeType; ++i) {
        models.append(QVariant::fromValue(new QmlProfilerRangeModel(modelManager, (RangeType)i,
                                                                    d->m_modelProxy)));
    }
    d->m_modelProxy->setModels(models);

    // Minimum height: 5 rows of 20 pixels + scrollbar of 50 pixels + 20 pixels margin
    setMinimumHeight(170);

    d->m_mainView->engine()->addImportPath(":/qt/qml/");
    Timeline::TimelineTheme::setupTheme(d->m_mainView->engine());

    d->m_mainView->rootContext()->setContextProperty(QLatin1String("timelineModelAggregator"),
                                                     d->m_modelProxy);
    d->m_mainView->rootContext()->setContextProperty(QLatin1String("zoomControl"),
                                                     d->m_zoomControl);
    d->m_mainView->setSource(QUrl(QLatin1String("qrc:/qt/qml/QtCreator/Tracing/MainView.qml")));

    connect(d->m_modelProxy, &Timeline::TimelineModelAggregator::updateCursorPosition,
            this, &QmlProfilerTraceView::updateCursorPosition);
}

QmlProfilerTraceView::~QmlProfilerTraceView()
{
    delete d->m_mainView;
    delete d;
}

bool QmlProfilerTraceView::hasValidSelection() const
{
    QQuickItem *rootObject = d->m_mainView->rootObject();
    if (rootObject)
        return rootObject->property("selectionRangeReady").toBool();
    return false;
}

qint64 QmlProfilerTraceView::selectionStart() const
{
    return d->m_zoomControl->selectionStart();
}

qint64 QmlProfilerTraceView::selectionEnd() const
{
    return d->m_zoomControl->selectionEnd();
}

void QmlProfilerTraceView::clear()
{
    QMetaObject::invokeMethod(d->m_mainView->rootObject(), "clear");
}

void QmlProfilerTraceView::selectByTypeId(int typeId)
{
    QQuickItem *rootObject = d->m_mainView->rootObject();
    if (!rootObject)
        return;
    QMetaObject::invokeMethod(rootObject, "selectByTypeId", Q_ARG(QVariant,QVariant(typeId)));
}

void QmlProfilerTraceView::selectByEventIndex(int modelId, int eventIndex)
{
    QQuickItem *rootObject = d->m_mainView->rootObject();
    if (!rootObject)
        return;

    const int modelIndex = d->m_modelProxy->modelIndexById(modelId);
    QTC_ASSERT(modelIndex != -1, return);
    QMetaObject::invokeMethod(rootObject, "selectByIndices",
                              Q_ARG(QVariant, QVariant(modelIndex)),
                              Q_ARG(QVariant, QVariant(eventIndex)));
}

// Goto source location
void QmlProfilerTraceView::updateCursorPosition()
{
    QQuickItem *rootObject = d->m_mainView->rootObject();
    QString file = rootObject->property("fileName").toString();
    if (!file.isEmpty())
        emit gotoSourceLocation(file, rootObject->property("lineNumber").toInt(),
                                rootObject->property("columnNumber").toInt());

    emit typeSelected(rootObject->property("typeId").toInt());
}

void QmlProfilerTraceView::mousePressEvent(QMouseEvent *event)
{
    d->m_zoomControl->setWindowLocked(true);
    QWidget::mousePressEvent(event);
}

void QmlProfilerTraceView::mouseReleaseEvent(QMouseEvent *event)
{
    d->m_zoomControl->setWindowLocked(false);
    QWidget::mouseReleaseEvent(event);
}

void QmlProfilerTraceView::contextMenuEvent(QContextMenuEvent *ev)
{
    showContextMenu(ev->globalPos());
}

void QmlProfilerTraceView::showContextMenu(QPoint position)
{
    QMenu menu;
    QAction *viewAllAction = nullptr;

    menu.addActions(QmlProfilerTool::profilerContextMenuActions());
    menu.addSeparator();

    QAction *getLocalStatsAction = menu.addAction(Tr::tr("Analyze Current Range"));
    if (!hasValidSelection())
        getLocalStatsAction->setEnabled(false);

    QAction *getGlobalStatsAction = menu.addAction(Tr::tr("Analyze Full Range"));
    if (!d->m_modelManager->isRestrictedToRange())
        getGlobalStatsAction->setEnabled(false);

    if (d->m_zoomControl->traceDuration() > 0) {
        menu.addSeparator();
        viewAllAction = menu.addAction(Tr::tr("Reset Zoom"));
    }

    QAction *selectedAction = menu.exec(position);

    if (selectedAction) {
        if (selectedAction == viewAllAction) {
            d->m_zoomControl->setRange(d->m_zoomControl->traceStart(),
                                       d->m_zoomControl->traceEnd());
        }
        if (selectedAction == getLocalStatsAction) {
            d->m_modelManager->restrictToRange(selectionStart(), selectionEnd());
        }
        if (selectedAction == getGlobalStatsAction)
            d->m_modelManager->restrictToRange(-1, -1);
    }
}

bool QmlProfilerTraceView::isUsable() const
{
    const QSGRendererInterface::GraphicsApi api =
            d->m_mainView->quickWindow()->rendererInterface()->graphicsApi();
    return QSGRendererInterface::isApiRhiBased(api);
}

bool QmlProfilerTraceView::isSuspended() const
{
    return !d->m_suspendedModels.isEmpty();
}

void QmlProfilerTraceView::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::EnabledChange) {
        QQuickItem *rootObject = d->m_mainView->rootObject();
        rootObject->setProperty("enabled", isEnabled());
    }
}

TraceViewFindSupport::TraceViewFindSupport(QmlProfilerTraceView *view,
                                           QmlProfilerModelManager *manager)
    : m_view(view), m_modelManager(manager)
{
}

bool TraceViewFindSupport::supportsReplace() const
{
    return false;
}

Utils::FindFlags TraceViewFindSupport::supportedFindFlags() const
{
    return Utils::FindBackward | Utils::FindCaseSensitively | Utils::FindRegularExpression
            | Utils::FindWholeWords;
}

void TraceViewFindSupport::resetIncrementalSearch()
{
    m_incrementalStartPos = -1;
    m_incrementalWrappedState = false;
}

void TraceViewFindSupport::clearHighlights()
{
}

QString TraceViewFindSupport::currentFindString() const
{
    return QString();
}

QString TraceViewFindSupport::completedFindString() const
{
    return QString();
}

Core::IFindSupport::Result TraceViewFindSupport::findIncremental(const QString &txt,
                                                                 Utils::FindFlags findFlags)
{
    if (m_incrementalStartPos < 0)
        m_incrementalStartPos = qMax(m_currentPosition, 0);
    bool wrapped = false;
    bool found = find(txt, findFlags, m_incrementalStartPos, &wrapped);
    if (wrapped != m_incrementalWrappedState && found) {
        m_incrementalWrappedState = wrapped;
        showWrapIndicator(m_view);
    }
    return found ? Core::IFindSupport::Found : Core::IFindSupport::NotFound;
}

Core::IFindSupport::Result TraceViewFindSupport::findStep(const QString &txt,
                                                          Utils::FindFlags findFlags)
{
    int start = (findFlags & Utils::FindBackward) ? m_currentPosition : m_currentPosition + 1;
    bool wrapped;
    bool found = find(txt, findFlags, start, &wrapped);
    if (wrapped)
        showWrapIndicator(m_view);
    if (found) {
        m_incrementalStartPos = m_currentPosition;
        m_incrementalWrappedState = false;
    }
    return found ? Core::IFindSupport::Found : Core::IFindSupport::NotFound;
}

// "start" is the model index that is searched first in a forward search, i.e. as if the
// "cursor" were between start-1 and start
bool TraceViewFindSupport::find(const QString &txt, Utils::FindFlags findFlags, int start,
                                bool *wrapped)
{
    if (wrapped)
        *wrapped = false;
    if (!findOne(txt, findFlags, start)) {
        int secondStart;
        if (findFlags & Utils::FindBackward)
            secondStart = m_modelManager->notesModel()->count();
        else
            secondStart = 0;
        if (!findOne(txt, findFlags, secondStart))
            return false;
        if (wrapped)
            *wrapped = true;
    }
    return true;
}

// "start" is the model index that is searched first in a forward search, i.e. as if the
// "cursor" were between start-1 and start
bool TraceViewFindSupport::findOne(const QString &txt, Utils::FindFlags findFlags, int start)
{
    bool caseSensitiveSearch = (findFlags & Utils::FindCaseSensitively);
    bool regexSearch = (findFlags & Utils::FindRegularExpression);
    QRegularExpression regexp(regexSearch ? txt : QRegularExpression::escape(txt),
                              caseSensitiveSearch ? QRegularExpression::NoPatternOption
                                                  : QRegularExpression::CaseInsensitiveOption);
    QTextDocument::FindFlags flags;
    if (caseSensitiveSearch)
        flags |= QTextDocument::FindCaseSensitively;
    if (findFlags & Utils::FindWholeWords)
        flags |= QTextDocument::FindWholeWords;
    bool forwardSearch = !(findFlags & Utils::FindBackward);
    int increment = forwardSearch ? +1 : -1;
    int current = forwardSearch ? start : start - 1;
    Timeline::TimelineNotesModel *model = m_modelManager->notesModel();
    while (current >= 0 && current < model->count()) {
        QTextDocument doc(model->text(current)); // for automatic handling of WholeWords option
        if (!doc.find(regexp, 0, flags).isNull()) {
            m_currentPosition = current;
            m_view->selectByEventIndex(model->timelineModel(m_currentPosition),
                                       model->timelineIndex(m_currentPosition));
            QWidget *findBar = QApplication::focusWidget();
            m_view->updateCursorPosition(); // open file/line that belongs to event
            QTC_ASSERT(findBar, return true);
            findBar->setFocus();
            return true;
        }
        current += increment;
    }
    return false;
}

} // namespace QmlProfiler::Internal
