// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "treeviewcombobox.h"

#include <QWheelEvent>

namespace Utils {

class TreeViewComboBoxView final : public QTreeView
{
public:
    TreeViewComboBoxView()
    {
        // TODO: Disable the root for all items (with a custom delegate?)
        setRootIsDecorated(false);
    }

    void adjustWidth(int width)
    {
        setMaximumWidth(width);
        setMinimumWidth(qMin(qMax(sizeHintForColumn(0), minimumSizeHint().width()), width));
    }
};

TreeViewComboBox::TreeViewComboBox(QWidget *parent)
    : QComboBox(parent)
{
    m_view = new TreeViewComboBoxView;
    m_view->setHeaderHidden(true);
    m_view->setItemsExpandable(true);
    setView(m_view);
    m_view->viewport()->installEventFilter(this);
}

QModelIndex TreeViewComboBox::indexAbove(QModelIndex index)
{
    do
        index = m_view->indexAbove(index);
    while (index.isValid() && !(model()->flags(index) & Qt::ItemIsSelectable));
    return index;
}

QModelIndex TreeViewComboBox::indexBelow(QModelIndex index)
{
    do
        index = m_view->indexBelow(index);
    while (index.isValid() && !(model()->flags(index) & Qt::ItemIsSelectable));
    return index;
}

QModelIndex TreeViewComboBox::lastIndex(const QModelIndex &index)
{
    if (index.isValid() && !m_view->isExpanded(index))
        return index;

    int rows = m_view->model()->rowCount(index);
    if (rows == 0)
        return index;
    return lastIndex(m_view->model()->index(rows - 1, 0, index));
}

void TreeViewComboBox::wheelEvent(QWheelEvent *e)
{
    QModelIndex index = m_view->currentIndex();
    if (e->angleDelta().y() > 0)
        index = indexAbove(index);
    else if (e->angleDelta().y() < 0)
        index = indexBelow(index);

    e->accept();
    if (!index.isValid())
        return;

    setCurrentIndex(index);

    // for compatibility we emit activated with a useless row parameter
    emit activated(index.row());
}

void TreeViewComboBox::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Up || e->key() == Qt::Key_PageUp) {
        setCurrentIndex(indexAbove(m_view->currentIndex()));
    } else if (e->key() == Qt::Key_Down || e->key() == Qt::Key_PageDown) {
        setCurrentIndex(indexBelow(m_view->currentIndex()));
    } else if (e->key() == Qt::Key_Home) {
        QModelIndex index = m_view->model()->index(0, 0);
        if (index.isValid() && !(model()->flags(index) & Qt::ItemIsSelectable))
            index = indexBelow(index);
        setCurrentIndex(index);
    } else if (e->key() == Qt::Key_End) {
        QModelIndex index = lastIndex(m_view->rootIndex());
        if (index.isValid() && !(model()->flags(index) & Qt::ItemIsSelectable))
            index = indexAbove(index);
        setCurrentIndex(index);
    } else {
        QComboBox::keyPressEvent(e);
        return;
    }

    e->accept();
}

void TreeViewComboBox::setCurrentIndex(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    setRootModelIndex(model()->parent(index));
    QComboBox::setCurrentIndex(index.row());
    setRootModelIndex(QModelIndex());
    m_view->setCurrentIndex(index);
}

bool TreeViewComboBox::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress && object == view()->viewport()) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        QModelIndex index = view()->indexAt(mouseEvent->pos());
        if (!view()->visualRect(index).contains(mouseEvent->pos()))
            m_skipNextHide = true;
    }
    return false;
}

void TreeViewComboBox::showPopup()
{
    m_view->adjustWidth(topLevelWidget()->geometry().width());
    QComboBox::showPopup();
}

void TreeViewComboBox::hidePopup()
{
    if (m_skipNextHide)
        m_skipNextHide = false;
    else
        QComboBox::hidePopup();
}

QTreeView *TreeViewComboBox::view() const
{
    return m_view;
}

} // Utils
