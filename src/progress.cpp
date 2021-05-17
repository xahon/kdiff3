/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "progress.h"
#include "common.h"
#include "defmac.h"

#include <QApplication>
#include <QEventLoop>
#include <QLabel>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QStatusBar>
#include <QThread>
#include <QTimer>
#include <QTimerEvent>
#include <QVBoxLayout>

#include <KIO/Job>
#include <KLocalizedString>

ProgressDialog* g_pProgressDialog = nullptr;

ProgressDialog::ProgressDialog(QWidget* pParent, QStatusBar* pStatusBar)
    : QDialog(pParent), m_pStatusBar(pStatusBar)
{
    m_pGuiThread = QThread::currentThread();

    setObjectName("ProgressDialog");
    m_bStayHidden = false;
    setModal(true);
    QVBoxLayout* layout = new QVBoxLayout(this);

    m_pInformation = new QLabel(" ", this);
    layout->addWidget(m_pInformation);

    m_pProgressBar = new QProgressBar();
    m_pProgressBar->setRange(0, 1000);
    layout->addWidget(m_pProgressBar);

    m_pSubInformation = new QLabel(" ", this);
    layout->addWidget(m_pSubInformation);

    m_pSubProgressBar = new QProgressBar();
    m_pSubProgressBar->setRange(0, 1000);
    layout->addWidget(m_pSubProgressBar);

    m_pSlowJobInfo = new QLabel(" ", this);
    layout->addWidget(m_pSlowJobInfo);

    QHBoxLayout* hlayout = new QHBoxLayout();
    layout->addLayout(hlayout);
    hlayout->addStretch(1);
    m_pAbortButton = new QPushButton(i18n("&Cancel"), this);
    hlayout->addWidget(m_pAbortButton);
    chk_connect(m_pAbortButton, &QPushButton::clicked, this, &ProgressDialog::slotAbort);
    if(m_pStatusBar != nullptr)
    {
        m_pStatusBarWidget = new QWidget;
        QHBoxLayout* pStatusBarLayout = new QHBoxLayout(m_pStatusBarWidget);
        pStatusBarLayout->setContentsMargins(0, 0, 0, 0);
        pStatusBarLayout->setSpacing(3);
        m_pStatusProgressBar = new QProgressBar;
        m_pStatusProgressBar->setRange(0, 1000);
        m_pStatusProgressBar->setTextVisible(false);
        m_pStatusAbortButton = new QPushButton(i18n("&Cancel"));
        chk_connect(m_pStatusAbortButton, &QPushButton::clicked, this, &ProgressDialog::slotAbort);
        pStatusBarLayout->addWidget(m_pStatusProgressBar);
        pStatusBarLayout->addWidget(m_pStatusAbortButton);
        m_pStatusBar->addPermanentWidget(m_pStatusBarWidget, 0);
        m_pStatusBarWidget->setFixedHeight(m_pStatusBar->height());
        m_pStatusBarWidget->hide();
    }
    else
    {
        m_pStatusProgressBar = nullptr;
        m_pStatusAbortButton = nullptr;
    }

    m_progressDelayTimer = 0;
    m_delayedHideTimer = 0;
    m_delayedHideStatusBarWidgetTimer = 0;
    resize(400, 100);
#ifndef AUTOTEST
    m_t1.start();
    m_t2.start();
#endif
    m_bWasCancelled = false;
    m_eCancelReason = eUserAbort;
    m_pJob = nullptr;
}

void ProgressDialog::setStayHidden(bool bStayHidden)
{
#ifndef AUTOTEST
    if(m_bStayHidden != bStayHidden)
    {
        m_bStayHidden = bStayHidden;
        if(m_pStatusBarWidget != nullptr)
        {
            if(m_bStayHidden)
            {
                if(m_delayedHideStatusBarWidgetTimer)
                {
                    killTimer(m_delayedHideStatusBarWidgetTimer);
                    m_delayedHideStatusBarWidgetTimer = 0;
                }
                m_pStatusBarWidget->show();
            }
            else
                hideStatusBarWidget(); // delayed
        }
        if(isVisible() && m_bStayHidden)
            hide(); // delayed hide
    }
#else
    Q_UNUSED(bStayHidden);
#endif
}

