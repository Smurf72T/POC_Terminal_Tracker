// Диагностика сканера штрих-кода (keyboard-wedge).
// Даёт ответ на вопросы:
//  1) Шлёт ли сканер вообще клавиши (виден ли он как клавиатура)?
//  2) Какие символы, с какими задержками между ними?
//  3) Чем завершается скан (Enter / Tab / пауза / ничего)?
//  4) Что получится из сырых нажатий через BarcodeScanner + BarcodeParser?
//
// Запуск: сканируйте штрих-код, глядя на это окно. Всё пишется в консоль и в окно.

#include <QApplication>
#include <QKeyEvent>
#include <QDebug>
#include <QElapsedTimer>
#include <QEvent>
#include <QObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>

#include "utils/barcodescanner.h"
#include "utils/barcodeparser.h"
#include "utils/serialscanner.h"

#include <windows.h>
#include <QAbstractNativeEventFilter>
#include <QList>

// Глобальный низкоуровневый хук клавиатуры (WH_KEYBOARD_LL): видит нажатия
// ЛЮБОГО приложения Windows, не только нашего — независимо от фокуса.
class GlobalKeyboardHook : public QObject {
    Q_OBJECT

public:
    explicit GlobalKeyboardHook(QObject* parent = nullptr) : QObject(parent)
    {
        s_instance = this;
        m_hook = SetWindowsHookExW(WH_KEYBOARD_LL, &GlobalKeyboardHook::hookProc,
                                   GetModuleHandleW(nullptr), 0);
    }

    ~GlobalKeyboardHook() override
    {
        if (m_hook) {
            UnhookWindowsHookEx(m_hook);
            m_hook = nullptr;
        }
        s_instance = nullptr;
    }

    bool installed() const { return m_hook != nullptr; }

signals:
    void keyEvent(int vkCode, bool press, bool repeat, DWORD scanCode, DWORD flags);

private:
    static LRESULT CALLBACK hookProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode == HC_ACTION && s_instance) {
            auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
            const bool press = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
            const bool repeat = ((kb->flags & LLKHF_INJECTED) == 0) && (wParam == WM_KEYDOWN)
                                    ? (kb->flags & 0x10) != 0 // LLKHF_UP off, no auto-repeat bit in LL
                                    : false;
            // Для LLKHF_INJECTED важнее всего: приходит ли это от "инъекции".
            const bool injected = (kb->flags & LLKHF_INJECTED) != 0;
            if (wParam == WM_KEYDOWN || wParam == WM_KEYUP || wParam == WM_SYSKEYDOWN)
                emit s_instance->keyEvent(kb->vkCode, press, repeat, kb->scanCode, kb->flags);
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    HHOOK m_hook = nullptr;
    static GlobalKeyboardHook* s_instance;
};

GlobalKeyboardHook* GlobalKeyboardHook::s_instance = nullptr;

class ProbeWindow : public QWidget {
    Q_OBJECT

public:
    explicit ProbeWindow(QWidget* parent = nullptr) : QWidget(parent), m_last(new QElapsedTimer)
    {
        setWindowTitle("Сканер штрих-кода — диагностика");
        resize(760, 520);

        auto* layout = new QVBoxLayout(this);
        auto* hint = new QLabel(
            "Нажмите на это окно и просто отсканируйте штрих-код.\n"
            "Каждое нажатие клавиши будет записано с задержкой от предыдущего.\n"
            "Внизу — что собралось в цельный буфер и как его разобрал BarcodeParser.", this);
        hint->setWordWrap(true);
        layout->addWidget(hint);

        m_log = new QPlainTextEdit(this);
        m_log->setReadOnly(true);
        m_log->setFont(QFont("Consolas", 10));
        layout->addWidget(m_log, 1);

        auto* clearBtn = new QPushButton("Очистить", this);
        layout->addWidget(clearBtn);
        connect(clearBtn, &QPushButton::clicked, this, &ProbeWindow::clearLog);

        m_scan = new BarcodeScanner(this);
        m_scan->setInterCharTimeoutMs(50);
        connect(m_scan, &BarcodeScanner::scanFinished, this, &ProbeWindow::onScanFinished);

        qApp->installEventFilter(this);

        // COM-порт: пробуем основные скорости.
        m_serial = new SerialScanner(this);
        connect(m_serial, &SerialScanner::scanFinished, this, &ProbeWindow::onSerialScan);
        m_serialStarted = m_serial->start("COM8", 9600);
        if (m_serialStarted) {
            log("COM8 открыт (9600 8N1) — данные сканера будут здесь.");
        } else {
            log("COM8 не открылся. Проверьте, что порт свободен и имя верное.");
        }

        m_global = new GlobalKeyboardHook(this);
        connect(m_global, &GlobalKeyboardHook::keyEvent, this, &ProbeWindow::onGlobalKeyEvent);
        if (m_global->installed()) {
            log("Готово. Сканируйте код. ГЛОБАЛЬНЫЙ хук активен — вижу нажатия любого приложения.");
            log("Если сканер печатает в Блокнот/терминал — нажатия будут видны здесь тоже.");
        } else {
            log("ВНИМАНИЕ: глобальный хук не установлен (код ошибки ниже). Вижу только события своей программы.");
        }

        m_last->start();
    }

