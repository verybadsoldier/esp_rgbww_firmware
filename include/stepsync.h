#pragma once

#include <limits>


class StepSync {
public:
    virtual uint32_t onMasterClock(uint32_t stepsCurrent, uint32_t stepsMaster) = 0;
    virtual int getCatchupOffset() const;
    virtual uint32_t reset() = 0;

protected:
    template<typename T>
    static T calcOverflowVal(T prevValue, T curValue) {
        if (curValue < prevValue) {
            //overflow
            return std::numeric_limits<T>::max() - prevValue + curValue;
        }
        else {
            return curValue - prevValue;
        }
    }

    int _catchupOffset = 0;
};


class ClockCatchUp : public StepSync {
public:
    virtual uint32_t onMasterClock(uint32_t stepsCurrent, uint32_t stepsMaster) override;
    virtual int getCatchupOffset() const;
    virtual uint32_t reset();

private:
    uint32_t _stepsSyncMasterLast = 0;
    uint32_t _stepsSyncLast = 0;
    bool _firstMasterSync = true;
    double _steering = 1.0;
    const uint32_t _constBaseInt = RGBWW_MINTIMEDIFF_US;
};
