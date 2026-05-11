#include "mainwindow.h"
#include "./ui_mainwindow.h"

#ifdef Q_OS_WIN
#include "trainer_core/DemonStarTrainer.h"
#endif

#include <QCheckBox>
#include <QKeySequence>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QString>
#include <QTimer>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
constexpr int TrainerPollIntervalMs = 100;

#ifdef Q_OS_WIN
constexpr int PlanesHotkeyId = 0x4401;
constexpr int NukesHotkeyId = 0x4402;
constexpr int HealthHotkeyId = 0x4403;
constexpr int AddPlanesHotkeyId = 0x4411;
constexpr int AddNukesHotkeyId = 0x4412;
constexpr int AddHealthHotkeyId = 0x4413;
#endif

QString toQString(const std::string &text)
{
    return QString::fromUtf8(text.c_str());
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->gameStatusGroupBox->setTitle(QStringLiteral("游戏状态"));
    ui->gameStatusTitleLabel->setText(QStringLiteral("当前状态："));
    ui->cheatsGroupBox->setTitle(QStringLiteral("修改功能"));
    ui->planesTitleLabel->setText(QStringLiteral("飞机数量"));
    ui->nukesTitleLabel->setText(QStringLiteral("核弹数量"));
    ui->healthTitleLabel->setText(QStringLiteral("生命值"));
    ui->planesAmountSpinBox->setValue(5);
    ui->nukesAmountSpinBox->setValue(5);
    ui->healthAmountSpinBox->setValue(160);
    ui->addPlanesButton->setText(QStringLiteral("增加 (Ctrl+Shift+1)"));
    ui->addNukesButton->setText(QStringLiteral("增加 (Ctrl+Shift+2)"));
    ui->addHealthButton->setText(QStringLiteral("增加 (Ctrl+Shift+3)"));
    ui->infinitePlanesCheckBox->setText(QStringLiteral("锁定 (Ctrl+1)"));
    ui->infiniteNukesCheckBox->setText(QStringLiteral("锁定 (Ctrl+2)"));
    ui->infiniteHealthCheckBox->setText(QStringLiteral("锁定 (Ctrl+3)"));

#ifdef Q_OS_WIN
    trainer = std::make_unique<demonstar::DemonStarTrainer>();
    trainer->setListener(this);
#endif

    connect(ui->infinitePlanesCheckBox, &QCheckBox::toggled, this, &MainWindow::applyPlanes);
    connect(ui->infiniteNukesCheckBox, &QCheckBox::toggled, this, &MainWindow::applyNukes);
    connect(ui->infiniteHealthCheckBox, &QCheckBox::toggled, this, &MainWindow::applyHealth);
    connect(ui->addPlanesButton, &QPushButton::clicked, this, &MainWindow::increasePlanes);
    connect(ui->addNukesButton, &QPushButton::clicked, this, &MainWindow::increaseNukes);
    connect(ui->addHealthButton, &QPushButton::clicked, this, &MainWindow::increaseHealth);

    setGameAvailable(false);
    setupHotkeys();
    setupTrainerTimer();
    updateTrainer();
    statusBar()->showMessage(QStringLiteral("Ctrl+1/2/3 切换锁定，Ctrl+Shift+1/2/3 增加数值"));
}

MainWindow::~MainWindow()
{
#ifdef Q_OS_WIN
    if (trainer) {
        trainer->shutdown();
    }
    unregisterGlobalHotkeys();
#endif
    delete ui;
}

void MainWindow::onTrainerEvent(const demonstar::TrainerEvent &event)
{
    setGameAvailable(event.gameAvailable);

    if (event.valueId != demonstar::TrainerValueId::None) {
        syncLockCheckbox(event.valueId, event.locked);
    } else {
        syncAllLockCheckboxes();
    }

    if (!event.message.empty()) {
        statusBar()->showMessage(toQString(event.message), 2000);
    }
}

void MainWindow::setupHotkeys()
{
#ifdef Q_OS_WIN
    if (registerGlobalHotkeys()) {
        return;
    }
#endif

    setupLocalHotkeys();
}

void MainWindow::setupLocalHotkeys()
{
    auto *planesShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+1")), this);
    planesShortcut->setContext(Qt::ApplicationShortcut);
    connect(planesShortcut, &QShortcut::activated, this, &MainWindow::togglePlanes);

    auto *nukesShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+2")), this);
    nukesShortcut->setContext(Qt::ApplicationShortcut);
    connect(nukesShortcut, &QShortcut::activated, this, &MainWindow::toggleNukes);

    auto *healthShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+3")), this);
    healthShortcut->setContext(Qt::ApplicationShortcut);
    connect(healthShortcut, &QShortcut::activated, this, &MainWindow::toggleHealth);

    auto *addPlanesShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+1")), this);
    addPlanesShortcut->setContext(Qt::ApplicationShortcut);
    connect(addPlanesShortcut, &QShortcut::activated, this, &MainWindow::increasePlanes);

    auto *addNukesShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+2")), this);
    addNukesShortcut->setContext(Qt::ApplicationShortcut);
    connect(addNukesShortcut, &QShortcut::activated, this, &MainWindow::increaseNukes);

    auto *addHealthShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+3")), this);
    addHealthShortcut->setContext(Qt::ApplicationShortcut);
    connect(addHealthShortcut, &QShortcut::activated, this, &MainWindow::increaseHealth);
}