    ~ProbeWindow() override
    {
        qApp->removeEventFilter(this);
        delete m_last;
    }

private slots:
    void onSerialScan(const QString& raw)
    {
        log(QString("SERIAL (COM8): raw=%1").arg(printableRaw(raw)));
        const BarcodeScan data = BarcodeParser::parse(raw);
        if (data.hasData()) {
            log(QString("    serial='%1' imei1='%2' imei2='%3'")
                    .arg(data.serial, data.imei1, data.imei2));
        } else {
            log("    Parser: НЕТ данных.");
        }
    }

    void onGlobalKeyEvent(int vkCode, bool press, bool repeat, DWORD scanCode, DWORD flags)
    {
        QString name;
        switch (vkCode) {
            case VK_RETURN: name = "VK_RETURN"; break;
            case VK_TAB: name = "VK_TAB"; break;
            case VK_SPACE: name = "VK_SPACE"; break;
            case VK_ESCAPE: name = "VK_ESCAPE"; break;
            case VK_BACK: name = "VK_BACK"; break;
            case VK_CONTROL: name = "VK_CONTROL"; break;
            case VK_SHIFT: name = "VK_SHIFT"; break;
            case VK_MENU: name = "VK_MENU"; break;
            case VK_CAPITAL: name = "VK_CAPITAL"; break;
            case VK_OEM_1: name = "VK_OEM_1(;:)"; break;
            case VK_OEM_2: name = "VK_OEM_2(/?.)"; break;
            case VK_OEM_3: name = "VK_OEM_3(`~)"; break;
            case VK_OEM_4: name = "VK_OEM_4([{)"; break;
            case VK_OEM_5: name = "VK_OEM_5(\\|)"; break;
            case VK_OEM_6: name = "VK_OEM_6(])"; break;
            case VK_OEM_7: name = "VK_OEM_7('\" )"; break;
            case VK_OEM_MINUS: name = "VK_OEM_MINUS(-_)"; break;
            case VK_OEM_PLUS: name = "VK_OEM_PLUS(+=)"; break;
            case VK_OEM_PERIOD: name = "VK_OEM_PERIOD(>.)"; break;
            case VK_OEM_COMMA: name = "VK_OEM_COMMA(<,)"; break;
            case VK_OEM_102: name = "VK_OEM_102(\\|)"; break;
            case VK_NUMLOCK: name = "VK_NUMLOCK"; break;
            case VK_DECIMAL: name = "VK_DECIMAL"; break;
            case VK_ADD: name = "VK_ADD"; break;
            case VK_SUBTRACT: name = "VK_SUBTRACT"; break;
            case VK_MULTIPLY: name = "VK_MULTIPLY"; break;
            default: name = QString("VK_0x%1").arg(vkCode, 2, 16, QLatin1Char('0')); break;
        }
        // Для печатных ASCII-клавиш VK совпадает с кодом символа (VK_A=0x41, VK_0=0x30).
        if (vkCode >= 0x30 && vkCode <= 0x39)
            name = QString("'%1'").arg(QChar(vkCode));
        else if (vkCode >= 0x41 && vkCode <= 0x5A)
            name = QString("'%1'").arg(QChar(vkCode));

        QString type = press ? "DOWN" : "UP  ";
        QString inj = (flags & LLKHF_INJECTED) ? " [ИНЪЕКТ/сканирование]" : "";
        QString ext = (flags & LLKHF_EXTENDED) ? " [EXT]" : "";
        log(QString("GLOBAL: %1 %2 sc=0x%3 flags=0x%4 %5%6").arg(type).arg(name, -22).arg(scanCode, 3, 16, QLatin1Char('0')).arg(flags, 4, 16, QLatin1Char('0')).arg(inj).arg(ext));
    }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease)
            return QWidget::eventFilter(obj, event);

        auto* key = static_cast<QKeyEvent*>(event);

        qint64 delta = m_last->elapsed();
        m_last->restart();

        QString rep = key->isAutoRepeat() ? " [REPEAT]" : "";
        QString mods;
        Qt::KeyboardModifiers m = key->modifiers();
        if (m & Qt::ShiftModifier) mods += " Shift";
        if (m & Qt::ControlModifier) mods += " Ctrl";
        if (m & Qt::AltModifier) mods += " Alt";
        if (m & Qt::MetaModifier) mods += " Meta";
        if (m & Qt::KeypadModifier) mods += " Keypad";

        QString typeStr = (event->type() == QEvent::KeyPress) ? "press" : "release";

        // Печатная форма текста для невидимых клавиш.
        QString text;
        const QString t = key->text();
        for (QChar ch : t)
            text += QString("0x%1").arg(static_cast<int>(ch.unicode()), 2, 16, QLatin1Char('0')) + "'" + printable(ch) + "' ";

        // key() -> имя (например, Key_Return). Показываем и код.
        QString line = QString("%1 %2ms%3  key()=0x%4  text[]=%5  mods:%6")
                           .arg(typeStr, -7)
                           .arg(delta, 7)
                           .arg(rep.isEmpty() ? "" : rep)
                           .arg(key->key(), 0, 16)
                           .arg(text)
                           .arg(mods);
        log(line);

        if (event->type() == QEvent::KeyPress && !key->isAutoRepeat()) {
            const int k = key->key();
            if (k == Qt::Key_Return || k == Qt::Key_Enter || k == Qt::Key_Tab) {
                if (!m_scan->feedTerminator())
                    log("    -> терминатор (не буфер, терминатор пропущен)");
            } else {
                const QString feedText = key->text();
                bool any = false;
                for (QChar ch : feedText) {
                    if (!ch.isNull() && ch.unicode() <= 0xFF && !ch.isSpace()) {
                        any = true;
                        break;
                    }
                }
                if (any) {
                    m_scan->feed(feedText);
                    if (!m_scan->isActive())
                        log("    -> fed into scan buffer (буфер пуст: только пробелы/спецсимволы)");
                } else if (!feedText.isEmpty()) {
                    log("    -> не-ASCII/пробел: в буфер не добавляю");
                }
            }
        }
        // Не поглощаем — пусть сканер-клавиатура ведёт себя как обычно,
        // чтобы мы видели сырьё; терминатор Enter всё равно дойдёт до виджета.
        return QWidget::eventFilter(obj, event);
    }

