#include "pos_ui.h"

#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
const QColor kNavy(0x0F, 0x17, 0x2A);
const QColor kBlue(0x25, 0x63, 0xEB);
const QColor kOrange(0xF9, 0x73, 0x16);

// Owner: Shoaib Ul Hassan - legacy authentication placeholder; active login uses AuthService.
bool authenticate(const QString &username, const QString &password, QString &error)
{
    Q_UNUSED(username);
    Q_UNUSED(password);
    error = QStringLiteral("Login is not connected to the user service yet.");
    return false;
}

// Barcode icon drawn with QPainter, so no image file is needed.
class BarcodeIcon : public QWidget
{
public:
    explicit BarcodeIcon(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(220, 220);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        painter.setPen(QPen(QColor(255, 255, 255, 140), 2));
        painter.setBrush(QColor(15, 23, 42, 170));
        painter.drawEllipse(QRectF(rect()).adjusted(4, 4, -4, -4));

        // Alternating bar / space widths (30 units in total).
        static const int widths[] = {3, 1, 2, 1, 3, 2, 1, 1, 3, 1, 2, 3, 1, 2, 1, 3};
        const int unit = 4;
        const int barcodeWidth = 30 * unit;
        const int barcodeHeight = 90;
        const int left = (width() - barcodeWidth) / 2;
        const int top = (height() - barcodeHeight) / 2;

        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::white);
        int x = left;
        for (int i = 0; i < 16; ++i) {
            const int w = widths[i] * unit;
            if (i % 2 == 0)
                painter.drawRect(x, top, w, barcodeHeight);
            x += w;
        }

        // Orange scan line across the barcode.
        painter.setPen(QPen(kOrange, 3, Qt::SolidLine, Qt::RoundCap));
        const int y = height() / 2;
        painter.drawLine(left - 16, y, left + barcodeWidth + 16, y);
    }
};

class LoginPage : public QWidget
{
public:
    LoginPage()
    {
        setWindowTitle(QStringLiteral("DukaanOS"));
        setMinimumSize(1000, 560);
        resize(1100, 680);
        m_background.load(QStringLiteral(":/images/market.jpg"));
        buildUi();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        if (!m_background.isNull()) {
            m_scaled = m_background.scaled(size(), Qt::KeepAspectRatioByExpanding,
                                           Qt::SmoothTransformation);
        }
        QWidget::resizeEvent(event);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), kNavy);

        // Market photo, cropped to cover the whole window.
        if (!m_scaled.isNull()) {
            painter.drawPixmap((width() - m_scaled.width()) / 2,
                               (height() - m_scaled.height()) / 2, m_scaled);
        }

        // Overlay only behind the left half (the login form).
        const QRect leftHalf(0, 0, width() / 2, height());
        QLinearGradient overlay(leftHalf.topLeft(), leftHalf.bottomRight());
        overlay.setColorAt(0.0, QColor(15, 23, 42, 235));
        overlay.setColorAt(1.0, QColor(37, 99, 235, 205));
        painter.fillRect(leftHalf, overlay);
    }