void MainWindow::setupTrainerTimer()
{
    trainerTimer = new QTimer(this);
    trainerTimer->setInterval(TrainerPollIntervalMs);
    connect(trainerTimer, &QTimer::timeout, this, &MainWindow::updateTrainer);
    trainerTimer->start();
}

void MainWindow::updateTrainer()
{
#ifdef Q_OS_WIN
    if (trainer) {
        trainer->tick();
        setGameAvailable(trainer->isGameAvailable());
        syncAllLockCheckboxes();
    }
#else
    setGameAvailable(false);
    syncAllLockCheckboxes();
#endif
}

void MainWindow::setGameAvailable(bool available)
{
    ui->cheatsGroupBox->setEnabled(available);
    ui->gameStatusValueLabel->setText(available ? QStringLiteral("已启动，可以修改")
                                                : QStringLiteral("未启动，修改已禁用"));
    ui->gameStatusValueLabel->setStyleSheet(available ? QStringLiteral("color: #1f7a3f; font-weight: 600;")
                                                      : QStringLiteral("color: #a33; font-weight: 600;"));
}

void MainWindow::increasePlanes()
{
    if (!trainer || !trainer->isGameAvailable()) {
        statusBar()->showMessage(QStringLiteral("请先启动 demonstar.exe"), 2000);
        return;
    }

    trainer->addValue(demonstar::TrainerValueId::Planes, ui->planesAmountSpinBox->value());
    syncLockCheckbox(demonstar::TrainerValueId::Planes,
                      trainer->isValueLocked(demonstar::TrainerValueId::Planes));
}

void MainWindow::increaseNukes()
{
    if (!trainer || !trainer->isGameAvailable()) {
        statusBar()->showMessage(QStringLiteral("请先启动 demonstar.exe"), 2000);
        return;
    }

    trainer->addValue(demonstar::TrainerValueId::Nukes, ui->nukesAmountSpinBox->value());
    syncLockCheckbox(demonstar::TrainerValueId::Nukes,
                      trainer->isValueLocked(demonstar::TrainerValueId::Nukes));
}

void MainWindow::increaseHealth()
{
    if (!trainer || !trainer->isGameAvailable()) {
        statusBar()->showMessage(QStringLiteral("请先启动 demonstar.exe"), 2000);
        return;
    }

    trainer->addValue(demonstar::TrainerValueId::Health, ui->healthAmountSpinBox->value());
    syncLockCheckbox(demonstar::TrainerValueId::Health,
                      trainer->isValueLocked(demonstar::TrainerValueId::Health));
}

void MainWindow::syncLockCheckbox(demonstar::TrainerValueId valueId, bool enabled)
{
    QCheckBox *checkBox = nullptr;
    switch (valueId) {
    case demonstar::TrainerValueId::Planes:
        checkBox = ui->infinitePlanesCheckBox;
        break;
    case demonstar::TrainerValueId::Nukes:
        checkBox = ui->infiniteNukesCheckBox;
        break;
    case demonstar::TrainerValueId::Health:
        checkBox = ui->infiniteHealthCheckBox;
        break;
    case demonstar::TrainerValueId::None:
        return;
    }

    const QSignalBlocker blocker(checkBox);
    checkBox->setChecked(enabled);
}

void MainWindow::syncAllLockCheckboxes()
{
#ifdef Q_OS_WIN
    if (trainer) {
        syncLockCheckbox(demonstar::TrainerValueId::Planes,
                          trainer->isValueLocked(demonstar::TrainerValueId::Planes));
        syncLockCheckbox(demonstar::TrainerValueId::Nukes,
                          trainer->isValueLocked(demonstar::TrainerValueId::Nukes));
        syncLockCheckbox(demonstar::TrainerValueId::Health,
                          trainer->isValueLocked(demonstar::TrainerValueId::Health));
        return;
    }
#endif

    syncLockCheckbox(demonstar::TrainerValueId::Planes, false);
    syncLockCheckbox(demonstar::TrainerValueId::Nukes, false);
    syncLockCheckbox(demonstar::TrainerValueId::Health, false);
}

void MainWindow::togglePlanes()
{
    if (!trainer || !trainer->isGameAvailable()) {
        statusBar()->showMessage(QStringLiteral("请先启动 demonstar.exe"), 2000);
        return;
    }

    ui->infinitePlanesCheckBox->toggle();
}

void MainWindow::toggleNukes()
{
    if (!trainer || !trainer->isGameAvailable()) {
        statusBar()->showMessage(QStringLiteral("请先启动 demonstar.exe"), 2000);
        return;
    }

    ui->infiniteNukesCheckBox->toggle();
}

void MainWindow::toggleHealth()
{
    if (!trainer || !trainer->isGameAvailable()) {
        statusBar()->showMessage(QStringLiteral("请先启动 demonstar.exe"), 2000);
        return;
    }

    ui->infiniteHealthCheckBox->toggle();
}

