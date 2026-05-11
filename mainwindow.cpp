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
constexpr int InfinitePlanesHotkeyId = 0x4401;
constexpr int InfiniteNukesHotkeyId = 0x4402;
constexpr int InfiniteHealthHotkeyId = 0x4403;
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

    connect(ui->infinitePlanesCheckBox, &QCheckBox::toggled, this, &MainWindow::applyInfinitePlanes);
    connect(ui->infiniteNukesCheckBox, &QCheckBox::toggled, this, &MainWindow::applyInfiniteNukes);
    connect(ui->infiniteHealthCheckBox, &QCheckBox::toggled, this, &MainWindow::applyInfiniteHealth);
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

    if (event.cheat != demonstar::CheatId::None) {
        syncCheatCheckbox(event.cheat, event.cheatEnabled);
    } else {
        syncAllCheatCheckboxes();
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
    connect(planesShortcut, &QShortcut::activated, this, &MainWindow::toggleInfinitePlanes);

    auto *nukesShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+2")), this);
    nukesShortcut->setContext(Qt::ApplicationShortcut);
    connect(nukesShortcut, &QShortcut::activated, this, &MainWindow::toggleInfiniteNukes);

    auto *healthShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+3")), this);
    healthShortcut->setContext(Qt::ApplicationShortcut);
    connect(healthShortcut, &QShortcut::activated, this, &MainWindow::toggleInfiniteHealth);

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
        syncAllCheatCheckboxes();
    }
#else
    setGameAvailable(false);
    syncAllCheatCheckboxes();
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

    trainer->addCheatValue(demonstar::CheatId::InfinitePlanes, ui->planesAmountSpinBox->value());
    syncCheatCheckbox(demonstar::CheatId::InfinitePlanes,
                      trainer->isCheatEnabled(demonstar::CheatId::InfinitePlanes));
}

void MainWindow::increaseNukes()
{
    if (!trainer || !trainer->isGameAvailable()) {
        statusBar()->showMessage(QStringLiteral("请先启动 demonstar.exe"), 2000);
        return;
    }

    trainer->addCheatValue(demonstar::CheatId::InfiniteNukes, ui->nukesAmountSpinBox->value());
    syncCheatCheckbox(demonstar::CheatId::InfiniteNukes,
                      trainer->isCheatEnabled(demonstar::CheatId::InfiniteNukes));
}

void MainWindow::increaseHealth()
{
    if (!trainer || !trainer->isGameAvailable()) {
        statusBar()->showMessage(QStringLiteral("请先启动 demonstar.exe"), 2000);
        return;
    }

    trainer->addCheatValue(demonstar::CheatId::InfiniteHealth, ui->healthAmountSpinBox->value());
    syncCheatCheckbox(demonstar::CheatId::InfiniteHealth,
                      trainer->isCheatEnabled(demonstar::CheatId::InfiniteHealth));
}

void MainWindow::syncCheatCheckbox(demonstar::CheatId cheat, bool enabled)
{
    QCheckBox *checkBox = nullptr;
    switch (cheat) {
    case demonstar::CheatId::InfinitePlanes:
        checkBox = ui->infinitePlanesCheckBox;
        break;
    case demonstar::CheatId::InfiniteNukes:
        checkBox = ui->infiniteNukesCheckBox;
        break;
    case demonstar::CheatId::InfiniteHealth:
        checkBox = ui->infiniteHealthCheckBox;
        break;
    case demonstar::CheatId::None:
        return;
    }

    const QSignalBlocker blocker(checkBox);
    checkBox->setChecked(enabled);
}

void MainWindow::syncAllCheatCheckboxes()
{
#ifdef Q_OS_WIN
    if (trainer) {
        syncCheatCheckbox(demonstar::CheatId::InfinitePlanes,
                          trainer->isCheatEnabled(demonstar::CheatId::InfinitePlanes));
        syncCheatCheckbox(demonstar::CheatId::InfiniteNukes,
                          trainer->isCheatEnabled(demonstar::CheatId::InfiniteNukes));
        syncCheatCheckbox(demonstar::CheatId::InfiniteHealth,
                          trainer->isCheatEnabled(demonstar::CheatId::InfiniteHealth));
        return;
    }
#endif

    syncCheatCheckbox(demonstar::CheatId::InfinitePlanes, false);
    syncCheatCheckbox(demonstar::CheatId::InfiniteNukes, false);
    syncCheatCheckbox(demonstar::CheatId::InfiniteHealth, false);
}

void MainWindow::toggleInfinitePlanes()
{
    if (!trainer || !trainer->isGameAvailable()) {
        statusBar()->showMessage(QStringLiteral("请先启动 demonstar.exe"), 2000);
        return;
    }

    ui->infinitePlanesCheckBox->toggle();
}

void MainWindow::toggleInfiniteNukes()
{
    if (!trainer || !trainer->isGameAvailable()) {
        statusBar()->showMessage(QStringLiteral("请先启动 demonstar.exe"), 2000);
        return;
    }

    ui->infiniteNukesCheckBox->toggle();
}

