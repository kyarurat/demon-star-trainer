#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "trainer_core/ITrainer.h"

#include <QByteArray>
#include <QMainWindow>
#include <QtGlobal>

#include <memory>

QT_BEGIN_NAMESPACE
class QTimer;
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow, public demonstar::ITrainerListener
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void onTrainerEvent(const demonstar::TrainerEvent &event) override;

private:
    void setupHotkeys();
    void setupLocalHotkeys();
    void setupTrainerTimer();
    void updateTrainer();
    void setGameAvailable(bool available);
    void syncCheatCheckbox(demonstar::CheatId cheat, bool enabled);
    void syncAllCheatCheckboxes();
    void toggleInfinitePlanes();
    void toggleInfiniteNukes();
    void toggleInfiniteHealth();
    void increasePlanes();
    void increaseNukes();
    void increaseHealth();
    void applyInfinitePlanes(bool enabled);
    void applyInfiniteNukes(bool enabled);
    void applyInfiniteHealth(bool enabled);

#ifdef Q_OS_WIN
protected:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#else
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

private:
    bool registerGlobalHotkeys();
    void unregisterGlobalHotkeys();

    bool infinitePlanesHotkeyRegistered = false;
    bool infiniteNukesHotkeyRegistered = false;
    bool infiniteHealthHotkeyRegistered = false;
#endif

    QTimer *trainerTimer = nullptr;
    std::unique_ptr<demonstar::ITrainer> trainer;
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