void MainWindow::applyPlanes(bool enabled)
{
#ifdef Q_OS_WIN
    if (trainer) {
        trainer->setValueLocked(demonstar::TrainerValueId::Planes, enabled);
        syncLockCheckbox(demonstar::TrainerValueId::Planes,
                          trainer->isValueLocked(demonstar::TrainerValueId::Planes));
    }
#else
    Q_UNUSED(enabled)
    syncLockCheckbox(demonstar::TrainerValueId::Planes, false);
    statusBar()->showMessage(QStringLiteral("内存修改功能仅支持 Windows"), 2000);
#endif
}

void MainWindow::applyNukes(bool enabled)
{
#ifdef Q_OS_WIN
    if (trainer) {
        trainer->setValueLocked(demonstar::TrainerValueId::Nukes, enabled);
        syncLockCheckbox(demonstar::TrainerValueId::Nukes,
                          trainer->isValueLocked(demonstar::TrainerValueId::Nukes));
    }
#else
    Q_UNUSED(enabled)
    syncLockCheckbox(demonstar::TrainerValueId::Nukes, false);
    statusBar()->showMessage(QStringLiteral("内存修改功能仅支持 Windows"), 2000);
#endif
}

void MainWindow::applyHealth(bool enabled)
{
#ifdef Q_OS_WIN
    if (trainer) {
        trainer->setValueLocked(demonstar::TrainerValueId::Health, enabled);
        syncLockCheckbox(demonstar::TrainerValueId::Health,
                          trainer->isValueLocked(demonstar::TrainerValueId::Health));
    }
#else
    Q_UNUSED(enabled)
    syncLockCheckbox(demonstar::TrainerValueId::Health, false);
    statusBar()->showMessage(QStringLiteral("内存修改功能仅支持 Windows"), 2000);
#endif
}

#ifdef Q_OS_WIN
bool MainWindow::registerGlobalHotkeys()
{
    const auto hwnd = reinterpret_cast<HWND>(winId());

    PlanesHotkeyRegistered = RegisterHotKey(hwnd, PlanesHotkeyId, MOD_CONTROL, '1') != 0;
    NukesHotkeyRegistered = RegisterHotKey(hwnd, NukesHotkeyId, MOD_CONTROL, '2') != 0;
    HealthHotkeyRegistered = RegisterHotKey(hwnd, HealthHotkeyId, MOD_CONTROL, '3') != 0;
    const bool addPlanesRegistered = RegisterHotKey(hwnd, AddPlanesHotkeyId, MOD_CONTROL | MOD_SHIFT, '1') != 0;
    const bool addNukesRegistered = RegisterHotKey(hwnd, AddNukesHotkeyId, MOD_CONTROL | MOD_SHIFT, '2') != 0;
    const bool addHealthRegistered = RegisterHotKey(hwnd, AddHealthHotkeyId, MOD_CONTROL | MOD_SHIFT, '3') != 0;

    if (!PlanesHotkeyRegistered || !NukesHotkeyRegistered || !HealthHotkeyRegistered
        || !addPlanesRegistered || !addNukesRegistered || !addHealthRegistered) {
        unregisterGlobalHotkeys();
        return false;
    }

    return true;
}

void MainWindow::unregisterGlobalHotkeys()
{
    const auto hwnd = reinterpret_cast<HWND>(winId());

    if (PlanesHotkeyRegistered) {
        UnregisterHotKey(hwnd, PlanesHotkeyId);
        PlanesHotkeyRegistered = false;
    }

    if (NukesHotkeyRegistered) {
        UnregisterHotKey(hwnd, NukesHotkeyId);
        NukesHotkeyRegistered = false;
    }

    if (HealthHotkeyRegistered) {
        UnregisterHotKey(hwnd, HealthHotkeyId);
        HealthHotkeyRegistered = false;
    }

    UnregisterHotKey(hwnd, AddPlanesHotkeyId);
    UnregisterHotKey(hwnd, AddNukesHotkeyId);
    UnregisterHotKey(hwnd, AddHealthHotkeyId);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
#else
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
#endif
{
    Q_UNUSED(eventType)

    const auto *msg = static_cast<MSG *>(message);
    if (msg->message != WM_HOTKEY) {
        return false;
    }

    switch (static_cast<int>(msg->wParam)) {
    case PlanesHotkeyId:
        togglePlanes();
        if (result != nullptr) {
            *result = 0;
        }
        return true;
    case NukesHotkeyId:
        toggleNukes();
        if (result != nullptr) {
            *result = 0;
        }
        return true;
    case HealthHotkeyId:
        toggleHealth();
        if (result != nullptr) {
            *result = 0;
        }
        return true;
    case AddPlanesHotkeyId:
        increasePlanes();
        if (result != nullptr) {
            *result = 0;
        }
        return true;
    case AddNukesHotkeyId:
        increaseNukes();
        if (result != nullptr) {
            *result = 0;
        }
        return true;
    case AddHealthHotkeyId:
        increaseHealth();
        if (result != nullptr) {
            *result = 0;
        }
        return true;
    default:
        return false;
    }
}
#endif
