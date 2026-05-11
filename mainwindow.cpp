#include "mainwindow.h"
#include "./ui_mainwindow.h"

#ifdef Q_OS_WIN
#include "trainer_core/DemonStarTrainer.h"
#endif

#include <QCheckBox>
#include <QKeySequence>
#include <QShortcut>
#include <QSignalBlocker>
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
    ui->infinitePlanesCheckBox->setText(QStringLiteral("无限飞机 (Ctrl+1)"));
    ui->infiniteNukesCheckBox->setText(QStringLiteral("无限核弹 (Ctrl+2)"));
    ui->infiniteHealthCheckBox->setText(QStringLiteral("无限生命值 (Ctrl+3)"));

#ifdef Q_OS_WIN
    trainer = std::make_unique<demonstar::DemonStarTrainer>();
    trainer->setListener(this);
#endif

    connect(ui->infinitePlanesCheckBox, &QCheckBox::toggled, this, &MainWindow::applyInfinitePlanes);
    connect(ui->infiniteNukesCheckBox, &QCheckBox::toggled, this, &MainWindow::applyInfiniteNukes);
    connect(ui->infiniteHealthCheckBox, &QCheckBox::toggled, this, &MainWindow::applyInfiniteHealth);

    setGameAvailable(false);
    setupHotkeys();
    setupTrainerTimer();
    updateTrainer();
    statusBar()->showMessage(QStringLiteral("Ctrl+1 切换无限飞机，Ctrl+2 切换无限核弹，Ctrl+3 切换无限生命值"));
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

    if (!infinitePlanesHotkeyRegistered || !infiniteNukesHotkeyRegistered || !infiniteHealthHotkeyRegistered) {
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
    default:
        return false;
    }
}
#endif
