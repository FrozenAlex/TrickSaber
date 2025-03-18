#include "Installers/GameInstaller.hpp"
#include "logging.hpp"

#include "custom-types/shared/delegate.hpp"
#include "lapiz/shared/arrayutils.hpp"

#include "GameplayManager.hpp"
#include "GlobalTrickManager.hpp"
#include "MovementController.hpp"
#include "SaberControllerBearer.hpp"
#include "SaberTrickModel.hpp"
#include "InputHandling/InputManager.hpp"
#include "Tricks/Trick.hpp"

#include "GlobalNamespace/SaberManager.hpp"
#include "Zenject/FromBinderNonGeneric.hpp"
#include "Zenject/ConcreteBinderGeneric_1.hpp"
#include "Zenject/ConcreteIdBinderGeneric_1.hpp"
#include "Zenject/FactoryToChoiceIdBinder_3.hpp"
#include "Zenject/BindInfo.hpp"
#include "Zenject/InjectUtil.hpp"
#include "System/Func_2.hpp"

// condep from libreplay.so
#include "conditional-dependencies/shared/main.hpp"
std::optional<bool> IsInReplay() {
    static auto function = CondDeps::Find<bool>("replay", "IsInReplay");
    if (function)
        return function.value()();
    return std::nullopt;
}

DEFINE_TYPE(TrickSaber::Installers, GameInstaller);

namespace TrickSaber::Installers {
    void GameInstaller::InstallBindings() {
        DEBUG("InstallBindings");
        if (IsInReplay().value_or(false)) {
            return;
        }

        auto container = get_Container();

        container->BindInterfacesAndSelfTo<GameplayManager*>()->AsSingle();
        container->BindInterfacesAndSelfTo<GlobalTrickManager*>()->AsSingle();

        container->Bind<MovementController*>()->FromNewComponentSibling()->AsTransient();
        container->Bind<InputHandling::InputManager*>()->AsTransient();

        container->Bind<SaberControllerBearer*>()->AsSingle();

        //TODO: make SaberTrickManagers non-Monobehaviours
        BindTrickManager(GlobalNamespace::SaberType::SaberA);
        BindTrickManager(GlobalNamespace::SaberType::SaberB);

        container->Bind<SaberTrickModel*>()->AsTransient();
    }

    void GameInstaller::BindTrickManager(GlobalNamespace::SaberType saberType) {
        //DEBUG("BindTrickManager: {}", saberType);
        auto func = custom_types::MakeDelegate<System::Func_2<::Zenject::InjectContext*, UnityW<UnityEngine::GameObject>>*>(
            std::function<UnityW<UnityEngine::GameObject>(::Zenject::InjectContext*)>(
                std::bind(&GameInstaller::GetSaber, this, std::placeholders::_1, saberType)
            )
        );

        if (saberType == GlobalNamespace::SaberType::SaberA) {
            auto cachedResult = get_Container()
                ->Bind<LeftSaberTrickManager*>()
                ->FromNewComponentOn(func)
                ->AsCached();

            cachedResult->BindInfo->Arguments->Clear();
            auto pair = ::Zenject::InjectUtil::CreateTypePair<GlobalNamespace::SaberType>(saberType);
            cachedResult->BindInfo->Arguments->Add(pair);
        } else {
            auto cachedResult = get_Container()
                ->Bind<RightSaberTrickManager*>()
                ->FromNewComponentOn(func)
                ->AsCached();

            cachedResult->BindInfo->Arguments->Clear();
            auto pair = ::Zenject::InjectUtil::CreateTypePair<GlobalNamespace::SaberType>(saberType);
            cachedResult->BindInfo->Arguments->Add(pair);
        }
    }

    UnityEngine::GameObject* GameInstaller::GetSaber(::Zenject::InjectContext* ctx, GlobalNamespace::SaberType saberType) {
        DEBUG("GetSaber");
        auto saberManager = ctx->get_Container()->TryResolve<GlobalNamespace::SaberManager*>();

        if (!saberManager)
        {
            ERROR("Couldn't resolve SaberManager");
            return nullptr;
        }

        return saberType == GlobalNamespace::SaberType::SaberA
            ? saberManager->leftSaber->get_gameObject()
            : saberManager->rightSaber->get_gameObject();
    }
}