void MainWindow::toggleInfiniteHealth()
{
    if (!trainer || !trainer->isGameAvailable()) {
        statusBar()->showMessage(QStringLiteral("请先启动 demonstar.exe"), 2000);
        return;
    }

    ui->infiniteHealthCheckBox->toggle();
}

void MainWindow::applyInfinitePlanes(bool enabled)
{
#ifdef Q_OS_WIN
    if (trainer) {
        trainer->setCheatEnabled(demonstar::CheatId::InfinitePlanes, enabled);
        syncCheatCheckbox(demonstar::CheatId::InfinitePlanes,
                          trainer->isCheatEnabled(demonstar::CheatId::InfinitePlanes));
    }
#else
    Q_UNUSED(enabled)
    syncCheatCheckbox(demonstar::CheatId::InfinitePlanes, false);
    statusBar()->showMessage(QStringLiteral("内存修改功能仅支持 Windows"), 2000);
#endif
}

void MainWindow::applyInfiniteNukes(bool enabled)
{
#ifdef Q_OS_WIN
    if (trainer) {
        trainer->setCheatEnabled(demonstar::CheatId::InfiniteNukes, enabled);
        syncCheatCheckbox(demonstar::CheatId::InfiniteNukes,
                          trainer->isCheatEnabled(demonstar::CheatId::InfiniteNukes));
    }
#else
    Q_UNUSED(enabled)
    syncCheatCheckbox(demonstar::CheatId::InfiniteNukes, false);
    statusBar()->showMessage(QStringLiteral("内存修改功能仅支持 Windows"), 2000);
#endif
}

void MainWindow::applyInfiniteHealth(bool enabled)
{
#ifdef Q_OS_WIN
    if (trainer) {
        trainer->setCheatEnabled(demonstar::CheatId::InfiniteHealth, enabled);
        syncCheatCheckbox(demonstar::CheatId::InfiniteHealth,
                          trainer->isCheatEnabled(demonstar::CheatId::InfiniteHealth));
    }
#else
    Q_UNUSED(enabled)
    syncCheatCheckbox(demonstar::CheatId::InfiniteHealth, false);
    statusBar()->showMessage(QStringLiteral("内存修改功能仅支持 Windows"), 2000);
#endif
}

#ifdef Q_OS_WIN
bool MainWindow::registerGlobalHotkeys()
{
    const auto hwnd = reinterpret_cast<HWND>(winId());

    infinitePlanesHotkeyRegistered = RegisterHotKey(hwnd, InfinitePlanesHotkeyId, MOD_CONTROL, '1') != 0;
    infiniteNukesHotkeyRegistered = RegisterHotKey(hwnd, InfiniteNukesHotkeyId, MOD_CONTROL, '2') != 0;
    infiniteHealthHotkeyRegistered = RegisterHotKey(hwnd, InfiniteHealthHotkeyId, MOD_CONTROL, '3') != 0;
    const bool addPlanesRegistered = RegisterHotKey(hwnd, AddPlanesHotkeyId, MOD_CONTROL | MOD_SHIFT, '1') != 0;
    const bool addNukesRegistered = RegisterHotKey(hwnd, AddNukesHotkeyId, MOD_CONTROL | MOD_SHIFT, '2') != 0;
    const bool addHealthRegistered = RegisterHotKey(hwnd, AddHealthHotkeyId, MOD_CONTROL | MOD_SHIFT, '3') != 0;

    if (!infinitePlanesHotkeyRegistered || !infiniteNukesHotkeyRegistered || !infiniteHealthHotkeyRegistered
        || !addPlanesRegistered || !addNukesRegistered || !addHealthRegistered) {
        unregisterGlobalHotkeys();
        return false;
    }

    return true;
}

void MainWindow::unregisterGlobalHotkeys()
{
    const auto hwnd = reinterpret_cast<HWND>(winId());

    if (infinitePlanesHotkeyRegistered) {
        UnregisterHotKey(hwnd, InfinitePlanesHotkeyId);
        infinitePlanesHotkeyRegistered = false;
    }

    if (infiniteNukesHotkeyRegistered) {
        UnregisterHotKey(hwnd, InfiniteNukesHotkeyId);
        infiniteNukesHotkeyRegistered = false;
    }

    if (infiniteHealthHotkeyRegistered) {
        UnregisterHotKey(hwnd, InfiniteHealthHotkeyId);
        infiniteHealthHotkeyRegistered = false;
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
    case InfinitePlanesHotkeyId:
        toggleInfinitePlanes();
        if (result != nullptr) {
            *result = 0;
        }
        return true;
    case InfiniteNukesHotkeyId:
        toggleInfiniteNukes();
        if (result != nullptr) {
            *result = 0;
        }
        return true;
    case InfiniteHealthHotkeyId:
        toggleInfiniteHealth();
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
