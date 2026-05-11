#ifndef DEMONSTAR_ITRAINER_H
#define DEMONSTAR_ITRAINER_H

#include "ITrainerListener.h"

namespace demonstar {

class ITrainer
{
public:
    virtual ~ITrainer() = default;

    virtual void setListener(ITrainerListener *listener) = 0;
    virtual void tick() = 0;
    virtual bool setCheatEnabled(CheatId cheat, bool enabled) = 0;
    virtual bool addCheatValue(CheatId cheat, int amount) = 0;
    virtual bool isCheatEnabled(CheatId cheat) const = 0;
    virtual bool isGameAvailable() const = 0;
    virtual void shutdown() = 0;
};

} // namespace demonstar

#endif // DEMONSTAR_ITRAINER_H
