#include "AboutDialog.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScroller>
#include <QShowEvent>
#include <QStyle>
#include <QTextBrowser>
#include <QVBoxLayout>

// ================================================================================

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("About"));
    setModal(true);
#if defined(Q_OS_ANDROID)
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
#else
    resize(720, 560);
#endif

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

#if defined(Q_OS_ANDROID)
    auto* header = new QHBoxLayout();
    header->setSpacing(12);
    m_close = new QPushButton(style()->standardIcon(QStyle::SP_ArrowBack), tr("Back"), this);
    m_close->setToolTip(tr("Back"));
    m_close->setAccessibleName(tr("Back"));
    auto* title = new QLabel(tr("About"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    title->setFont(titleFont);
    header->addWidget(m_close);
    header->addWidget(title, 1);
    layout->addLayout(header);
    auto* headerRule = new QFrame(this);
    headerRule->setFrameShape(QFrame::HLine);
    headerRule->setFrameShadow(QFrame::Sunken);
    layout->addWidget(headerRule);
#else
    auto* buttons = new QDialogButtonBox(this);
    m_close = buttons->addButton(QDialogButtonBox::Close);
    m_close->setText(tr("Close"));
    m_close->setToolTip(tr("Close"));
    m_close->setAccessibleName(tr("Close"));
    m_close->setAutoDefault(false);
    m_close->setDefault(false);
#endif
    m_close->setObjectName(QStringLiteral("closeAbout"));
    m_close->setMinimumHeight(44);
    connect(m_close, &QPushButton::clicked, this, &QDialog::reject);

    m_text = new QTextBrowser(this);
    m_text->setObjectName(QStringLiteral("aboutText"));
    m_text->setOpenExternalLinks(true);
    m_text->setReadOnly(true);
#if defined(Q_OS_ANDROID)
    // A finger drag is a mouse drag on Android, and QTextBrowser treats that as
    // selection. Scrolling owns the drag; links still open on a tap.
    m_text->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    QScroller::grabGesture(m_text->viewport(), QScroller::LeftMouseButtonGesture);
#endif
    QFile readme(QStringLiteral(":/README.md"));
    if (readme.open(QIODevice::ReadOnly) == true) {
        m_text->setMarkdown(QString::fromUtf8(readme.readAll()));
    } else {
        m_text->setPlainText(tr("The README could not be loaded."));
    }
    layout->addWidget(m_text, 1);

#if !defined(Q_OS_ANDROID)
    auto* rule = new QFrame(this);
    rule->setFrameShape(QFrame::HLine);
    rule->setFrameShadow(QFrame::Sunken);
    layout->addWidget(rule);
    layout->addWidget(buttons);
#endif
}

// ================================================================================

void AboutDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
#if defined(Q_OS_ANDROID)
    if (m_fullScreenApplied == true) {
        return;
    }
    m_fullScreenApplied = true;
    if (QScreen* screen = this->screen()) {
        setGeometry(screen->geometry());
        connect(screen, &QScreen::geometryChanged, this, [this](const QRect& geometry) {
            setGeometry(geometry);
            setWindowState(windowState() | Qt::WindowFullScreen);
        });
    }
    setWindowState(windowState() | Qt::WindowFullScreen);
#else
    (void)m_fullScreenApplied;
#endif
}
