#include "bill_page.h"

#include <QDateTime>
#include <QColor>
#include <QDialog>
#include <QFileDialog>
#include <QScrollArea>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPushButton>
#include <QPdfWriter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <utility>

namespace
{
const QString kNavy = QStringLiteral("#111184");
const QString kInk = QStringLiteral("#20233B");
const QString kMuted = QStringLiteral("#777B91");
const QString kLine = QStringLiteral("#E7E9F3");

QString money(double amount)
{
    return QStringLiteral("PKR %1").arg(amount, 0, 'f', 2);
}

bool exportBillPdfFile(const OrderPreview &order, const QString &path)
{
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(16, 16, 16, 16), QPageLayout::Millimeter);
    writer.setResolution(120);

    QPainter painter;
    if (!painter.begin(&writer))
        return false;

    const int pageWidth = writer.width();
    const int pageHeight = writer.height();
    const int left = 64;
    const int right = pageWidth - 64;
    const int bottom = pageHeight - 64;
    const int rowHeight = 34;
    int y = 0;
    int pageNumber = 0;

    auto drawText = [&painter](const QRect &rect, int alignment, const QString &text) {
        painter.drawText(rect, alignment, text);
    };
    auto drawPageHeader = [&]() {
        ++pageNumber;
        painter.fillRect(QRect(0, 0, pageWidth, pageHeight), Qt::white);
        painter.setPen(QColor(kNavy));
        QFont brandFont(QStringLiteral("Segoe UI"), 20, QFont::Bold);
        painter.setFont(brandFont);
        drawText(QRect(left, 42, pageWidth - 2 * left, 42), Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("DukaanOS"));

        painter.setPen(QColor(kMuted));
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 10));
        drawText(QRect(left, 84, pageWidth - 2 * left, 24), Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("SALES RECEIPT"));
        drawText(QRect(left, 110, pageWidth - 2 * left, 22), Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("Bill: %1    Order: %2").arg(order.id.isEmpty()
                ? QStringLiteral("PREVIEW") : order.id,
                order.orderId.isEmpty() ? QStringLiteral("-") : order.orderId));

        const QString date = order.createdAt.isEmpty()
            ? QDateTime::currentDateTime().toString(QStringLiteral("dd MMM yyyy, hh:mm AP"))
            : order.createdAt;
        drawText(QRect(left, 134, pageWidth - 2 * left, 22), Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("Date: %1").arg(date));
        drawText(QRect(left, 158, pageWidth - 2 * left, 22), Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("Customer: %1").arg(order.customerName.isEmpty()
                ? QStringLiteral("Walk-in customer") : order.customerName));
        QString paymentText = QStringLiteral("Payment: %1").arg(order.paymentMethod.isEmpty()
            ? QStringLiteral("Cash") : order.paymentMethod);
        if (!order.phone.isEmpty())
            paymentText += QStringLiteral("    Phone: %1").arg(order.phone);
        drawText(QRect(left, 182, pageWidth - 2 * left, 22), Qt::AlignLeft | Qt::AlignVCenter,
            paymentText);

        y = 228;
        painter.setPen(QPen(QColor(kLine), 1));
        painter.drawLine(left, y, right, y);
        y += 12;

        painter.fillRect(QRect(left, y, right - left, 30), QColor(QStringLiteral("#F2F3FA")));
        painter.setPen(QColor(kMuted));
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 9, QFont::DemiBold));
        drawText(QRect(left + 8, y, (right - left) * 0.42, 30),
            Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("ITEM"));
        drawText(QRect(left + (right - left) * 0.43, y, (right - left) * 0.18, 30),
            Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("UNIT PRICE"));
        drawText(QRect(left + (right - left) * 0.62, y, (right - left) * 0.10, 30),
            Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("QTY"));
        drawText(QRect(left + (right - left) * 0.73, y, (right - left) * 0.27 - 8, 30),
            Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("AMOUNT"));
        y += 30;
    };

    drawPageHeader();
    painter.setFont(QFont(QStringLiteral("Segoe UI"), 10));
    for (const OrderPreviewLine &line : order.lines)
    {
        if (y + rowHeight > bottom - 150)
        {
            painter.setPen(QColor(kMuted));
            painter.setFont(QFont(QStringLiteral("Segoe UI"), 8));
            drawText(QRect(left, bottom - 18, right - left, 16), Qt::AlignRight,
                QStringLiteral("Page %1").arg(pageNumber));
            writer.newPage();
            drawPageHeader();
            painter.setFont(QFont(QStringLiteral("Segoe UI"), 10));
        }

        QString name = line.product.name;
        name.replace(QLatin1Char('_'), QLatin1Char(' '));
        const double lineTotal = line.product.unitPrice * line.quantity;
        painter.setPen(QColor(kInk));
        drawText(QRect(left + 8, y, (right - left) * 0.42, rowHeight),
            Qt::AlignLeft | Qt::AlignVCenter, name);
        drawText(QRect(left + (right - left) * 0.43, y, (right - left) * 0.18, rowHeight),
            Qt::AlignRight | Qt::AlignVCenter, money(line.product.unitPrice));
        drawText(QRect(left + (right - left) * 0.62, y, (right - left) * 0.10, rowHeight),
            Qt::AlignRight | Qt::AlignVCenter, QString::number(line.quantity));
        drawText(QRect(left + (right - left) * 0.73, y, (right - left) * 0.27 - 8, rowHeight),
            Qt::AlignRight | Qt::AlignVCenter, money(lineTotal));
        y += rowHeight;
        painter.setPen(QPen(QColor(QStringLiteral("#F0F1F6")), 1));
        painter.drawLine(left, y, right, y);
    }

    if (y + 140 > bottom)
    {
        painter.setPen(QColor(kMuted));
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 8));
        drawText(QRect(left, bottom - 18, right - left, 16), Qt::AlignRight,
            QStringLiteral("Page %1").arg(pageNumber));
        writer.newPage();
        drawPageHeader();
    }

    y += 20;
    const int labelWidth = (right - left) / 2;
    auto drawTotal = [&](const QString &label, const QString &amount, bool emphasized) {
        painter.setPen(QColor(emphasized ? kNavy : kMuted));
        painter.setFont(QFont(QStringLiteral("Segoe UI"), emphasized ? 12 : 10,
            emphasized ? QFont::Bold : QFont::Normal));
        drawText(QRect(left, y, labelWidth, 26), Qt::AlignLeft | Qt::AlignVCenter, label);
        drawText(QRect(left + labelWidth, y, labelWidth, 26),
            Qt::AlignRight | Qt::AlignVCenter, amount);
        y += emphasized ? 34 : 27;
    };
    drawTotal(QStringLiteral("Subtotal"), money(order.subtotal), false);
    drawTotal(QStringLiteral("Tax"), money(order.tax), false);
    drawTotal(QStringLiteral("Discount"), QStringLiteral("- %1").arg(money(order.discount)), false);
    painter.setPen(QPen(QColor(kLine), 1));
    painter.drawLine(left, y, right, y);
    y += 7;
    drawTotal(QStringLiteral("TOTAL"), money(order.total), true);

    painter.setPen(QColor(kMuted));
    painter.setFont(QFont(QStringLiteral("Segoe UI"), 9));
    drawText(QRect(left, qMin(y + 16, bottom - 30), right - left, 24),
        Qt::AlignHCenter | Qt::AlignVCenter, QStringLiteral("Thank you for shopping with us!"));
    painter.end();
    return true;
}

