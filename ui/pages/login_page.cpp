#include "login_page.h"

#include <QAction>
#include <QDebug>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QList>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
enum class Icon
{
    User,
    Lock,
    Eye,
    EyeOff,
    Login,
    Shield,
    Refresh
};

void drawUserShape(QPainter &p)
{
    p.drawEllipse(QPointF(12, 8), 4.2, 4.2);
    QPainterPath body;
    body.moveTo(4, 21);
    body.cubicTo(4, 15.5, 8, 14, 12, 14);
    body.cubicTo(16, 14, 20, 15.5, 20, 21);
    body.closeSubpath();
    p.drawPath(body);
}

// Small vector icons drawn with QPainter, so no extra image files are needed.
QPixmap drawIcon(Icon icon, const QColor &color, int size)
{
    const qreal ratio = 2.0;
    QPixmap pixmap(QSize(size, size) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 24.0, size / 24.0);

    const QPen stroke(color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(Qt::NoPen);
    p.setBrush(color);

    switch (icon)
    {
    case Icon::User:
        drawUserShape(p);
        break;
    case Icon::Lock:
        p.drawRoundedRect(QRectF(5, 11, 14, 10), 2.5, 2.5);
        p.setPen(stroke);
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(8, 3, 8, 10), 0, 180 * 16);
        p.drawLine(QPointF(8, 8), QPointF(8, 11));
        p.drawLine(QPointF(16, 8), QPointF(16, 11));
        break;
    case Icon::Eye:
    case Icon::EyeOff:
    {
        QPainterPath eye;
        eye.moveTo(2, 12);
        eye.cubicTo(6, 5, 18, 5, 22, 12);
        eye.cubicTo(18, 19, 6, 19, 2, 12);
        p.setPen(stroke);
        p.setBrush(Qt::NoBrush);
        p.drawPath(eye);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(QPointF(12, 12), 3, 3);
        if (icon == Icon::EyeOff)
        {
            p.setPen(stroke);
            p.drawLine(QPointF(4, 20), QPointF(20, 4));
        }
        break;
    }
    case Icon::Login:
        p.setPen(stroke);
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(3.5, 12), QPointF(14, 12));
        p.drawPolyline(QPolygonF() << QPointF(10, 7.5) << QPointF(14.5, 12) << QPointF(10, 16.5));
        p.drawPolyline(QPolygonF() << QPointF(14, 4) << QPointF(20, 4) << QPointF(20, 20) << QPointF(14, 20));
        break;
    case Icon::Shield:
    {
        QPainterPath shield;
        shield.moveTo(12, 2);
        shield.lineTo(20, 5.5);
        shield.lineTo(20, 12);
        shield.cubicTo(20, 17, 16.5, 20.5, 12, 22);
        shield.cubicTo(7.5, 20.5, 4, 17, 4, 12);
        shield.lineTo(4, 5.5);
        shield.closeSubpath();
        p.drawPath(shield);
        p.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(QPolygonF() << QPointF(8.5, 12) << QPointF(11, 14.5) << QPointF(15.5, 9.5));
        break;
    }
    case Icon::Refresh:
        p.setPen(stroke);
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(4, 4, 16, 16), 90 * 16, 270 * 16);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPolygon(QPolygonF() << QPointF(20, 6.5) << QPointF(16.2, 11.5) << QPointF(23.8, 11.5));
        break;
    }
    return pixmap;
}

struct CaptchaGlyph
{
    int offset;
    int angle;
};

struct CaptchaNoise
{
    double start;
    double end;
};

