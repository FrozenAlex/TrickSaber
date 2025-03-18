#include "SaberTrickManager.hpp"
#include "GlobalTrickManager.hpp"
#include "config.hpp"
#include "logging.hpp"

#include "Tricks/SpinTrick.hpp"
#include "Tricks/ThrowTrick.hpp"

#include "System/Action.hpp"
#include "bsml/shared/BSML/SharedCoroutineStarter.hpp"

#include "custom-types/shared/delegate.hpp"

DEFINE_TYPE(TrickSaber, SaberTrickManager);
DEFINE_TYPE(TrickSaber, LeftSaberTrickManager);
DEFINE_TYPE(TrickSaber, RightSaberTrickManager);

namespace TrickSaber {
    void LeftSaberTrickManager::ctor() {
        DEBUG("LeftSaberTrickManager ctor");
        custom_types::InvokeBaseCtor(classof(SaberTrickManager*), this);
    }

    void RightSaberTrickManager::ctor() {
        DEBUG("RightSaberTrickManager ctor");
        custom_types::InvokeBaseCtor(classof(SaberTrickManager*), this);
    }

    void SaberTrickManager::ctor() {
        DEBUG("ctor");
        _tricks = TrickDictionary::New_ctor();
        _managerEnabled = true;
    }

    void SaberTrickManager::Inject(::Zenject::DiContainer* container, TrickSaber::MovementController* movementController, TrickSaber::InputHandling::InputManager* inputManager, TrickSaber::SaberControllerBearer* saberControllerBearer, GlobalNamespace::SaberType saberType, SaberTrickModel* saberTrickModel, GlobalNamespace::AudioTimeSyncController* audioTimeSyncController/*, TrickSaber::Tricks::TrickCustomFactory* trickFactory*/) {
        _movementController = movementController;
        _inputManager = inputManager;
        _audioTimeSyncController = audioTimeSyncController;
        _saberTrickModel = saberTrickModel;

        auto saberPackage = saberControllerBearer->get(saberType);
        _saber = saberPackage->saber;
        _vrController = saberPackage->vrController;

        //_trickFactory = trickFactory;
        _container = container;
        _pauseController = container->TryResolve<GlobalNamespace::PauseController*>();
    }

    void SaberTrickManager::Init(GlobalTrickManager* globalTrickManager) {
        _globalTrickManager = globalTrickManager;
        _inited = false;
        BSML::SharedCoroutineStarter::get_instance()->StartCoroutine(custom_types::Helpers::CoroutineHelper::New(InitAsync()));
    }

    custom_types::Helpers::Coroutine SaberTrickManager::InitAsync() {
        DEBUG("Instantiated on {}", get_gameObject()->get_name());

        if (!_vrController)
        {
            ERROR("Controller not present");
            Cleanup();
            co_return;
        }

        if (get_isLeftSaber()) _globalTrickManager->leftSaberTrickManager = reinterpret_cast<LeftSaberTrickManager*>(this);
        else _globalTrickManager->rightSaberTrickManager = reinterpret_cast<RightSaberTrickManager*>(this);

        _movementController->Init(_vrController, this);

        _inputManager->Init(_saber->get_saberType());
        _inputManager->trickActivated += { &SaberTrickManager::OnTrickActivated, this };
        _inputManager->trickDeactivated += { &SaberTrickManager::OnTrickDeactivated, this };

        auto success = false;
        co_yield reinterpret_cast<System::Collections::IEnumerator*>(BSML::SharedCoroutineStarter::get_instance()->StartCoroutine(custom_types::Helpers::CoroutineHelper::New(_saberTrickModel->Init(_saber, success))));

        if (success) INFO("Got saber model");
        else {
            ERROR("Couldn't get saber model");
            Cleanup();
            co_return;
        }

        _movementController->set_enabled(true);

        AddTrick<Tricks::SpinTrick*>();
        AddTrick<Tricks::ThrowTrick*>();

        INFO("{} tricks initialized", _tricks->get_Count());

        if (_pauseController) {
            auto delegate = custom_types::MakeDelegate<System::Action*>(
                std::function<void()>(std::bind(&SaberTrickManager::EndAllTricks, this))
            );
            _pauseController->add_didResumeEvent(delegate);
        }

        INFO("Trick Manager initialized");
        _inited = true;
        co_return;
    }
    Tricks::Trick* SaberTrickManager::GetTrick(TrickAction trickAction) {
        union {
            Il2CppObject* obj = nullptr;
            Tricks::Trick* trick;
        };

        if (_tricks->TryGetValue(trickAction, byref(obj)) && trick && trick->m_CachedPtr.m_value) {
            return trick;
        }
        return nullptr;
    }

