#ifndef DEMONSTAR_ITRAINER_LISTENER_H
#define DEMONSTAR_ITRAINER_LISTENER_H

#include "TrainerTypes.h"

namespace demonstar {

class ITrainerListener
{
public:
    virtual ~ITrainerListener() = default;
    virtual void onTrainerEvent(const TrainerEvent &event) = 0;
};

} // namespace demonstar

#endif // DEMONSTAR_ITRAINER_LISTENER_H
