#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif

#include <QWidget>
#include <QApplication>
#include <QDebug>
#include <QtEndian>
#include <QUdpSocket>
#include <QTimer>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QDialog>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QSettings>
#include <QComboBox>
#include <QPainter>
#include <QSpinBox>
#include <QDoubleSpinBox>

#include <algorithm>
#include <cmath>

#define CPAD_BOUND          0x5d0
#define CPP_BOUND           0x7f

#define TOUCH_SCREEN_WIDTH  320
#define TOUCH_SCREEN_HEIGHT 240

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

double lx = 0.0, ly = 0.0;
double rx = 0.0, ry = 0.0;
u32 interfaceButtons = 0;
QString ipAddress;
int yAxisMultiplier = 1;
bool abInverse = false;
bool xyInverse = false;

bool touchScreenPressed;
QPoint touchScreenPosition;

QSettings settings("TuxSH", "InputRedirectionClient-Qt");

QString homeButtonString = "ButtonHome";
QString powerButtonString = "ButtonPower";
QString powerLongButtonString = "ButtonPowerLong";

QString touchButton1String = "ButtonT1";
QString touchButton2String = "ButtonT2";
int touchButton1X = settings.value("touchButton1X", 0).toInt(),
    touchButton1Y = settings.value("touchButton1Y", 0).toInt(),
    touchButton2X = settings.value("touchButton2X", 0).toInt(),
    touchButton2Y = settings.value("touchButton2Y", 0).toInt();

QStringList hidButtonsABString = {
    "ButtonA",
    "ButtonB",
};

QStringList hidButtonsMiddleString = {
    "ButtonSelect",
    "ButtonStart",
    "ButtonRight",
    "ButtonLeft",
    "ButtonUp",
    "ButtonDown",
    "ButtonR",
    "ButtonL",
};

QStringList hidButtonsXYString = {
    "ButtonX",
    "ButtonY",
};

QStringList irButtonsString = {
    "ButtonZR",
    "ButtonZL",
};

void sendFrame(QString button = "")
{
    u32 hidPad = 0xfff;
    if(!abInverse)
    {
        for(u32 i = 0; i < 2; i++)
        {
            if(button.contains(hidButtonsABString[i]))
                hidPad &= ~(1 << i);
        }
    }
    else
    {
        for(u32 i = 0; i < 2; i++)
        {
            if(button.contains(hidButtonsABString[1-i]))
                hidPad &= ~(1 << i);
        }
    }

    for(u32 i = 2; i < 10; i++)
    {
        if(button.contains(hidButtonsMiddleString[i-2]))
            hidPad &= ~(1 << i);
    }

    if(!xyInverse)
    {
        for(u32 i = 10; i < 12; i++)
        {
            if(button.contains(hidButtonsXYString[i-10]))
                hidPad &= ~(1 << i);
        }
    }
    else
    {
        for(u32 i = 10; i < 12; i++)
        {
            if(button.contains(hidButtonsXYString[1-(i-10)]))
                hidPad &= ~(1 << i);
        }
    }

    u32 irButtonsState = 0;
    for(u32 i = 0; i < 2; i++)
    {
            if(button.contains(irButtonsString[i]))
                irButtonsState |= 1 << (i + 1);
    }

    u32 touchScreenState = 0x2000000;
    u32 circlePadState = 0x7ff7ff;
    u32 cppState = 0x80800081;

    if(lx != 0.0 || ly != 0.0)
    {
        u32 x = (u32)(lx * CPAD_BOUND + 0x800);
        u32 y = (u32)(ly * CPAD_BOUND + 0x800);
        x = x >= 0xfff ? (lx < 0.0 ? 0x000 : 0xfff) : x;
        y = y >= 0xfff ? (ly < 0.0 ? 0x000 : 0xfff) : y;

        circlePadState = (y << 12) | x;
    }

    if(rx != 0.0 || ry != 0.0 || irButtonsState != 0)
    {
        // We have to rotate the c-stick position 45°. Thanks, Nintendo.
        u32 x = (u32)(M_SQRT1_2 * (rx + ry) * CPP_BOUND + 0x80);
        u32 y = (u32)(M_SQRT1_2 * (ry - rx) * CPP_BOUND + 0x80);
        x = x >= 0xff ? (rx < 0.0 ? 0x00 : 0xff) : x;
        y = y >= 0xff ? (ry < 0.0 ? 0x00 : 0xff) : y;

        cppState = (y << 24) | (x << 16) | (irButtonsState << 8) | 0x81;
    }

    if(touchScreenPressed)
    {
        u32 x = (u32)(0xfff * std::min(std::max(0, touchScreenPosition.x()), TOUCH_SCREEN_WIDTH)) / TOUCH_SCREEN_WIDTH;
        u32 y = (u32)(0xfff * std::min(std::max(0, touchScreenPosition.y()), TOUCH_SCREEN_HEIGHT)) / TOUCH_SCREEN_HEIGHT;
        touchScreenState = (1 << 24) | (y << 12) | x;
    }

    QByteArray ba(20, 0);
    qToLittleEndian(hidPad, (uchar *)ba.data());
    qToLittleEndian(touchScreenState, (uchar *)ba.data() + 4);
    qToLittleEndian(circlePadState, (uchar *)ba.data() + 8);
    qToLittleEndian(cppState, (uchar *)ba.data() + 12);
    qToLittleEndian(interfaceButtons, (uchar *)ba.data() + 16);
    QUdpSocket().writeDatagram(ba, QHostAddress(ipAddress), 4950);
}

