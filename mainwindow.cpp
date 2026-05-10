#include "mainwindow.h"
#include "./ui_mainwindow.h"

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
#include <tlhelp32.h>
#endif

namespace {
constexpr int TrainerPollIntervalMs = 100;

#ifdef Q_OS_WIN
constexpr int InfinitePlanesHotkeyId = 0x4401;
constexpr int InfiniteNukesHotkeyId = 0x4402;
constexpr quintptr InfinitePlanesOffset = 0x14C7954;
constexpr quintptr InfiniteNukesOffset = 0x14C80E4;
constexpr qint32 CheatValueLimit = 10;

bool executableNameEquals(const wchar_t *name, const QString &expected)
{
    return QString::fromWCharArray(name).compare(expected, Qt::CaseInsensitive) == 0;
}

DWORD findProcessId(const QString &processName)
{
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);

    DWORD processId = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (executableNameEquals(entry.szExeFile, processName)) {
                processId = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return processId;
}

quintptr findModuleBaseAddress(DWORD processId, const QString &moduleName)
{
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    MODULEENTRY32W entry = {};
    entry.dwSize = sizeof(entry);

    quintptr firstModuleBaseAddress = 0;
    quintptr moduleBaseAddress = 0;
    if (Module32FirstW(snapshot, &entry)) {
        do {
            const auto currentBaseAddress = reinterpret_cast<quintptr>(entry.modBaseAddr);
            if (firstModuleBaseAddress == 0) {
                firstModuleBaseAddress = currentBaseAddress;
            }

            if (executableNameEquals(entry.szModule, moduleName)) {
                moduleBaseAddress = currentBaseAddress;
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return moduleBaseAddress != 0 ? moduleBaseAddress : firstModuleBaseAddress;
}
#endif
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->infinitePlanesCheckBox, &QCheckBox::toggled, this, &MainWindow::applyInfinitePlanes);
    connect(ui->infiniteNukesCheckBox, &QCheckBox::toggled, this, &MainWindow::applyInfiniteNukes);

    setupHotkeys();
    setupTrainerTimer();
    statusBar()->showMessage(QStringLiteral("Ctrl+1 toggles Infinite Planes, Ctrl+2 toggles Infinite Nukes"));
}

MainWindow::~MainWindow()
{
#ifdef Q_OS_WIN
    closeGameProcess();
    unregisterGlobalHotkeys();
#endif
    delete ui;
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
    bool hasEnabledCheat = false;

    if (ui->infinitePlanesCheckBox->isChecked()) {
        hasEnabledCheat = true;
        updateInfinitePlanes();
    }

    if (ui->infiniteNukesCheckBox->isChecked()) {
        hasEnabledCheat = true;
        updateInfiniteNukes();
    }

    if (!hasEnabledCheat) {
        resetInfinitePlanesState();
        resetInfiniteNukesState();
        waitingForGameProcess = false;
        closeGameProcess();
    }
#endif
}

void MainWindow::toggleInfinitePlanes()
{
    ui->infinitePlanesCheckBox->toggle();
}

void MainWindow::toggleInfiniteNukes()
{
    ui->infiniteNukesCheckBox->toggle();
}

void MainWindow::applyInfinitePlanes(bool enabled)
{
#ifdef Q_OS_WIN
    resetInfinitePlanesState();
    waitingForGameProcess = false;

    if (enabled) {
        updateInfinitePlanes();
        return;
    }

    if (!ui->infiniteNukesCheckBox->isChecked()) {
        closeGameProcess();
    }
    statusBar()->showMessage(QStringLiteral("Infinite Planes disabled"), 2000);
#else
    if (enabled) {
        const QSignalBlocker blocker(ui->infinitePlanesCheckBox);
        ui->infinitePlanesCheckBox->setChecked(false);
    }
    statusBar()->showMessage(QStringLiteral("Memory editing is only implemented on Windows"), 2000);
#endif
}

void MainWindow::applyInfiniteNukes(bool enabled)
{
#ifdef Q_OS_WIN
    resetInfiniteNukesState();
    waitingForGameProcess = false;

    if (enabled) {
        updateInfiniteNukes();
        return;
    }

    if (!ui->infinitePlanesCheckBox->isChecked()) {
        closeGameProcess();
    }
    statusBar()->showMessage(QStringLiteral("Infinite Nukes disabled"), 2000);
#else
    if (enabled) {
        const QSignalBlocker blocker(ui->infiniteNukesCheckBox);
        ui->infiniteNukesCheckBox->setChecked(false);
    }
    statusBar()->showMessage(QStringLiteral("Memory editing is only implemented on Windows"), 2000);
#endif
}

#ifdef Q_OS_WIN
void MainWindow::updateInfinitePlanes()
{
    if (!ensureGameProcess()) {
        resetInfinitePlanesState();
        if (!waitingForGameProcess) {
            waitingForGameProcess = true;
            statusBar()->showMessage(QStringLiteral("Waiting for demonstar.exe"));
        }
        return;
    }

    qint32 currentValue = 0;
    if (!readInfinitePlanesValue(&currentValue)) {
        resetInfinitePlanesState();
        closeGameProcess();
        waitingForGameProcess = false;
        statusBar()->showMessage(QStringLiteral("Failed to read Infinite Planes value"), 2000);
        return;
    }

    if (waitingForGameProcess) {
        waitingForGameProcess = false;
        statusBar()->showMessage(QStringLiteral("Attached to demonstar.exe"), 2000);
    }

    if (currentValue > CheatValueLimit) {
        hasInfinitePlanesLockedValue = false;
        if (!infinitePlanesPausedAboveLimit) {
            infinitePlanesPausedAboveLimit = true;
            statusBar()->showMessage(QStringLiteral("Infinite Planes paused above 10"), 2000);
        }
        return;
    }

    if (!hasInfinitePlanesLockedValue || infinitePlanesPausedAboveLimit) {
        infinitePlanesLockedValue = currentValue;
        hasInfinitePlanesLockedValue = true;
        infinitePlanesPausedAboveLimit = false;
        statusBar()->showMessage(QStringLiteral("Infinite Planes locked"), 2000);
        return;
    }

    if (currentValue < infinitePlanesLockedValue) {
        if (!writeInfinitePlanesValue(infinitePlanesLockedValue)) {
            resetInfinitePlanesState();
            closeGameProcess();
            statusBar()->showMessage(QStringLiteral("Failed to write Infinite Planes value"), 2000);
        }
        return;
    }

    if (currentValue > infinitePlanesLockedValue) {
        infinitePlanesLockedValue = currentValue;
    }
}

void MainWindow::updateInfiniteNukes()
{
    if (!ensureGameProcess()) {
        resetInfiniteNukesState();
        if (!waitingForGameProcess) {
            waitingForGameProcess = true;
            statusBar()->showMessage(QStringLiteral("Waiting for demonstar.exe"));
        }
        return;
    }

    qint32 currentValue = 0;
    if (!readInfiniteNukesValue(&currentValue)) {
        resetInfiniteNukesState();
        closeGameProcess();
        waitingForGameProcess = false;
        statusBar()->showMessage(QStringLiteral("Failed to read Infinite Nukes value"), 2000);
        return;
    }

    if (waitingForGameProcess) {
        waitingForGameProcess = false;
        statusBar()->showMessage(QStringLiteral("Attached to demonstar.exe"), 2000);
    }

    if (currentValue > CheatValueLimit) {
        hasInfiniteNukesLockedValue = false;
        if (!infiniteNukesPausedAboveLimit) {
            infiniteNukesPausedAboveLimit = true;
            statusBar()->showMessage(QStringLiteral("Infinite Nukes paused above 10"), 2000);
        }
        return;
    }

    if (!hasInfiniteNukesLockedValue || infiniteNukesPausedAboveLimit) {
        infiniteNukesLockedValue = currentValue;
        hasInfiniteNukesLockedValue = true;
        infiniteNukesPausedAboveLimit = false;
        statusBar()->showMessage(QStringLiteral("Infinite Nukes locked"), 2000);
        return;
    }

    if (currentValue < infiniteNukesLockedValue) {
        if (!writeInfiniteNukesValue(infiniteNukesLockedValue)) {
            resetInfiniteNukesState();
            closeGameProcess();
            statusBar()->showMessage(QStringLiteral("Failed to write Infinite Nukes value"), 2000);
        }
        return;
    }

    if (currentValue > infiniteNukesLockedValue) {
        infiniteNukesLockedValue = currentValue;
    }
}

void MainWindow::resetInfinitePlanesState()
{
    infinitePlanesLockedValue = 0;
    hasInfinitePlanesLockedValue = false;
    infinitePlanesPausedAboveLimit = false;
}

void MainWindow::resetInfiniteNukesState()
{
    infiniteNukesLockedValue = 0;
    hasInfiniteNukesLockedValue = false;
    infiniteNukesPausedAboveLimit = false;
}

bool MainWindow::ensureGameProcess()
{
    if (gameProcessHandle != nullptr) {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(static_cast<HANDLE>(gameProcessHandle), &exitCode) && exitCode == STILL_ACTIVE) {
            return true;
        }

        closeGameProcess();
    }

    const QString processName = QStringLiteral("demonstar.exe");
    const DWORD processId = findProcessId(processName);
    if (processId == 0) {
        return false;
    }

    const HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE,
                                             FALSE,
                                             processId);
    if (processHandle == nullptr) {
        return false;
    }