QLabel *makeLabel(const QString &text, const QString &style, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setStyleSheet(style);
    return label;
}

QWidget *createBillContents(const OrderPreview &order, QWidget *parent)
{
    auto *receipt = new QFrame(parent);
    receipt->setObjectName(QStringLiteral("billReceipt"));
    receipt->setStyleSheet(QStringLiteral(
        "QFrame#billReceipt { background: white; border: 1px solid %1; border-radius: 16px; }")
        .arg(kLine));
    auto *layout = new QVBoxLayout(receipt);
    layout->setContentsMargins(28, 26, 28, 24);
    layout->setSpacing(0);

    auto *brandRow = new QHBoxLayout;
    brandRow->addWidget(makeLabel(QStringLiteral("DukaanOS"),
        QStringLiteral("color: %1; font-size: 19px; font-weight: 800; background: transparent;")
            .arg(kNavy), receipt));
    brandRow->addStretch(1);
    brandRow->addWidget(makeLabel(order.orderId.isEmpty() ? QStringLiteral("PAYMENT PREVIEW")
        : (order.status.isEmpty() ? QStringLiteral("paid") : order.status).toUpper(),
        QStringLiteral("color: #18764A; background: #E8F8F0; border-radius: 8px;"
                       " padding: 7px 10px; font-size: 9px; font-weight: 700;"), receipt));
    layout->addLayout(brandRow);
    layout->addSpacing(7);
    layout->addWidget(makeLabel(QStringLiteral("Sales receipt"),
        QStringLiteral("color: %1; font-size: 12px; background: transparent;").arg(kMuted),
        receipt));
    layout->addSpacing(20);

    auto *details = new QGridLayout;
    details->setHorizontalSpacing(32);
    details->setVerticalSpacing(12);
    const QString keyStyle = QStringLiteral(
        "color: %1; font-size: 9px; font-weight: 700; background: transparent;").arg(kMuted);
    const QString valueStyle = QStringLiteral(
        "color: %1; font-size: 11px; font-weight: 600; background: transparent;").arg(kInk);
    details->addWidget(makeLabel(QStringLiteral("BILL NUMBER"), keyStyle, receipt), 0, 0);
    details->addWidget(makeLabel(QStringLiteral("DATE & TIME"), keyStyle, receipt), 0, 1);
    details->addWidget(makeLabel(order.id.isEmpty() ? QStringLiteral("PREVIEW") : order.id,
        valueStyle, receipt), 1, 0);
    details->addWidget(makeLabel(order.createdAt.isEmpty()
            ? QDateTime::currentDateTime().toString(QStringLiteral("dd MMM yyyy, hh:mm AP"))
            : order.createdAt, valueStyle, receipt), 1, 1);
    details->addWidget(makeLabel(QStringLiteral("CUSTOMER"), keyStyle, receipt), 2, 0);
    details->addWidget(makeLabel(QStringLiteral("PAYMENT METHOD"), keyStyle, receipt), 2, 1);
    details->addWidget(makeLabel(order.customerName.isEmpty()
            ? QStringLiteral("Walk-in customer") : order.customerName.toHtmlEscaped(),
        valueStyle, receipt), 3, 0);
    details->addWidget(makeLabel(order.paymentMethod.isEmpty()
            ? QStringLiteral("Cash") : order.paymentMethod, valueStyle, receipt), 3, 1);
    if (!order.phone.isEmpty())
    {
        details->addWidget(makeLabel(QStringLiteral("PHONE"), keyStyle, receipt), 4, 0);
        details->addWidget(makeLabel(order.phone.toHtmlEscaped(), valueStyle, receipt), 5, 0);
    }
    if (!order.orderId.isEmpty())
    {
        details->addWidget(makeLabel(QStringLiteral("ORDER ID"), keyStyle, receipt), 4, 1);
        details->addWidget(makeLabel(order.orderId, valueStyle, receipt), 5, 1);
    }
    layout->addLayout(details);
    layout->addSpacing(22);

    auto *table = new QTableWidget(receipt);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({
        QStringLiteral("ITEM"), QStringLiteral("PRICE"), QStringLiteral("QTY"),
        QStringLiteral("AMOUNT")
    });
    table->setRowCount(order.lines.size());
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 4; ++column)
        table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setFixedHeight(36);
    table->setShowGrid(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setMinimumHeight(qMax(80, order.lines.size() * 38 + 36));
    table->setStyleSheet(QStringLiteral(
        "QTableWidget { background: white; border: none; color: %1; }"
        "QHeaderView::section { background: #F8F8FC; color: %2; border: none;"
        " border-bottom: 1px solid %3; padding: 0 8px; font-size: 9px; font-weight: 700; }")
        .arg(kInk, kMuted, kLine));
    for (int row = 0; row < order.lines.size(); ++row)
    {
        const OrderPreviewLine &line = order.lines.at(row);
        QString name = line.product.name;
        name.replace(QLatin1Char('_'), QLatin1Char(' '));
        const double amount = line.product.unitPrice * line.quantity;
        const QString values[] = {
            name, money(line.product.unitPrice), QString::number(line.quantity), money(amount)
        };
        for (int column = 0; column < 4; ++column)
        {
            auto *cell = new QTableWidgetItem(values[column]);
            cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
            table->setItem(row, column, cell);
        }
        table->setRowHeight(row, 38);
    }
    layout->addWidget(table);
    layout->addSpacing(18);

    auto *totals = new QGridLayout;
    totals->setHorizontalSpacing(20);
    totals->setVerticalSpacing(9);
    auto addTotal = [receipt, totals, &keyStyle, &valueStyle](int row,
                         const QString &label, const QString &value) {
        totals->addWidget(makeLabel(label, keyStyle, receipt), row, 0);
        auto *amount = makeLabel(value, valueStyle, receipt);
        amount->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        totals->addWidget(amount, row, 1);
    };
    addTotal(0, QStringLiteral("Subtotal"), money(order.subtotal));
    addTotal(1, QStringLiteral("Tax"), money(order.tax));
    addTotal(2, QStringLiteral("Discount"), QStringLiteral("- %1").arg(money(order.discount)));
    auto *divider = new QFrame(receipt);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral("color: %1;").arg(kLine));
    totals->addWidget(divider, 3, 0, 1, 2);
    auto *totalLabel = makeLabel(order.status.isEmpty() || order.status == QStringLiteral("paid")
            ? QStringLiteral("TOTAL PAID") : QStringLiteral("TOTAL"),
        QStringLiteral("color: %1; font-size: 11px; font-weight: 800; background: transparent;")
            .arg(kInk), receipt);
    auto *totalValue = makeLabel(money(order.total),
        QStringLiteral("color: %1; font-size: 17px; font-weight: 800; background: transparent;")
            .arg(kNavy), receipt);
    totalValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    totals->addWidget(totalLabel, 4, 0);
    totals->addWidget(totalValue, 4, 1);
    layout->addLayout(totals);
    layout->addSpacing(20);
    auto *footer = makeLabel(QStringLiteral("Thank you for shopping with us!"),
        QStringLiteral("color: %1; font-size: 11px; font-weight: 600;"
                       " background: transparent; padding-top: 12px;"
                       " border-top: 1px dashed %2;").arg(kMuted, kLine), receipt);
    footer->setAlignment(Qt::AlignCenter);
    layout->addWidget(footer);
    return receipt;
}