private:
    void buildUi()
    {
        auto *root = new QHBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        // Left half: login form.
        auto *leftSide = new QWidget(this);
        auto *leftLayout = new QVBoxLayout(leftSide);
        leftLayout->setContentsMargins(40, 40, 40, 40);
        leftLayout->addStretch();
        leftLayout->addWidget(buildCard(), 0, Qt::AlignHCenter);
        leftLayout->addStretch();

        // Right half: icon and app name.
        auto *rightSide = new QWidget(this);
        auto *rightLayout = new QVBoxLayout(rightSide);
        rightLayout->setContentsMargins(40, 40, 40, 40);
        rightLayout->addStretch();
        rightLayout->addWidget(new BarcodeIcon(rightSide), 0, Qt::AlignHCenter);
        rightLayout->addSpacing(24);

        auto *appName = new QLabel(QStringLiteral("DukaanOS"), rightSide);
        appName->setAlignment(Qt::AlignCenter);
        appName->setStyleSheet(QStringLiteral(
            "color: #FFFFFF; font-size: 44px; font-weight: 700;"));
        auto *nameShadow = new QGraphicsDropShadowEffect(appName);
        nameShadow->setBlurRadius(20);
        nameShadow->setOffset(0, 2);
        nameShadow->setColor(QColor(0, 0, 0, 160));
        appName->setGraphicsEffect(nameShadow);
        rightLayout->addWidget(appName);

        rightLayout->addStretch();

        root->addWidget(leftSide, 1);
        root->addWidget(rightSide, 1);
    }

    QFrame *buildCard()
    {
        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("card"));
        card->setFixedWidth(440);
        card->setStyleSheet(QStringLiteral(
            "#card { background: #FFFFFF; border-radius: 16px; }"
            "#title { color: #0F172A; font-size: 26px; font-weight: 700; }"
            "#subtitle { color: #475569; font-size: 13px; }"
            "#fieldLabel { color: #0F172A; font-size: 12px; font-weight: 600; }"
            "#error { color: #DC2626; font-size: 12px; }"
            "QLineEdit { background: #F8FAFC; color: #0F172A; border: 1px solid #CBD5E1;"
            " border-radius: 8px; padding: 10px 12px; font-size: 14px; }"
            "QLineEdit:focus { background: #FFFFFF; border: 1px solid #2563EB; }"
            "#loginButton { background: #2563EB; color: #FFFFFF; border: none;"
            " border-radius: 8px; padding: 12px; font-size: 14px; font-weight: 600; }"
            "#loginButton:hover { background: #1D4ED8; }"
            "#loginButton:pressed { background: #1E40AF; }"));

        auto *shadow = new QGraphicsDropShadowEffect(card);
        shadow->setBlurRadius(40);
        shadow->setOffset(0, 10);
        shadow->setColor(QColor(0, 0, 0, 90));
        card->setGraphicsEffect(shadow);

        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(36, 36, 36, 36);
        layout->setSpacing(6);

        auto *title = new QLabel(QStringLiteral("Welcome back"), card);
        title->setObjectName(QStringLiteral("title"));
        layout->addWidget(title);

        auto *subtitle = new QLabel(QStringLiteral("Sign in to your DukaanOS account"), card);
        subtitle->setObjectName(QStringLiteral("subtitle"));
        layout->addWidget(subtitle);
        layout->addSpacing(18);

        auto *usernameLabel = new QLabel(QStringLiteral("Username"), card);
        usernameLabel->setObjectName(QStringLiteral("fieldLabel"));
        layout->addWidget(usernameLabel);
        m_username = new QLineEdit(card);
        m_username->setPlaceholderText(QStringLiteral("Enter username"));
        layout->addWidget(m_username);
        layout->addSpacing(8);

        auto *passwordLabel = new QLabel(QStringLiteral("Password"), card);
        passwordLabel->setObjectName(QStringLiteral("fieldLabel"));
        layout->addWidget(passwordLabel);
        m_password = new QLineEdit(card);
        m_password->setEchoMode(QLineEdit::Password);
        m_password->setPlaceholderText(QStringLiteral("Enter password"));
        layout->addWidget(m_password);

        m_error = new QLabel(card);
        m_error->setObjectName(QStringLiteral("error"));
        m_error->setWordWrap(true);
        m_error->setMinimumHeight(30);
        layout->addWidget(m_error);

        auto *loginButton = new QPushButton(QStringLiteral("Login"), card);
        loginButton->setObjectName(QStringLiteral("loginButton"));
        loginButton->setCursor(Qt::PointingHandCursor);
        loginButton->setMinimumHeight(44);
        layout->addWidget(loginButton);

        connect(m_username, &QLineEdit::returnPressed, m_password,
                qOverload<>(&QWidget::setFocus));
        connect(m_password, &QLineEdit::returnPressed, this, [this]() { attemptLogin(); });
        connect(loginButton, &QPushButton::clicked, this, [this]() { attemptLogin(); });

        return card;
    }

    void attemptLogin()
    {
        const QString username = m_username->text().trimmed();
        const QString password = m_password->text();

        if (username.isEmpty() || password.isEmpty()) {
            m_error->setText(QStringLiteral("Enter your username and password."));
            return;
        }

        QString error;
        if (!authenticate(username, password, error)) {
            m_error->setText(error);
            return;
        }

        m_error->clear();
        // Owner: Shoaib Ul Hassan - legacy screen; active login opens the role dashboard.
    }

    QPixmap m_background;
    QPixmap m_scaled;
    QLineEdit *m_username = nullptr;
    QLineEdit *m_password = nullptr;
    QLabel *m_error = nullptr;
};
}

namespace PosUi
{
// Owner: Shoaib Ul Hassan - legacy login UI, retained outside the active app flow.
QWidget *createPosWindow()
{
    return new LoginPage;
}
}