    const quintptr baseAddress = findModuleBaseAddress(processId, processName);
    if (baseAddress == 0) {
        CloseHandle(processHandle);
        return false;
    }

    gameProcessHandle = processHandle;
    gameProcessId = processId;
    gameBaseAddress = baseAddress;
    return true;
}

void MainWindow::closeGameProcess()
{
    if (gameProcessHandle != nullptr) {
        CloseHandle(static_cast<HANDLE>(gameProcessHandle));
        gameProcessHandle = nullptr;
    }

    gameProcessId = 0;
    gameBaseAddress = 0;
}

bool MainWindow::readInfinitePlanesValue(qint32 *value) const
{
    if (gameProcessHandle == nullptr || gameBaseAddress == 0 || value == nullptr) {
        return false;
    }

    SIZE_T bytesRead = 0;
    const auto address = reinterpret_cast<LPCVOID>(gameBaseAddress + InfinitePlanesOffset);
    return ReadProcessMemory(static_cast<HANDLE>(gameProcessHandle), address, value, sizeof(*value), &bytesRead)
           && bytesRead == sizeof(*value);
}

bool MainWindow::writeInfinitePlanesValue(qint32 value) const
{
    if (gameProcessHandle == nullptr || gameBaseAddress == 0) {
        return false;
    }

    SIZE_T bytesWritten = 0;
    const auto address = reinterpret_cast<LPVOID>(gameBaseAddress + InfinitePlanesOffset);
    return WriteProcessMemory(static_cast<HANDLE>(gameProcessHandle), address, &value, sizeof(value), &bytesWritten)
           && bytesWritten == sizeof(value);
}