// Simple "plus" question drawn like a captcha: shifted, tilted characters and light noise lines.
class CaptchaWidget : public QWidget
{
public:
    explicit CaptchaWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedHeight(60);
        regenerate();
    }

    int answer() const { return answer_; }

    void regenerate()
    {
        QRandomGenerator *random = QRandomGenerator::global();
        const int first = random->bounded(2, 10);
        const int second = random->bounded(2, 10);
        answer_ = first + second;
        question_ = QStringLiteral("%1 + %2 = ?").arg(first).arg(second);

        glyphs_.clear();
        for (int i = 0; i < question_.size(); ++i)
            glyphs_.append(CaptchaGlyph{random->bounded(-5, 6), random->bounded(-10, 11)});
        noise_.clear();
        for (int i = 0; i < 3; ++i)
            noise_.append(CaptchaNoise{random->generateDouble(), random->generateDouble()});
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath shape;
        shape.addRoundedRect(box, 14, 14);

        QLinearGradient background(box.topLeft(), box.bottomRight());
        background.setColorAt(0.0, QColor(0xF1, 0xF5, 0xFF));
        background.setColorAt(1.0, QColor(0xE2, 0xEA, 0xFF));
        painter.fillPath(shape, background);

        painter.save();
        painter.setClipPath(shape);
        painter.setPen(QPen(QColor(21, 86, 232, 55), 1.4));
        for (const CaptchaNoise &noise : noise_)
        {
            QPainterPath line;
            line.moveTo(0, noise.start * height());
            line.cubicTo(width() * 0.3, noise.start * height() - 18,
                         width() * 0.65, noise.end * height() + 18,
                         width(), noise.end * height());
            painter.drawPath(line);
        }
        painter.restore();

        painter.setPen(QPen(QColor(0xCF, 0xD9, 0xF2), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(box, 14, 14);

        QFont font(QStringLiteral("Segoe UI"));
        font.setBold(true);
        font.setPixelSize(26);
        painter.setFont(font);
        const QFontMetrics metrics(font);
        const int spacing = 6;

        int totalWidth = -spacing;
        for (const QChar ch : question_)
            totalWidth += metrics.horizontalAdvance(ch) + spacing;

        qreal x = (width() - totalWidth) / 2.0;
        for (int i = 0; i < question_.size(); ++i)
        {
            const QChar ch = question_.at(i);
            const int advance = metrics.horizontalAdvance(ch);
            if (!ch.isSpace())
            {
                painter.save();
                painter.translate(x + advance / 2.0, height() / 2.0 + glyphs_.at(i).offset);
                painter.rotate(glyphs_.at(i).angle);
                painter.setPen(ch.isDigit() ? QColor(0x0B, 0x1F, 0x5C) : QColor(0x15, 0x56, 0xE8));
                painter.drawText(QPointF(-advance / 2.0, (metrics.ascent() - metrics.descent()) / 2.0),
                                 QString(ch));
                painter.restore();
            }
            x += advance + spacing;
        }
    }

private:
    int answer_ = 0;
    QString question_;
    QList<CaptchaGlyph> glyphs_;
    QList<CaptchaNoise> noise_;
};

class LoginPage : public QWidget
{
public:
    explicit LoginPage(QWidget *parent = nullptr) : QWidget(parent)
    {
        setWindowTitle(QStringLiteral("DukaanOS | Sign in"));
        setWindowIcon(QIcon(QStringLiteral(":/assets/app_icon.png")));
        setMinimumSize(1100, 700);
        resize(1280, 800);
        setFont(QFont(QStringLiteral("Segoe UI"), 10));

        background_.load(QStringLiteral(":/assets/market_background.jpg"));
        logo_.load(QStringLiteral(":/assets/dukaanos_logo.png"));
        if (background_.isNull())
            qWarning() << "Could not load login background image.";
        if (logo_.isNull())
            qWarning() << "Could not load DukaanOS logo.";

        // Left half: logo and text on the dark overlay. Right half: white login form.
        auto *pageLayout = new QHBoxLayout(this);
        pageLayout->setContentsMargins(0, 0, 0, 0);
        pageLayout->setSpacing(0);
        pageLayout->addWidget(createBrandPanel(), 1);
        pageLayout->addWidget(createFormPanel(), 1);
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        if (!background_.isNull())
        {
            scaledBackground_ = background_.scaled(
                size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        }
        if (!logo_.isNull())
        {
            const int logoWidth = qBound(300, width() / 2 - 96, 560);
            logoLabel_->setPixmap(logo_.scaled(
                QSize(logoWidth, 300), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(0x0B, 0x12, 0x26));

        // Market photo behind the left half.
        if (!scaledBackground_.isNull())
        {
            painter.drawPixmap((width() - scaledBackground_.width()) / 2,
                               (height() - scaledBackground_.height()) / 2,
                               scaledBackground_);
        }

        const int half = width() / 2;
        const QRect left(0, 0, half, height());
        const QRect right(half, 0, width() - half, height());

        // Dark blue overlay on the left half.
        QLinearGradient overlay(left.topLeft(), left.bottomRight());
        overlay.setColorAt(0.0, QColor(4, 14, 60, 242));
        overlay.setColorAt(1.0, QColor(8, 32, 110, 228));
        painter.fillRect(left, overlay);

        // White form side.
        painter.fillRect(right, Qt::white);
    }

private:
    QWidget *createBrandPanel()
    {
        auto *panel = new QWidget(this);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(48, 48, 48, 48);
        layout->setSpacing(0);
        layout->addStretch(1);

        logoLabel_ = new QLabel(panel);
        logoLabel_->setAlignment(Qt::AlignCenter);
        logoLabel_->setMinimumSize(1, 1);
        logoLabel_->setStyleSheet(QStringLiteral("background: transparent;"));
        layout->addWidget(logoLabel_);
        layout->addSpacing(20);

        auto *headline = new QLabel(QStringLiteral("Your store, in control."), panel);
        headline->setAlignment(Qt::AlignCenter);
        headline->setWordWrap(false);
        headline->setMaximumWidth(480);
        headline->setStyleSheet(QStringLiteral(
            "color: white; font-size: 24px; font-weight: 700; background: transparent;"));
        layout->addWidget(headline, 0, Qt::AlignHCenter);
        layout->addSpacing(9);

        auto *description = new QLabel(
            QStringLiteral("Smart tools for everyday retail."), panel);
        description->setAlignment(Qt::AlignCenter);
        description->setWordWrap(false);
        description->setMaximumWidth(480);
        description->setStyleSheet(QStringLiteral(
            "color: rgba(255,255,255,225); font-size: 14px; background: transparent;"));
        layout->addWidget(description, 0, Qt::AlignHCenter);
        layout->addStretch(1);
        return panel;
    }

    QWidget *createFormPanel()
    {
        auto *panel = new QWidget(this);
        auto *panelLayout = new QHBoxLayout(panel);
        panelLayout->setContentsMargins(40, 32, 40, 32);
        panelLayout->addStretch(1);

        // The form sits directly on the white half, without a separate card.
        auto *formWidget = new QWidget(panel);
        formWidget->setMinimumWidth(380);
        formWidget->setMaximumWidth(460);
        auto *form = new QVBoxLayout(formWidget);
        form->setContentsMargins(0, 0, 0, 0);
        form->setSpacing(0);

        // Optional round emblem: shown only if dukaanos_icon.png exists in the resources.
        const QPixmap emblem(QStringLiteral(":/assets/dukaanos_icon.png"));
        if (!emblem.isNull())
        {
            auto *emblemLabel = new QLabel(formWidget);
            emblemLabel->setAlignment(Qt::AlignCenter);
            emblemLabel->setPixmap(emblem.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            form->addWidget(emblemLabel);
            form->addSpacing(6);
        }

        auto *title = new QLabel(QStringLiteral("Welcome Back!"), formWidget);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet(QStringLiteral(
            "color: #0B1F5C; font-size: 32px; font-weight: 800;"));
        form->addWidget(title);
        form->addSpacing(6);

        auto *subtitle = new QLabel(QStringLiteral("Sign in to your DukaanOS account"), formWidget);
        subtitle->setAlignment(Qt::AlignCenter);
        subtitle->setStyleSheet(QStringLiteral("color: #5B6784; font-size: 14px;"));
        form->addWidget(subtitle);
        form->addSpacing(30);

        const QString fieldStyle = QStringLiteral(
            "QLineEdit { background: #F5F7FB; border: 1px solid #DCE3F1; border-radius: 14px;"
            " padding: 0 12px; color: #0F1B3D; font-size: 14px;"
            " selection-background-color: #1556E8; }"
            "QLineEdit:focus { background: white; border: 2px solid #1556E8; }");
        const QColor iconColor(0x1B, 0x3A, 0x8C);

        usernameEdit_ = new QLineEdit(formWidget);
        usernameEdit_->setPlaceholderText(QStringLiteral("Username"));
        usernameEdit_->setFixedHeight(52);
        usernameEdit_->setStyleSheet(fieldStyle);
        usernameEdit_->addAction(QIcon(drawIcon(Icon::User, iconColor, 20)), QLineEdit::LeadingPosition);
        form->addWidget(usernameEdit_);
        form->addSpacing(14);

        passwordEdit_ = new QLineEdit(formWidget);
        passwordEdit_->setPlaceholderText(QStringLiteral("Password"));
        passwordEdit_->setEchoMode(QLineEdit::Password);
        passwordEdit_->setFixedHeight(52);
        passwordEdit_->setStyleSheet(fieldStyle);
        passwordEdit_->addAction(QIcon(drawIcon(Icon::Lock, iconColor, 20)), QLineEdit::LeadingPosition);
        QAction *toggleEye = passwordEdit_->addAction(
            QIcon(drawIcon(Icon::EyeOff, iconColor, 20)), QLineEdit::TrailingPosition);
        connect(toggleEye, &QAction::triggered, this, [this, toggleEye, iconColor]() {
            const bool wasHidden = passwordEdit_->echoMode() == QLineEdit::Password;
            passwordEdit_->setEchoMode(wasHidden ? QLineEdit::Normal : QLineEdit::Password);
            toggleEye->setIcon(QIcon(drawIcon(wasHidden ? Icon::Eye : Icon::EyeOff, iconColor, 20)));
        });
        form->addWidget(passwordEdit_);
        form->addSpacing(22);

        // Verification: solve the simple question.
        auto *verifyLabel = new QLabel(QStringLiteral("Security check"), formWidget);
        verifyLabel->setStyleSheet(QStringLiteral(
            "color: #475569; font-size: 12px; font-weight: 700;"));
        form->addWidget(verifyLabel);
        form->addSpacing(8);

        auto *captchaRow = new QHBoxLayout;
        captchaRow->setSpacing(10);
        captcha_ = new CaptchaWidget(formWidget);
        captchaRow->addWidget(captcha_, 1);

        auto *refreshButton = new QPushButton(formWidget);
        refreshButton->setIcon(QIcon(drawIcon(Icon::Refresh, iconColor, 24)));
        refreshButton->setIconSize(QSize(24, 24));
        refreshButton->setToolTip(QStringLiteral("New question"));
        refreshButton->setCursor(Qt::PointingHandCursor);
        refreshButton->setFixedSize(60, 60);
        refreshButton->setStyleSheet(QStringLiteral(
            "QPushButton { background: #F5F7FB; border: 1px solid #CFD9F2; border-radius: 14px; }"
            "QPushButton:hover { background: #E8EEFF; }"));
        captchaRow->addWidget(refreshButton);
        form->addLayout(captchaRow);
        form->addSpacing(10);

        answerEdit_ = new QLineEdit(formWidget);
        answerEdit_->setPlaceholderText(QStringLiteral("Type the answer"));
        answerEdit_->setFixedHeight(52);
        answerEdit_->setMaxLength(3);
        answerEdit_->setStyleSheet(fieldStyle);
        answerEdit_->addAction(QIcon(drawIcon(Icon::Shield, iconColor, 20)), QLineEdit::LeadingPosition);
        form->addWidget(answerEdit_);
        form->addSpacing(8);

        notice_ = new QLabel(formWidget);
        notice_->setMinimumHeight(22);
        notice_->setWordWrap(true);
        notice_->setStyleSheet(QStringLiteral("color: #DC2626; font-size: 12px;"));
        form->addWidget(notice_);
        form->addSpacing(8);

        auto *loginButton = new QPushButton(QStringLiteral("  Login"), formWidget);
        loginButton->setIcon(QIcon(drawIcon(Icon::Login, Qt::white, 22)));
        loginButton->setIconSize(QSize(22, 22));
        loginButton->setCursor(Qt::PointingHandCursor);
        loginButton->setFixedHeight(54);
        loginButton->setStyleSheet(QStringLiteral(
            "QPushButton { background: #111184; color: white; border: none;"
            " border-radius: 14px; font-size: 15px; font-weight: 700; }"
            "QPushButton:hover { background: #0D0D6B; }"
            "QPushButton:pressed { background: #090957; }"));
        form->addWidget(loginButton);

        connect(refreshButton, &QPushButton::clicked, this, [this]() { newQuestion(); });
        connect(usernameEdit_, &QLineEdit::returnPressed, this, [this]() { passwordEdit_->setFocus(); });
        connect(passwordEdit_, &QLineEdit::returnPressed, this, [this]() { answerEdit_->setFocus(); });
        connect(answerEdit_, &QLineEdit::returnPressed, this, [this]() { attemptLogin(); });
        connect(loginButton, &QPushButton::clicked, this, [this]() { attemptLogin(); });

        panelLayout->addWidget(formWidget, 8, Qt::AlignVCenter);
        panelLayout->addStretch(1);
        return panel;
    }

    void showNotice(const QString &text, bool isError = true)
    {
        notice_->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
            .arg(isError ? QStringLiteral("#DC2626") : QStringLiteral("#1556E8")));
        notice_->setText(text);
    }

    void newQuestion()
    {
        captcha_->regenerate();
        answerEdit_->clear();
    }

    void attemptLogin()
    {
        if (usernameEdit_->text().trimmed().isEmpty() || passwordEdit_->text().isEmpty())
        {
            showNotice(QStringLiteral("Enter your username and password."));
            return;
        }

        bool isNumber = false;
        const int answer = answerEdit_->text().trimmed().toInt(&isNumber);
        if (!isNumber)
        {
            showNotice(QStringLiteral("Solve the security question to continue."));
            return;
        }
        if (answer != captcha_->answer())
        {
            showNotice(QStringLiteral("Wrong answer. Please try the new question."));
            newQuestion();
            return;
        }

        newQuestion();
        // TODO(Shoaib): call AuthService::login and open the Dashboard for the logged-in role.
        showNotice(QStringLiteral("Authentication is not connected yet."));
    }

    QPixmap background_;
    QPixmap scaledBackground_;
    QPixmap logo_;
    QLabel *logoLabel_ = nullptr;
    QLineEdit *usernameEdit_ = nullptr;
    QLineEdit *passwordEdit_ = nullptr;
    QLineEdit *answerEdit_ = nullptr;
    CaptchaWidget *captcha_ = nullptr;
    QLabel *notice_ = nullptr;
};
}

QWidget *UiPages::createLoginPage(QWidget *parent)
{
    return new LoginPage(parent);
}