void ProgressDialog::push()
{
#ifndef AUTOTEST
    ProgressLevelData pld;
    if(!m_progressStack.empty())
    {
        pld.m_dRangeMax = m_progressStack.back().m_dSubRangeMax;
        pld.m_dRangeMin = m_progressStack.back().m_dSubRangeMin;
    }
    else
    {
        m_bWasCancelled = false;
        m_t1.restart();
        m_t2.restart();
        if(!m_bStayHidden)
            show();
    }

    m_progressStack.push_back(pld);
#endif
}

void ProgressDialog::pop(bool bRedrawUpdate)
{
#ifndef AUTOTEST
    if(!m_progressStack.empty())
    {
        m_progressStack.pop_back();
        if(m_progressStack.empty())
        {
            hide();
        }
        else
            recalc(bRedrawUpdate);
    }
#else
    Q_UNUSED(bRedrawUpdate);
#endif
}

void ProgressDialog::setInformation(const QString& info, int current, bool bRedrawUpdate)
{
#ifndef AUTOTEST
    if(m_progressStack.empty())
        return;
    ProgressLevelData& pld = m_progressStack.back();
    pld.m_current = current;
    int level = m_progressStack.size();
    if(level == 1)
    {
        m_pInformation->setText(info);
        m_pSubInformation->setText("");
        if(m_pStatusBar != nullptr && m_bStayHidden)
            m_pStatusBar->showMessage(info);
    }
    else if(level == 2)
    {
        m_pSubInformation->setText(info);
    }
    recalc(bRedrawUpdate);
#else
    Q_UNUSED(info);
    Q_UNUSED(bRedrawUpdate);
    Q_UNUSED(current);
#endif
}

void ProgressDialog::setInformation(const QString& info, bool bRedrawUpdate)
{
#ifndef AUTOTEST
    if(m_progressStack.empty())
        return;
    //ProgressLevelData& pld = m_progressStack.back();
    int level = m_progressStack.size();
    if(level == 1)
    {
        m_pInformation->setText(info);
        m_pSubInformation->setText("");
        if(m_pStatusBar && m_bStayHidden)
            m_pStatusBar->showMessage(info);
    }
    else if(level == 2)
    {
        m_pSubInformation->setText(info);
    }
    recalc(bRedrawUpdate);
#else
    Q_UNUSED(info);
    Q_UNUSED(bRedrawUpdate);
#endif
}

void ProgressDialog::setMaxNofSteps(const qint64 maxNofSteps)
{
#ifndef AUTOTEST
    if(m_progressStack.empty() || maxNofSteps == 0)
        return;
    ProgressLevelData& pld = m_progressStack.back();
    pld.m_maxNofSteps = maxNofSteps;
    pld.m_current = 0;
#else
    Q_UNUSED(maxNofSteps);
#endif
}

void ProgressDialog::addNofSteps(const qint64 nofSteps)
{
#ifndef AUTOTEST
    if(m_progressStack.empty())
        return;
    ProgressLevelData& pld = m_progressStack.back();
    pld.m_maxNofSteps.fetchAndAddRelaxed(nofSteps);
#else
    Q_UNUSED(nofSteps);
#endif
}

void ProgressDialog::step(bool bRedrawUpdate)
{
#ifndef AUTOTEST
    if(m_progressStack.empty())
        return;
    ProgressLevelData& pld = m_progressStack.back();
    pld.m_current.fetchAndAddRelaxed(1);
    recalc(bRedrawUpdate);
#else
    Q_UNUSED(bRedrawUpdate);
#endif
}

void ProgressDialog::setCurrent(qint64 subCurrent, bool bRedrawUpdate)
{
#ifndef AUTOTEST
    if(m_progressStack.empty())
        return;
    ProgressLevelData& pld = m_progressStack.back();
    pld.m_current = subCurrent;
    recalc(bRedrawUpdate);
#else
    Q_UNUSED(subCurrent);
    Q_UNUSED(bRedrawUpdate);
#endif
}

void ProgressDialog::clear()
{
#ifndef AUTOTEST
    if(m_progressStack.isEmpty())
        return;

    ProgressLevelData& pld = m_progressStack.back();
    setCurrent(pld.m_maxNofSteps);
#endif
}