private slots:
    void onScanFinished(const QString& raw)
    {
        log(QString("=== СКАН ЗАВЕРШЁН ==="));
        log(QString("    raw (%1 симв.): %2").arg(raw.size()).arg(printableRaw(raw)));
        const BarcodeScan data = BarcodeParser::parse(raw);
        if (data.hasData()) {
            log(QString("    serial='%1' imei1='%2' imei2='%3'")
                    .arg(data.serial, data.imei1, data.imei2));
        } else {
            log("    Parser: НЕТ данных (пусто/слишком коротко/не-ASCII).");
        }
    }

    void clearLog()
    {
        m_log->clear();
        m_last->restart();
    }

private:
    static QString printable(const QChar& ch)
    {
        if (ch.isPrint() && !ch.isSpace())
            return ch;
        if (ch == QLatin1Char(' '))
            return "SP";
        if (ch == QLatin1Char('\t'))
            return "TAB";
        return ".";
    }

    static QString printableRaw(const QString& s)
    {
        QString out;
        for (QChar ch : s) {
            if (ch.isPrint() && !ch.isSpace())
                out += ch;
            else if (ch == QLatin1Char(' '))
                out += "␣";
            else
                out += QString("<0x%1>").arg(static_cast<int>(ch.unicode()), 2, 16, QLatin1Char('0'));
        }
        return out;
    }

    void log(const QString& line)
    {
        qInfo().noquote() << line;
        m_log->appendPlainText(line);
    }

    QPlainTextEdit* m_log;
    BarcodeScanner* m_scan;
    SerialScanner* m_serial = nullptr;
    bool m_serialStarted = false;
    QElapsedTimer* m_last;
    GlobalKeyboardHook* m_global = nullptr;
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    ProbeWindow w;
    w.show();
    return app.exec();
}

#include "scanner_probe.moc"