// Asks where to save, writes the PDF and reports the result.
void downloadBillPdf(const OrderPreview &order, QWidget *parent)
{
    QString fileName = order.id.isEmpty() ? QStringLiteral("DukaanOS-Bill")
                                          : QStringLiteral("DukaanOS-%1").arg(order.id);
    for (QChar &character : fileName)
    {
        if (!character.isLetterOrNumber() && character != QLatin1Char('-') &&
            character != QLatin1Char('_'))
            character = QLatin1Char('_');
    }
    const QString path = QFileDialog::getSaveFileName(parent,
        QStringLiteral("Download bill as PDF"), fileName + QStringLiteral(".pdf"),
        QStringLiteral("PDF files (*.pdf)"));
    if (path.isEmpty())
        return;

    QString pdfPath = path;
    if (!pdfPath.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
        pdfPath += QStringLiteral(".pdf");
    if (!UiPages::writeBillPdf(order, pdfPath))
    {
        QMessageBox::critical(parent, QStringLiteral("Download failed"),
            QStringLiteral("The bill PDF could not be written to:\n%1").arg(pdfPath));
        return;
    }
    QMessageBox::information(parent, QStringLiteral("Bill downloaded"),
        QStringLiteral("Your bill was saved as a PDF:\n%1").arg(pdfPath));
}

class BillPage : public QWidget
{
public:
    BillPage(const OrderPreview &order, std::function<void()> onNewOrder, QWidget *parent)
        : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 20);
        root->setSpacing(14);

        auto *actions = new QHBoxLayout;
        auto *caption = makeLabel(QStringLiteral("Your bill is ready to review"),
            QStringLiteral("color: %1; font-size: 12px; background: transparent;").arg(kMuted),
            this);
        actions->addWidget(caption);
        actions->addStretch(1);
        auto *newOrder = new QPushButton(QStringLiteral("New order"), this);
        newOrder->setCursor(Qt::PointingHandCursor);
        newOrder->setFixedHeight(38);
        newOrder->setStyleSheet(QStringLiteral(
            "QPushButton { background: white; color: %1; border: 1px solid %2;"
            " border-radius: 9px; padding: 0 15px; font-size: 11px; font-weight: 600; }"
            "QPushButton:hover { background: #F8F8FC; }").arg(kNavy, kLine));
        actions->addWidget(newOrder);
        auto *download = new QPushButton(QStringLiteral("Download PDF"), this);
        download->setCursor(Qt::PointingHandCursor);
        download->setFixedHeight(38);
        download->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: white; border: none; border-radius: 9px;"
            " padding: 0 16px; font-size: 11px; font-weight: 700; }"
            "QPushButton:hover { background: #0D0D6B; }").arg(kNavy));
        actions->addWidget(download);
        root->addLayout(actions);

        auto *receiptWrap = new QWidget(this);
        auto *receiptLayout = new QHBoxLayout(receiptWrap);
        receiptLayout->setContentsMargins(0, 0, 0, 0);
        receiptLayout->addStretch(1);
        receiptLayout->addWidget(createBillContents(order, receiptWrap), 1);
        receiptLayout->addStretch(1);
        root->addWidget(receiptWrap, 1);

        auto *note = makeLabel(QStringLiteral(
            "Download a print-ready A4 PDF copy of this bill."),
            QStringLiteral("color: %1; font-size: 10px; background: transparent;").arg(kMuted),
            this);
        note->setAlignment(Qt::AlignCenter);
        root->addWidget(note);

        connect(newOrder, &QPushButton::clicked, this, [onNewOrder]() {
            if (onNewOrder)
                onNewOrder();
        });
        connect(download, &QPushButton::clicked, this, [this, order]() {
            downloadBillPdf(order, this);
        });
    }
};
}