bool MainWindow::readInfiniteNukesValue(qint32 *value) const
{
    if (gameProcessHandle == nullptr || gameBaseAddress == 0 || value == nullptr) {
        return false;
    }

    SIZE_T bytesRead = 0;
    const auto address = reinterpret_cast<LPCVOID>(gameBaseAddress + InfiniteNukesOffset);
    return ReadProcessMemory(static_cast<HANDLE>(gameProcessHandle), address, value, sizeof(*value), &bytesRead)
           && bytesRead == sizeof(*value);
}

bool MainWindow::writeInfiniteNukesValue(qint32 value) const
{
    if (gameProcessHandle == nullptr || gameBaseAddress == 0) {
        return false;
    }

    SIZE_T bytesWritten = 0;
    const auto address = reinterpret_cast<LPVOID>(gameBaseAddress + InfiniteNukesOffset);
    return WriteProcessMemory(static_cast<HANDLE>(gameProcessHandle), address, &value, sizeof(value), &bytesWritten)
           && bytesWritten == sizeof(value);
}

bool MainWindow::registerGlobalHotkeys()
{
    const auto hwnd = reinterpret_cast<HWND>(winId());

    infinitePlanesHotkeyRegistered = RegisterHotKey(hwnd, InfinitePlanesHotkeyId, MOD_CONTROL, '1') != 0;
    infiniteNukesHotkeyRegistered = RegisterHotKey(hwnd, InfiniteNukesHotkeyId, MOD_CONTROL, '2') != 0;

    if (!infinitePlanesHotkeyRegistered || !infiniteNukesHotkeyRegistered) {
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
    default:
        return false;
    }
}
#endif