    void SaberTrickManager::Cleanup() {
        auto values = _tricks->get_Values();
        auto iter = values->GetEnumerator();
        while (iter.MoveNext()) {
            UnityEngine::Object::DestroyImmediate(reinterpret_cast<Tricks::Trick*>(iter.get_Current()));
        }
        iter.Dispose();

        UnityEngine::Object::DestroyImmediate(_movementController);
        UnityEngine::Object::DestroyImmediate(this);
    }

    void SaberTrickManager::Update() {
        if (!_inited) return;
        _inputManager->Tick();
    }

    void SaberTrickManager::OnTrickDeactivated(TrickAction trickAction) {
        DEBUG("tricks: {}", fmt::ptr(_tricks));
        auto trick = GetTrick(trickAction);
        DEBUG("Trick: {}", fmt::ptr(trick));
        if (trick->_trickState != TrickState::Started) return;
        trick->EndTrick();
    }

    void SaberTrickManager::OnTrickActivated(TrickAction trickAction, float val) {
        if (!CanDoTrick() || IsDoingTrick()) return;
        auto trick = GetTrick(trickAction);
        trick->value = val;
        if (trick->_trickState != TrickState::Inactive) return;
        if (_audioTimeSyncController->get_state() ==
            GlobalNamespace::AudioTimeSyncController::State::Paused) return;
        trick->StartTrick();
    }

    void SaberTrickManager::OnTrickStart(TrickAction trickAction) {
        _globalTrickManager->OnTrickStarted(trickAction);
    }

    void SaberTrickManager::OnTrickEnding(TrickAction trickAction) {
        _globalTrickManager->OnTrickEndRequested(trickAction);
    }

    void SaberTrickManager::OnTrickEnd(TrickAction trickAction) {
        _globalTrickManager->OnTrickEnded(trickAction);
    }

    void SaberTrickManager::AddTrick(System::Type* type) {
        auto trick = reinterpret_cast<Tricks::Trick*>(_container->InstantiateComponent(type, get_gameObject()).ptr());
        //auto trick = _trickFactory->Create(type, get_gameObject());
        trick->Init(this, _movementController);
        trick->trickStarted += {&SaberTrickManager::OnTrickStart, this };
        trick->trickEnding += {&SaberTrickManager::OnTrickEnding, this };
        trick->trickEnded += {&SaberTrickManager::OnTrickEnd, this };
        DEBUG("Adding trick: {1}, trickaction {0}", trick->_trickAction, type->get_Name());
        _tricks->Add(trick->_trickAction, trick);
    }

    bool SaberTrickManager::IsTrickInState(TrickAction trickAction, TrickState state) {
        Tricks::Trick* trick = GetTrick(trickAction);
        if (trick) return trick->_trickState == state;
        return false;
    }

    bool SaberTrickManager::IsDoingTrick() {
        auto values = _tricks->get_Values();
        auto iter = values->GetEnumerator();
        while (iter.MoveNext()) {
            if (reinterpret_cast<Tricks::Trick*>(iter.get_Current())->_trickState != TrickState::Inactive) return true;
        }
        iter.Dispose();

        return false;
    }

    void SaberTrickManager::EndAllTricks() {
        auto values = _tricks->get_Values();
        auto iter = values->GetEnumerator();
        while (iter.MoveNext()) {
            reinterpret_cast<Tricks::Trick*>(iter.get_Current())->OnTrickEndImmediately_base();
        }
        iter.Dispose();
    }

    bool SaberTrickManager::CanDoTrick() {
        return config.trickSaberEnabled &&
                _managerEnabled &&
                _globalTrickManager->CanDoTrick();
    }

    SaberTrickManager::TrickDictionary* SaberTrickManager::get_tricks() { return _tricks; }
    SaberTrickModel* SaberTrickManager::get_saberTrickModel() { return _saberTrickModel; }
    bool SaberTrickManager::get_managerEnabled() { return _managerEnabled; }
    void SaberTrickManager::set_managerEnabled(bool value) { _managerEnabled = value; }
    bool SaberTrickManager::get_isLeftSaber() { return _saber->get_saberType() == GlobalNamespace::SaberType::SaberA; }
}