struct TouchScreen : public QDialog {
    TouchScreen(QWidget *parent = nullptr) : QDialog(parent)
    {
        this->setFixedSize(TOUCH_SCREEN_WIDTH, TOUCH_SCREEN_HEIGHT);
        this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint);
        this->setWindowTitle(tr("InputRedirectionClient-Qt - Touch screen"));
    }

    void mousePressEvent(QMouseEvent *ev)
    {
        if(ev->button() == Qt::LeftButton)
        {
            touchScreenPressed = true;
            touchScreenPosition = ev->pos();
            sendFrame();
        }
    }

    void mouseMoveEvent(QMouseEvent *ev)
    {
        if(touchScreenPressed && (ev->buttons() & Qt::LeftButton))
        {
            touchScreenPosition = ev->pos();
            sendFrame();
        }
    }

    void mouseReleaseEvent(QMouseEvent *ev)
    {
        if(ev->button() == Qt::LeftButton)
        {
            touchScreenPressed = false;
            sendFrame();
        }
    }

    void closeEvent(QCloseEvent *ev)
    {
        touchScreenPressed = false;
        sendFrame();
        ev->accept();
    }

    void paintEvent(QPaintEvent *)
    {
        QPainter painter(this);

        if (touchScreenPressed)
        {
            QPen pen(QColor("#f00"));
            painter.setPen(pen);
            painter.drawEllipse(QPoint(settings.value("touchButton1X", 0).toInt(), settings.value("touchButton1Y", 0).toInt()), 3, 3);
        }
        if (touchScreenPressed)
        {
            QPen pen(QColor("#00f"));
            painter.setPen(pen);
            painter.drawEllipse(QPoint(settings.value("touchButton2X", 0).toInt(), settings.value("touchButton2Y", 0).toInt()), 3, 3);
        }
    }
};

struct FrameTimer : public QTimer {
    FrameTimer(QObject *parent = nullptr) : QTimer(parent)
    {
        connect(this, &QTimer::timeout, this,
                [](void)
        {
            sendFrame();
        });
    }
};