// The progressbar goes from 0 to 1 usually.
// By supplying a subrange transformation the subCurrent-values
// 0 to 1 will be transformed to dMin to dMax instead.
// Requirement: 0 < dMin < dMax < 1
void ProgressDialog::setRangeTransformation(double dMin, double dMax)
{
#ifndef AUTOTEST
    if(m_progressStack.empty())
        return;
    ProgressLevelData& pld = m_progressStack.back();
    pld.m_dRangeMin = dMin;
    pld.m_dRangeMax = dMax;
    pld.m_current = 0;
#else
    Q_UNUSED(dMin);
    Q_UNUSED(dMax);
#endif
}

void ProgressDialog::setSubRangeTransformation(double dMin, double dMax)
{
#ifndef AUTOTEST
    if(m_progressStack.empty())
        return;
    ProgressLevelData& pld = m_progressStack.back();
    pld.m_dSubRangeMin = dMin;
    pld.m_dSubRangeMax = dMax;
#else
    Q_UNUSED(dMin);
    Q_UNUSED(dMax);
#endif
}

void ProgressDialog::enterEventLoop(KJob* pJob, const QString& jobInfo)
{
#ifndef AUTOTEST
    m_pJob = pJob;
    m_currentJobInfo = jobInfo;
    m_pSlowJobInfo->setText(m_currentJobInfo);
    if(m_progressDelayTimer)
        killTimer(m_progressDelayTimer);
    m_progressDelayTimer = startTimer(3000); /* 3 s delay */

    // immediately show the progress dialog for KIO jobs, because some KIO jobs require password authentication,
    // but if the progress dialog pops up at a later moment, this might cover the login dialog and hide it from the user.
    if(m_pJob && !m_bStayHidden)
        show();

    // instead of using exec() the eventloop is entered and exited often without hiding/showing the window.
    if(m_eventLoop == nullptr)
    {
        m_eventLoop = QPointer<QEventLoop>(new QEventLoop(this));
        m_eventLoop->exec(); // this function only returns after ProgressDialog::exitEventLoop() is called.
        m_eventLoop.clear();
    }
    else
    {
        m_eventLoop->processEvents(QEventLoop::WaitForMoreEvents);
    }
#else
    Q_UNUSED(pJob);
    Q_UNUSED(jobInfo);
#endif
}

void ProgressDialog::exitEventLoop()
{
#ifndef AUTOTEST
    if(m_progressDelayTimer)
        killTimer(m_progressDelayTimer);
    m_progressDelayTimer = 0;
    m_pJob = nullptr;
    if(m_eventLoop != nullptr)
        m_eventLoop->exit();
#endif
}

void ProgressDialog::recalc(bool bUpdate)
{
#ifndef AUTOTEST
    if(!m_bWasCancelled)
    {
        if(QThread::currentThread() == m_pGuiThread)
        {
            if(m_progressDelayTimer)
                killTimer(m_progressDelayTimer);
            m_progressDelayTimer = 0;
            if(!m_bStayHidden)
                m_progressDelayTimer = startTimer(3000); /* 3 s delay */

            int level = m_progressStack.size();
            if((bUpdate && level == 1) || m_t1.elapsed() > 200)
            {
                if(m_progressStack.empty())
                {
                    m_pProgressBar->setValue(0);
                    m_pSubProgressBar->setValue(0);
                }
                else
                {
                    QList<ProgressLevelData>::iterator i = m_progressStack.begin();
                    int value = int(1000.0 * (getAtomic(i->m_current) * (i->m_dRangeMax - i->m_dRangeMin) / getAtomic(i->m_maxNofSteps) + i->m_dRangeMin));
                    m_pProgressBar->setValue(value);
                    if(m_bStayHidden && m_pStatusProgressBar)
                        m_pStatusProgressBar->setValue(value);

                    ++i;
                    if(i != m_progressStack.end())
                        m_pSubProgressBar->setValue(int(1000.0 * (getAtomic(i->m_current) * (i->m_dRangeMax - i->m_dRangeMin) / getAtomic(i->m_maxNofSteps) + i->m_dRangeMin)));
                    else
                        m_pSubProgressBar->setValue(int(1000.0 * m_progressStack.front().m_dSubRangeMin));
                }

                if(!m_bStayHidden && !isVisible())
                    show();
                qApp->processEvents();
                m_t1.restart();
            }
        }
        else
        {
            QMetaObject::invokeMethod(this, "recalc", Qt::QueuedConnection, Q_ARG(bool, bUpdate));
        }
    }
#else
    Q_UNUSED(bUpdate);
#endif
}