bool UiPages::writeBillPdf(const OrderPreview &order, const QString &path)
{
    return exportBillPdfFile(order, path);
}

void UiPages::showBillDialog(const OrderPreview &order, QWidget *parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(order.id.isEmpty() ? QStringLiteral("Bill")
                                             : QStringLiteral("Bill %1").arg(order.id));
    dialog.setStyleSheet(QStringLiteral("QDialog { background: #F6F7FB; }"));
    dialog.resize(560, 760);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 18, 18, 16);
    layout->setSpacing(12);

    auto *scroll = new QScrollArea(&dialog);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
    auto *holder = new QWidget;
    holder->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *holderLayout = new QVBoxLayout(holder);
    holderLayout->setContentsMargins(0, 0, 0, 0);
    holderLayout->addWidget(createBillContents(order, holder));
    scroll->setWidget(holder);
    layout->addWidget(scroll, 1);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto *download = new QPushButton(QStringLiteral("Download PDF"), &dialog);
    download->setCursor(Qt::PointingHandCursor);
    download->setFixedHeight(38);
    download->setStyleSheet(QStringLiteral(
        "QPushButton { background: white; color: %1; border: 1px solid %2;"
        " border-radius: 9px; padding: 0 16px; font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background: #F8F8FC; }").arg(kNavy, kLine));
    auto *close = new QPushButton(QStringLiteral("Close"), &dialog);
    close->setCursor(Qt::PointingHandCursor);
    close->setFixedHeight(38);
    close->setDefault(true);
    close->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: white; border: none; border-radius: 9px;"
        " padding: 0 22px; font-size: 11px; font-weight: 700; }"
        "QPushButton:hover { background: #0D0D6B; }").arg(kNavy));
    buttons->addWidget(download);
    buttons->addWidget(close);
    layout->addLayout(buttons);

    QObject::connect(download, &QPushButton::clicked, &dialog,
        [&dialog, order]() { downloadBillPdf(order, &dialog); });
    QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
}

QWidget *UiPages::createBillPage(const OrderPreview &order, QWidget *parent,
    std::function<void()> onNewOrder)
{
    return new BillPage(order, std::move(onNewOrder), parent);
}
