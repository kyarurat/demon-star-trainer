#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QByteArray>
#include <QMainWindow>
#include <QtGlobal>

QT_BEGIN_NAMESPACE
class QTimer;
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupHotkeys();
    void setupLocalHotkeys();
    void setupTrainerTimer();
    void updateTrainer();
    void setGameAvailable(bool available);
    void disableCheats();
    void toggleInfinitePlanes();
    void toggleInfiniteNukes();
    void toggleInfiniteHealth();
    void applyInfinitePlanes(bool enabled);
    void applyInfiniteNukes(bool enabled);
    void applyInfiniteHealth(bool enabled);

#ifdef Q_OS_WIN
    void updateInfinitePlanes();
    void updateInfiniteNukes();
    void updateInfiniteHealth();
    void resetInfinitePlanesState();
    void resetInfiniteNukesState();
    void resetInfiniteHealthState();
    bool ensureGameProcess();
    void closeGameProcess();
    bool readInfinitePlanesValue(qint32 *value) const;
    bool writeInfinitePlanesValue(qint32 value) const;
    bool readInfiniteNukesValue(qint32 *value) const;
    bool writeInfiniteNukesValue(qint32 value) const;
    bool readInfiniteHealthValue(qint32 *value) const;
    bool writeInfiniteHealthValue(qint32 value) const;

protected:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#else
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

private:
    bool registerGlobalHotkeys();
    void unregisterGlobalHotkeys();

    void *gameProcessHandle = nullptr;
    unsigned long gameProcessId = 0;
    quintptr gameBaseAddress = 0;
    qint32 infinitePlanesLockedValue = 0;
    qint32 infiniteNukesLockedValue = 0;
    qint32 infiniteHealthLockedValue = 0;
    bool hasInfinitePlanesLockedValue = false;
    bool hasInfiniteNukesLockedValue = false;
    bool hasInfiniteHealthLockedValue = false;
    bool infinitePlanesPausedAboveLimit = false;
    bool infiniteNukesPausedAboveLimit = false;

    bool infinitePlanesHotkeyRegistered = false;
    bool infiniteNukesHotkeyRegistered = false;
    bool infiniteHealthHotkeyRegistered = false;
#endif

    QTimer *trainerTimer = nullptr;
    bool gameAvailable = false;
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
