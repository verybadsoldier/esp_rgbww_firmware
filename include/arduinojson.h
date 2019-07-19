#pragma once

#include <RGBWWLed/RGBWWLed.h>


namespace Json
{
    // gets a bool value from a variant which may also be an int or a String
    // this was introduced since FHEM interprets all user input as strings and therefore always
    // sends string values
    inline bool getBoolTolerant(JsonVariant var, bool& value) {
        if (var.isNull())
            return false;

        if (var.is<int>()) {
            value = var.as<int>() > 0;
        }
        else if (var.is<String>()) {
            String strVal = var.as<String>();
            strVal.toLowerCase();
            value = strVal == "1" || strVal == "true";
        }
        else {
            value = var.as<bool>();
        }
        return true;
    }

    inline bool getBoolTolerantChanged(JsonVariant var, bool& value) {
        bool newVal;
        if (!getBoolTolerant(var, newVal))
            return false;

        if (newVal != value)
            return false;

        value = newVal;
        return true;
    }
}