void ProgressDialog::show()
{
#ifndef AUTOTEST
    if(m_progressDelayTimer)
        killTimer(m_progressDelayTimer);
    if(m_delayedHideTimer)
        killTimer(m_delayedHideTimer);
    m_progressDelayTimer = 0;
    m_delayedHideTimer = 0;
    if(!isVisible() && (parentWidget() == nullptr || parentWidget()->isVisible()))
    {
        QDialog::show();
    }
#endif
}

void ProgressDialog::hide()
{
#ifndef AUTOTEST
    if(m_progressDelayTimer)
        killTimer(m_progressDelayTimer);
    m_progressDelayTimer = 0;
    // Calling QDialog::hide() directly doesn't always work. (?)
    if(m_delayedHideTimer)
        killTimer(m_delayedHideTimer);
    m_delayedHideTimer = startTimer(100);
#endif
}

void ProgressDialog::delayedHide()
{
#ifndef AUTOTEST
    if(m_pJob != nullptr)
    {
        m_pJob->kill(KJob::Quietly);
        m_pJob = nullptr;
    }
    QDialog::hide();
    m_pInformation->setText("");

    //m_progressStack.clear();

    m_pProgressBar->setValue(0);
    m_pSubProgressBar->setValue(0);
    m_pSubInformation->setText("");
    m_pSlowJobInfo->setText("");
#endif
}

void ProgressDialog::hideStatusBarWidget()
{
#ifndef AUTOTEST
    if(m_delayedHideStatusBarWidgetTimer)
        killTimer(m_delayedHideStatusBarWidgetTimer);
    m_delayedHideStatusBarWidgetTimer = startTimer(100);
#endif
}

void ProgressDialog::delayedHideStatusBarWidget()
{
#ifndef AUTOTEST
    if(m_progressDelayTimer)
        killTimer(m_progressDelayTimer);
    m_progressDelayTimer = 0;
    if(m_pStatusBarWidget != nullptr)
    {
        m_pStatusBarWidget->hide();
        m_pStatusProgressBar->setValue(0);
        m_pStatusBar->clearMessage();
    }
#endif
}

void ProgressDialog::reject()
{
#ifndef AUTOTEST
    cancel(eUserAbort);
    QDialog::reject();
#endif
}

void ProgressDialog::slotAbort()
{
#ifndef AUTOTEST
    reject();
#endif
}

bool ProgressDialog::wasCancelled()
{
#ifndef AUTOTEST
    if(QThread::currentThread() == m_pGuiThread)
    {
        if(m_t2.elapsed() > 100)
        {
            qApp->processEvents();
            m_t2.restart();
        }
    }
    return m_bWasCancelled;
#else
    return false;
#endif
}

void ProgressDialog::clearCancelState()
{
#ifndef AUTOTEST
    m_bWasCancelled = false;
#endif
}

void ProgressDialog::cancel(e_CancelReason eCancelReason)
{
#ifndef AUTOTEST
    if(!m_bWasCancelled)
    {
        m_bWasCancelled = true;
        m_eCancelReason = eCancelReason;
        if(m_eventLoop != nullptr)
            m_eventLoop->exit(1);
    }
#else
    Q_UNUSED(eCancelReason);
#endif
}

ProgressDialog::e_CancelReason ProgressDialog::cancelReason()
{
    return m_eCancelReason;
}

