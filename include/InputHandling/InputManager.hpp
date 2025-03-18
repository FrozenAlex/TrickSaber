#pragma once

#include "custom-types/shared/macros.hpp"
#include "lapiz/shared/macros.hpp"
#include "TrickInputHandler.hpp"
#include "GlobalNamespace/SaberType.hpp"
#include "System/Collections/Generic/HashSet_1.hpp"

DECLARE_CLASS_CODEGEN(TrickSaber::InputHandling, InputManager, Il2CppObject) {
    TrickInputHandler _trickInputHandler;

    DECLARE_CTOR(ctor);
    DECLARE_INSTANCE_METHOD(void, Init, GlobalNamespace::SaberType saberType);
    DECLARE_INSTANCE_METHOD(void, Tick);
    public:
        UnorderedEventCallback<TrickSaber::TrickAction, float> trickActivated;
        UnorderedEventCallback<TrickSaber::TrickAction> trickDeactivated;
    private:
        static bool CheckHandlersDown(const TrickInputHandler::TrickHandlerVector& handlers, float& val);
        static bool CheckHandlersUp(const TrickInputHandler::TrickHandlerVector& handlers);
};
