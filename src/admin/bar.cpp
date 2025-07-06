/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2024 Felix Ernst <felixernst@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "bar.h"

#include "dolphinviewcontainer.h"
#include "workerintegration.h"

#include <KColorScheme>
#include <KContextualHelpButton>
#include <KIO/JobUiDelegateFactory>
#include <KIO/SimpleJob>
#include <KLocalizedString>
#include <KMessageWidget>

#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>
#include <QWidgetAction>

using namespace Admin;

namespace
{
QPointer<KIO::SimpleJob> waitingForExpirationOfAuthorization;
}

Bar::Bar(DolphinViewContainer *parentViewContainer)
    : AnimatedHeightWidget{parentViewContainer}
    , m_parentViewContainer{parentViewContainer}
{
    setAutoFillBackground(true);
    updateColors();

    QWidget *contentsContainer = prepareContentsContainer();

    m_fullLabelString = i18nc("@info label above the view explaining the state", "Acting as an Administrator — Be careful!");
    m_shortLabelString = i18nc("@info label above the view explaining the state, keep short", "Acting as Admin");
    m_label = new QLabel(contentsContainer);
    m_label->setMinimumWidth(0);
    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByKeyboard); // for keyboard accessibility

    m_warningButton = new KContextualHelpButton(warningMessage(), nullptr, contentsContainer);
    m_warningButton->setIcon(QIcon::fromTheme(QStringLiteral("emblem-warning")));

    m_closeButton = new QPushButton(QIcon::fromTheme(QStringLiteral("window-close-symbolic")),
                                    i18nc("@action:button Finish/Stop/Done acting as an admin", "Finish"),
                                    contentsContainer);
    m_closeButton->setToolTip(i18nc("@info:tooltip", "Finish acting as an administrator"));
    m_closeButton->setFlat(true);
    connect(m_closeButton, &QAbstractButton::clicked, this, [this]() {
        m_parentViewContainer->setActive(true); // Ensure view active before exiting admin mode
        QAction *actAsAdminAction = WorkerIntegration::FriendAccess::actAsAdminAction();
        if (actAsAdminAction->isChecked()) {
            actAsAdminAction->trigger();
        }
    });

    connect(m_parentViewContainer->view(), &DolphinView::urlChanged, this, [this](const QUrl &url) {
        // Hide bar immediately if admin protocol no longer used
        if (url.scheme() != QStringLiteral("admin")) {
            setVisible(false, WithAnimation);
        }
    });

    QHBoxLayout *layout = new QHBoxLayout(contentsContainer);
    auto contentsMargins = layout->contentsMargins();
    m_preferredHeight = contentsMargins.top() + m_label->sizeHint().height() + contentsMargins.bottom();
    m_warningButton->setFixedHeight(m_preferredHeight);
    m_closeButton->setFixedHeight(m_preferredHeight);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addStretch();
    layout->addWidget(m_label);
    layout->addWidget(m_warningButton);
    layout->addStretch();
    layout->addWidget(m_closeButton);
}

bool Bar::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::PaletteChange:
        updateColors();
        break;
    case QEvent::Show:
        hideTheNextTimeAuthorizationExpires();
        break;
    default:
        break;
    }
    return AnimatedHeightWidget::event(event);
}

void Bar::resizeEvent(QResizeEvent *resizeEvent)
{
    updateLabelString();
    QWidget::resizeEvent(resizeEvent);
}

void Bar::hideTheNextTimeAuthorizationExpires()
{
    if (waitingForExpirationOfAuthorization.isNull()) {
        QByteArray packedArgs;
        QDataStream stream(&packedArgs, QIODevice::WriteOnly);
        stream << (int)1;
        waitingForExpirationOfAuthorization = KIO::special(QUrl(QStringLiteral("admin:/")), packedArgs, KIO::HideProgressInfo);
        waitingForExpirationOfAuthorization->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoWarningHandlingEnabled, m_parentViewContainer));

        connect(waitingForExpirationOfAuthorization, &KJob::finished, this, [](KJob *job) {
            if (job->error()) {
                if (job->uiDelegate()) {
                    job->uiDelegate()->showErrorMessage();
                }
            }
        }, Qt::UniqueConnection);
    }

    connect(waitingForExpirationOfAuthorization, &KJob::finished, this, [this](KJob *job) {
        if (job->error()) {
            return;
        }
        QUrl viewContainerUrl = m_parentViewContainer->url();
        if (viewContainerUrl.scheme() != QStringLiteral("admin")) {
            return;
        }
        viewContainerUrl.setScheme("file");
        m_parentViewContainer->setUrl(viewContainerUrl);

        // Show message about expiration with re-enable action
        if (!m_reenableActAsAdminAction) {
            auto actAsAdminAction = WorkerIntegration::FriendAccess::actAsAdminAction();
            m_reenableActAsAdminAction =
                new QAction{actAsAdminAction->icon(), i18nc("@action:button shown after acting as admin ended", "Act as Administrator Again"), this};
            m_reenableActAsAdminAction->setToolTip(actAsAdminAction->toolTip());
            m_reenableActAsAdminAction->setWhatsThis(actAsAdminAction->whatsThis());
            connect(m_reenableActAsAdminAction, &QAction::triggered, this, [this, actAsAdminAction]() {
                m_parentViewContainer->setActive(true);
                actAsAdminAction->trigger();
            });
        }
        m_parentViewContainer->showMessage(i18nc("@info", "Administrator authorization has expired."),
                                           KMessageWidget::Information,
                                           {m_reenableActAsAdminAction});
    }, Qt::UniqueConnection);
}

void Bar::updateColors()
{
    // Use KColorScheme for consistent theming
    KColorScheme colorScheme(QPalette::Active, KColorScheme::NegativeBackground);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, colorScheme.background().color());
    pal.setColor(QPalette::WindowText, colorScheme.foreground().color());
    setPalette(pal);
}

void Bar::updateLabelString()
{
    QFontMetrics fontMetrics = m_label->fontMetrics();
    const int widthNeeded = fontMetrics.horizontalAdvance(m_fullLabelString) +
                            m_warningButton->sizeHint().width() +
                            m_closeButton->sizeHint().width() +
                            style()->pixelMetric(QStyle::PM_LayoutLeftMargin) * 2 +
                            style()->pixelMetric(QStyle::PM_LayoutRightMargin) * 2;

    if (widthNeeded < width()) {
        m_label->setText(m_fullLabelString);
    } else {
        m_label->setText(m_shortLabelString);
    }
}

#include "moc_bar.cpp"