void ProgressDialog::timerEvent(QTimerEvent* te)
{
#ifndef AUTOTEST
    if(te->timerId() == m_progressDelayTimer)
    {
        if(!isVisible() && !m_bStayHidden)
        {
            show();
        }
        m_pSlowJobInfo->setText(m_currentJobInfo);
    }
    else if(te->timerId() == m_delayedHideTimer)
    {
        killTimer(m_delayedHideTimer);
        m_delayedHideTimer = 0;
        delayedHide();
    }
    else if(te->timerId() == m_delayedHideStatusBarWidgetTimer)
    {
        killTimer(m_delayedHideStatusBarWidgetTimer);
        m_delayedHideStatusBarWidgetTimer = 0;
        delayedHideStatusBarWidget();
    }
#else
    Q_UNUSED(te);
#endif
}
#ifndef AUTOTEST

ProgressProxy::ProgressProxy()
{
    g_pProgressDialog->push();
}
#else
ProgressProxy::ProgressProxy()=default;
#endif

#ifndef AUTOTEST
ProgressProxy::~ProgressProxy()
{
    g_pProgressDialog->pop(false);
}

#else
ProgressProxy::~ProgressProxy() = default;
#endif

void ProgressProxy::enterEventLoop(KJob* pJob, const QString& jobInfo)
{
#ifndef AUTOTEST
    g_pProgressDialog->enterEventLoop(pJob, jobInfo);
#else
    Q_UNUSED(pJob);
    Q_UNUSED(jobInfo);
#endif
}

void ProgressProxy::exitEventLoop()
{
#ifndef AUTOTEST
    g_pProgressDialog->exitEventLoop();
#endif
}

QDialog* ProgressProxy::getDialog()
{
    return g_pProgressDialog;
}

void ProgressProxy::setInformation(const QString& info, bool bRedrawUpdate)
{
#ifndef AUTOTEST
    g_pProgressDialog->setInformation(info, bRedrawUpdate);
#else
    Q_UNUSED(info);
    Q_UNUSED(bRedrawUpdate);
#endif
}

void ProgressProxy::setInformation(const QString& info, int current, bool bRedrawUpdate)
{
#ifndef AUTOTEST
    g_pProgressDialog->setInformation(info, current, bRedrawUpdate);
#else
    Q_UNUSED(info);
    Q_UNUSED(bRedrawUpdate);
    Q_UNUSED(current);
#endif
}

void ProgressProxy::setCurrent(qint64 current, bool bRedrawUpdate)
{
#ifndef AUTOTEST
    g_pProgressDialog->setCurrent(current, bRedrawUpdate);
#else
    Q_UNUSED(bRedrawUpdate);
    Q_UNUSED(current);
#endif
}

void ProgressProxy::step(bool bRedrawUpdate)
{
#ifndef AUTOTEST
    g_pProgressDialog->step(bRedrawUpdate);
#else
    Q_UNUSED(bRedrawUpdate);
#endif
}

void ProgressProxy::clear()
{
#ifndef AUTOTEST
    g_pProgressDialog->clear();
#endif
}

void ProgressProxy::setMaxNofSteps(const qint64 maxNofSteps)
{
#ifndef AUTOTEST
    g_pProgressDialog->setMaxNofSteps(maxNofSteps);
#else
    Q_UNUSED(maxNofSteps);
#endif
}

void ProgressProxy::addNofSteps(const qint64 nofSteps)
{
#ifndef AUTOTEST
    g_pProgressDialog->addNofSteps(nofSteps);
#else
    Q_UNUSED(nofSteps)
#endif
}

bool ProgressProxy::wasCancelled()
{
#ifndef AUTOTEST
    return g_pProgressDialog->wasCancelled();
#else
    return false;
#endif
}

void ProgressProxy::setRangeTransformation(double dMin, double dMax)
{
#ifndef AUTOTEST
    g_pProgressDialog->setRangeTransformation(dMin, dMax);
#else
    Q_UNUSED(dMin);
    Q_UNUSED(dMax);
#endif
}

void ProgressProxy::setSubRangeTransformation(double dMin, double dMax)
{
#ifndef AUTOTEST
    g_pProgressDialog->setSubRangeTransformation(dMin, dMax);
#else
    Q_UNUSED(dMin);
    Q_UNUSED(dMax);
#endif
}

void ProgressProxy::recalc()
{
#ifndef AUTOTEST
    g_pProgressDialog->recalc(true);
#endif
}