class Widget : public QWidget
{
private:
    QVBoxLayout *layout;
    QFormLayout *formLayout;
    QHBoxLayout *autoLayout;
    QLineEdit *addrLineEdit;
    QCheckBox *invertYCheckbox, *invertABCheckbox, *invertXYCheckbox;
    QPushButton *aButton, *homeButton, *powerButton, *longPowerButton, *remapConfigButton, *startAutoButton;
    TouchScreen *touchScreen;
    QSpinBox *timesSpinBox;
    QDoubleSpinBox *intervalDoubleSpinBox;

    bool autoA = false;
    QTimer *autoATimer;

    void autoATimerTimeout() {
        sendFrame(hidButtonsABString[0]);
        timesSpinBox->setValue(timesSpinBox->value() - 1);
        qDebug() << timesSpinBox->value();
        if (timesSpinBox->value() <= 0) {
            autoA = !autoA;
            autoATimer->stop();
            timesSpinBox->setValue(timesSpinBox->property("startvalue").toInt());
            startAutoButton->setText(autoA ? tr("&STOP A") : tr("&START A"));
            timesSpinBox->setDisabled(autoA);
            intervalDoubleSpinBox->setDisabled(autoA);
        }
    }

public:
    Widget(QWidget *parent = nullptr) : QWidget(parent)
    {
        layout = new QVBoxLayout(this);

        addrLineEdit = new QLineEdit(this);
        addrLineEdit->setClearButtonEnabled(true);

        invertYCheckbox = new QCheckBox(this);
        invertABCheckbox = new QCheckBox(this);
        invertXYCheckbox = new QCheckBox(this);
        formLayout = new QFormLayout;

        formLayout->addRow(tr("IP &address"), addrLineEdit);
        formLayout->addRow(tr("&Invert Y axis"), invertYCheckbox);
        formLayout->addRow(tr("Invert A<->&B"), invertABCheckbox);
        formLayout->addRow(tr("Invert X<->&Y"), invertXYCheckbox);
        remapConfigButton = new QPushButton(tr("BUTTON &CONFIG"), this);

        aButton = new QPushButton(tr("&A"), this);
        homeButton = new QPushButton(tr("&HOME"), this);
        powerButton = new QPushButton(tr("&POWER"), this);
        longPowerButton = new QPushButton(tr("POWER (&long)"), this);


        autoLayout = new QHBoxLayout(this);
        timesSpinBox = new QSpinBox(this);
        timesSpinBox->setMinimum(0);
        timesSpinBox->setMaximum(999999);
        timesSpinBox->setSingleStep(1);
        timesSpinBox->setValue(1);
        intervalDoubleSpinBox = new QDoubleSpinBox(this);
        intervalDoubleSpinBox->setDecimals(2);
        intervalDoubleSpinBox->setMinimum(0);
        intervalDoubleSpinBox->setMaximum(999999);
        intervalDoubleSpinBox->setSingleStep(1);
        intervalDoubleSpinBox->setValue(1);
        startAutoButton = new QPushButton(tr("&START A"), this);
        autoLayout->addWidget(timesSpinBox);
        autoLayout->addWidget(intervalDoubleSpinBox);
        autoLayout->addWidget(startAutoButton);

        layout->addLayout(formLayout);
        layout->addLayout(autoLayout);
        layout->addWidget(aButton);
        layout->addWidget(homeButton);
        layout->addWidget(powerButton);
        layout->addWidget(longPowerButton);
        layout->addWidget(remapConfigButton);

        autoATimer = new QTimer(this);
        connect(autoATimer, &QTimer::timeout, this,
                [this]()
        {
            autoATimerTimeout();
        });

        connect(startAutoButton, &QPushButton::released, this,
                [this]()
        {
            if (!autoA && timesSpinBox->value() <= 0) {
                return;
            }
            autoA = !autoA;
            startAutoButton->setText(autoA ? tr("&STOP A") : tr("&START A"));
            timesSpinBox->setDisabled(autoA);
            intervalDoubleSpinBox->setDisabled(autoA);

            if (autoA) {
                timesSpinBox->setProperty("startvalue", timesSpinBox->value());
                autoATimerTimeout();
            }

            if (autoA) {
                autoATimer->start(intervalDoubleSpinBox->value() * 1000);
            }
            else {
                autoATimer->stop();
                timesSpinBox->setValue(timesSpinBox->property("startvalue").toInt());
            }
        });

        connect(addrLineEdit, &QLineEdit::textChanged, this,
                [](const QString &text)
        {
            ipAddress = text;
            settings.setValue("ipAddress", text);
        });

        connect(invertYCheckbox, &QCheckBox::stateChanged, this,
                [](int state)
        {
            switch(state)
            {
                case Qt::Unchecked:
                    yAxisMultiplier = 1;
                    settings.setValue("invertY", false);
                    break;
                case Qt::Checked:
                    yAxisMultiplier = -1;
                    settings.setValue("invertY", true);
                    break;
                default: break;
            }
        });

        connect(invertABCheckbox, &QCheckBox::stateChanged, this,
                [](int state)
        {
            switch(state)
            {
                case Qt::Unchecked:
                    abInverse = false;
                    settings.setValue("invertAB", false);
                    break;
                case Qt::Checked:
                    abInverse = true;
                    settings.setValue("invertAB", true);
                    break;
                default: break;
            }
        });

        connect(invertXYCheckbox, &QCheckBox::stateChanged, this,
                [](int state)
        {
            switch(state)
            {
                case Qt::Unchecked:
                    xyInverse = false;
                    settings.setValue("invertXY", false);
                    break;
                case Qt::Checked:
                    xyInverse = true;
                    settings.setValue("invertXY", true);
                    break;
                default: break;
            }
        });

//        connect(aButton, &QPushButton::pressed, this,
//                [](void)
//        {
//           sendFrame(hidButtonsABString[0]);
//        });

        connect(aButton, &QPushButton::released, this,
                [](void)
        {
           sendFrame(hidButtonsABString[0]);
        });

        connect(homeButton, &QPushButton::pressed, this,
                [](void)
        {
           interfaceButtons |= 1;
           sendFrame(homeButtonString);
        });

        connect(homeButton, &QPushButton::released, this,
                [](void)
        {
           interfaceButtons &= ~1;
           sendFrame(homeButtonString);
        });

        connect(powerButton, &QPushButton::pressed, this,
                [](void)
        {
           interfaceButtons |= 2;
           sendFrame(powerButtonString);
        });

        connect(powerButton, &QPushButton::released, this,
                [](void)
        {
           interfaceButtons &= ~2;
           sendFrame(powerButtonString);
        });

        connect(longPowerButton, &QPushButton::pressed, this,
                [](void)
        {
           interfaceButtons |= 4;
           sendFrame(powerLongButtonString);
        });

        connect(longPowerButton, &QPushButton::released, this,
                [](void)
        {
           interfaceButtons &= ~4;
           sendFrame(powerLongButtonString);
        });

        touchScreen = new TouchScreen(nullptr);
        this->setWindowTitle(tr("InputRedirectionClient-Qt"));

        addrLineEdit->setText(settings.value("ipAddress", "").toString());
        invertYCheckbox->setChecked(settings.value("invertY", false).toBool());
        invertABCheckbox->setChecked(settings.value("invertAB", false).toBool());
        invertXYCheckbox->setChecked(settings.value("invertXY", false).toBool());
    }

    void show(void)
    {
        QWidget::show();
        touchScreen->show();
    }

    void closeEvent(QCloseEvent *ev)
    {
        touchScreen->close();
        ev->accept();
    }

    virtual ~Widget(void)
    {
        lx = ly = rx = ry = 0.0;
        interfaceButtons = 0;
        touchScreenPressed = false;
        sendFrame();
        delete touchScreen;
    }

};


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Widget w;
    FrameTimer t(&w);
    TouchScreen ts;
    t.start(50);
    w.show();

    return a.exec();
}
